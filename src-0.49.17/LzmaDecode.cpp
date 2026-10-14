#include "stdafx.h"
#include "LzmaDecode.h"
#include <stdlib.h>
#include <string.h>

// ---- ISzAlloc for LzmaDec (malloc backed; no file I/O, no globals) ----
static void *lzmaDecAlloc( ISzAllocPtr p, size_t size )
	{ (void)p; return malloc(size); }
static void lzmaDecFree( ISzAllocPtr p, void *addr )
	{ (void)p; free(addr); }
static ISzAlloc g_lzmaDecAlloc = { lzmaDecAlloc, lzmaDecFree };

bool LzmaDecodeStream(const BYTE props[LZMA_PROPS_SIZE],
                      LzmaFillFn fill, void *ictx,
                      LzmaWriteFn write, void *octx,
                      DWORD usz, bool exact)
{
	CLzmaDec st;
	CLzmaProps pp;
	ELzmaStatus stat;
	BYTE *inbuf;
	DWORD inlen = 0;
	DWORD nout = 0;
	SizeT limit, slen, dicPosPrev;
	int got;
	SRes r;
	bool ok = false;

	if( usz == 0 && exact )
		return true;

	if( LzmaProps_Decode( &pp, props, LZMA_PROPS_SIZE ) != SZ_OK )
		return false;
	if( pp.dicSize < 0x1000 )
		pp.dicSize = 0x1000;
	if( pp.dicSize > 0x4000000 )
		return false;

	LzmaDec_Construct( &st );
	r = LzmaDec_Allocate( &st, props, LZMA_PROPS_SIZE, &g_lzmaDecAlloc );
	if( r != SZ_OK )
		return false;
	LzmaDec_Init( &st );

	inbuf = new BYTE[XACR_BUFSIZE];
	stat = LZMA_STATUS_NOT_SPECIFIED;
	for(;;)
		{
		if( st.dicPos == st.dicBufSize )
			{
			write( octx, st.dic, (int)st.dicBufSize );
			nout += (DWORD)st.dicBufSize;
			st.dicPos = 0;
			if( exact && nout >= usz )
				{ ok = (nout == usz); break; }
			}
		if( exact && nout + (DWORD)st.dicPos >= usz )
			{ ok = (nout + (DWORD)st.dicPos == usz); break; }

		if( inlen == 0 )
			{
			got = fill( ictx, inbuf, XACR_BUFSIZE );
			if( got > 0 )
				inlen = (DWORD)got;
			else if( got < 0 )
				break;
			}

		if( exact )
			{
			SizeT room = st.dicBufSize - st.dicPos;
			SizeT remain = (SizeT)(usz - nout - (DWORD)st.dicPos);
			limit = st.dicPos + (remain < room ? remain : room);
			}
		else
			limit = st.dicBufSize;

		dicPosPrev = st.dicPos;
		slen = inlen;
		r = LzmaDec_DecodeToDic( &st, limit, inbuf, &slen, LZMA_FINISH_ANY, &stat );
		if( slen < inlen )
			{ memmove( inbuf, inbuf + slen, inlen - (DWORD)slen ); inlen -= (DWORD)slen; }
		else
			inlen = 0; // full consumption must reset, or refill stalls
		if( r != SZ_OK )
			break;
		if( stat == LZMA_STATUS_FINISHED_WITH_MARK )
			{ ok = !exact || (nout + (DWORD)st.dicPos == usz); break; }
		if( inlen == 0 )
			{
			int probe = fill( ictx, inbuf, 1 );
			if( probe <= 0 )
				break;
			inlen = (DWORD)probe;
			}
		if( slen == 0 && st.dicPos == dicPosPrev )
			break;
		}

	if( st.dicPos > 0 )
		{ write( octx, st.dic, (int)st.dicPos ); nout += (DWORD)st.dicPos; }
	if( exact && !ok )
		ok = (nout == usz) && (stat == LZMA_STATUS_FINISHED_WITH_MARK || r == SZ_OK);
	delete [] inbuf;
	LzmaDec_Free( &st, &g_lzmaDecAlloc );
	return ok;
}
