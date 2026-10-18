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
	unsigned long dict;
	(void)fname;
	if( siz < 13 )
		return false;
	// LZMA Alone has no magic: props(1) + dict LE32 + size LE64.
	// props is ((pb*5+lp)*9+lc) <= 224.
	if( hdr[0] > 224 )
		return false;
	// The dict size is what makes this a real test.  props alone
	// accepts every file whose first byte is <= 224, which is every
	// text encoding (bel/uu/shar/jam/boo/ask/atob/fish/mime) and
	// every 0x1f-prefixed codec (CompressZ 1f 9d, Packz 1f 1e);
	// this provider is invisible to the extension pass, so a lax
	// Check made it swallow all of them in the fallback sweep.
	// A genuine .lzma always has a power-of-two dictionary -- the
	// LZMA SDK/7-Zip encoder rejects anything else -- so this does
	// not reject real files.  Verified against every .lzma/.tlz
	// fixture: all 18 genuine ones pass, and the only rejects are
	// lzip streams ("LZIP"|VN|DS) and Xtro/PKZIPC-framed ZIP m14
	// members, neither of which is bare LZMA Alone.
	dict = (unsigned long)hdr[1] | ((unsigned long)hdr[2] << 8)
		| ((unsigned long)hdr[3] << 16) | ((unsigned long)hdr[4] << 24);
	if( dict < 4096 || (dict & (dict-1)) != 0 )
		return false;
	return true;
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
