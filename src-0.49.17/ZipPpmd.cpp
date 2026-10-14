#include "stdafx.h"
#include "ZipTool.h"
#include <stdlib.h>
extern "C" {
#include "ppmd8/ppmd8.h"
}

// ---- ISzAlloc for Ppmd8 (ppmd.h Mori own type; malloc backed) ----
static void *ppmdAlloc( ISzAllocPtr p, size_t size )
	{ (void)p; return malloc(size); }
static void ppmdFree( ISzAllocPtr p, void *addr )
	{ (void)p; free(addr); }
static ISzAlloc g_ppmdAlloc = { ppmdAlloc, ppmdFree };

// ---- streaming byte reader over CZipTool::zipread ----
struct PpmdIn
{
	IByteIn vt;
	CZipTool *tool;
	DWORD left;
	bool err;
};

Byte CZipTool::ppmdRead( IByteInPtr p )
{
	PpmdIn *io = (PpmdIn*)(void*)p;
	BYTE b;
	if( io->left==0 )
		{ io->err = true; return 0; }
	if( 1 != io->tool->zipread( &b, 1 ) )
		{ io->err = true; return 0; }
	io->left--;
	return b;
}

////////////////////////////////////////////////////////////////
// PPMd (ppmd8.c, ZIP method 98: PPMd Variant I revision 1)
////////////////////////////////////////////////////////////////

void CZipTool::UnPpmd( DWORD usz, DWORD csz )
{
	CPpmd8 model;
	PpmdIn input;
	BYTE props[2];
	BYTE *ob;
	DWORD obn = 0;
	DWORD total = 0;
	WORD pw;
	unsigned order, mem, restore;

	if( csz < 2 )
		return;
	if( 2 != zipread( props, 2 ) )
		return;
	pw = (WORD)(props[0] | (props[1] << 8));
	order = (pw & 15) + 1;
	mem = ((pw >> 4) & 255) + 1;
	restore = pw >> 12;
	if( order < PPMD8_MIN_ORDER || order > PPMD8_MAX_ORDER ||
	    mem < 1 || mem > 64 ||
	    restore > PPMD8_RESTORE_METHOD_FREEZE )
		return;

	Ppmd8_Construct( &model );
	Ppmd8_SetLegacyFreeze( &model, restore==PPMD8_RESTORE_METHOD_FREEZE );
	if( !Ppmd8_Alloc( &model, (UInt32)mem << 20, &g_ppmdAlloc ) )
		return;
	input.vt.Read = ppmdRead;
	input.tool = this;
	input.left = csz - 2;
	input.err = false;
	model.Stream.In = &input.vt;
	if( !Ppmd8_Init_RangeDec( &model ) )
		{ Ppmd8_Free( &model, &g_ppmdAlloc ); return; }
	Ppmd8_Init( &model, order, restore );

	ob = CTool::common_buf;
	while( total < usz )
		{
		int sym = Ppmd8_DecodeSymbol( &model );
		if( sym < 0 || input.err )
			break;
		ob[obn++] = (BYTE)sym;
		total++;
		if( obn >= (DWORD)XACR_BUFSIZE )
			{ zipwrite( ob, XACR_BUFSIZE ); obn = 0; }
		}
	if( total==usz && !input.err )
		{
		int end = Ppmd8_DecodeSymbol( &model );
		if( end!=PPMD8_SYM_END || input.err || input.left!=0 ||
		    !Ppmd8_RangeDec_IsFinishedOK( &model ) )
			{
			// corrupt tail with a full-size file: truncate short so the
			// size check in Extract() fails loudly instead of passing.
			if( obn != 0 )
				{ obn--; }
			}
		}
	if( obn != 0 )
		zipwrite( ob, obn );
	Ppmd8_Free( &model, &g_ppmdAlloc );
}
