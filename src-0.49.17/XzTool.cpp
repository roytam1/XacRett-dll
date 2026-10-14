#include "stdafx.h"
#include "XzTool.h"
#include "XzDecode.h"

static int xzFileFill( void *ctx, BYTE *buf, int want )
{
	int n = fread( buf, 1, want, (FILE*)ctx );
	return n > 0 ? n : 0;
}
static void xzFileWrite( void *ctx, const BYTE *buf, int len )
{
	fwrite( buf, 1, len, (FILE*)ctx );
}

bool XzDecodeFile( const char* src, const char* dst )
{
	FILE *fin, *fout;
	bool ok;

	fin = fopen( src, "rb" );
	if( !fin )
		return false;
	fout = fopen( dst, "wb" );
	if( !fout )
		{ fclose( fin ); return false; }
	ok = XzDecodeStream( xzFileFill, fin, xzFileWrite, fout, 0, false );
	fclose( fin );
	fclose( fout );
	return ok;
}

bool CXzTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	(void)fname;
	if( siz < 12 )
		return false;
	return hdr[0]==0xFD && hdr[1]==0x37 && hdr[2]==0x7A &&
		hdr[3]==0x58 && hdr[4]==0x5A && hdr[5]==0x00;
}

bool CXzTool::Extract( const char* fname,const char* ddir )
{
	char outname[MAX_PATH];
	(void)ddir;
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"xz") ? NULL : "unxz" );
	if (m_Endflg) return true;
	if (!CheckStr(outname))return true;
	char *name = kiutil::pathMake( outname );
	if( !XzDecodeFile( fname, name ) )
		return false;
	return true;
}
