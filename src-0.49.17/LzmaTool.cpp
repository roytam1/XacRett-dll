#include "stdafx.h"
#include "LzmaTool.h"
#include "LzmaDecode.h"

struct LzmaFileIO { FILE *f; };
static int lzmaFileFill( void *ctx, BYTE *buf, int want )
{
	int n = fread( buf, 1, want, (FILE*)ctx );
	return n > 0 ? n : 0;
}
static void lzmaFileWrite( void *ctx, const BYTE *buf, int len )
{
	fwrite( buf, 1, len, (FILE*)ctx );
}

// LZMA Alone: props(1) + dict LE32 + size LE64 (FF..FF = unknown).
bool LzmaDecodeFile( const char* src, const char* dst )
{
	BYTE hdr[13];
	BYTE props[5];
	DWORD lo, hi;
	DWORD usz;
	bool exact;
	bool ok;
	FILE *fin, *fout;

	fin = fopen( src, "rb" );
	if( !fin )
		return false;
	if( 13 != fread( hdr, 1, 13, fin ) )
		{ fclose( fin ); return false; }
	memcpy( props, hdr, 5 );
	lo = (DWORD)(hdr[5]|(hdr[6]<<8)|(hdr[7]<<16)|(hdr[8]<<24));
	hi = (DWORD)(hdr[9]|(hdr[10]<<8)|(hdr[11]<<16)|(hdr[12]<<24));
	if( hi==0xFFFFFFFF && lo==0xFFFFFFFF )
		{ exact = false; usz = 0; }
	else
		{
		if( hi != 0 )
			{ fclose( fin ); return false; }
		exact = true; usz = lo;
		}
	fout = fopen( dst, "wb" );
	if( !fout )
		{ fclose( fin ); return false; }
	ok = LzmaDecodeStream( props, lzmaFileFill, fin, lzmaFileWrite, fout, usz, exact );
	fclose( fin );
	fclose( fout );
	return ok;
}

bool CLzmaTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	(void)fname;
	if( siz < 13 )
		return false;
	// no magic in LZMA Alone: only the props byte is checkable
	// ((pb*5+lp)*9+lc <= 224).
	return hdr[0] <= 224;
}

bool CLzmaTool::Extract( const char* fname,const char* ddir )
{
	char outname[MAX_PATH];
	(void)ddir;
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"lzma") ? NULL : "unlzma" );
	if (m_Endflg) return true;
	if (!CheckStr(outname))return true;
	char *name = kiutil::pathMake( outname );
	if( !LzmaDecodeFile( fname, name ) )
		return false;
	return true;
}
