#include "stdafx.h"
#include "ZstdTool.h"
extern "C" {
#include "zstd/zstd.h"
}

#define ZSTDINBLK (1<<17)
#define ZSTDOUTBLK (1<<17)

bool ZstdDecodeFile( const char* src, const char* dst )
{
	FILE *fin, *fout;
	ZSTD_DCtx *ctx;
	BYTE *ib, *ob;
	ZSTD_inBuffer zin;
	ZSTD_outBuffer zout;
	size_t r, n;
	size_t consumed, fsize;
	long fs;
	bool frameDone, anyFrame;

	fin = fopen( src, "rb" );
	if( !fin )
		return false;
	fseek( fin, 0, SEEK_END ); fs = ftell( fin ); fseek( fin, 0, SEEK_SET );
	if( fs <= 0 )
		{ fclose( fin ); return false; }
	fsize = (size_t)fs;
	fout = fopen( dst, "wb" );
	if( !fout )
		{ fclose( fin ); return false; }
	ctx = ZSTD_createDCtx();
	ib = new BYTE[ZSTDINBLK];
	ob = new BYTE[ZSTDOUTBLK];
	if( ZSTD_isError( ZSTD_initDStream( ctx ) ) )
		{ goto fail; }
	consumed = 0;
	frameDone = false;
	anyFrame = false;
	r = 1;
	for(;;)
		{
		n = fread( ib, 1, ZSTDINBLK, fin );
		if( n==0 )
			break;
		zin.src = ib; zin.size = n; zin.pos = 0;
		while( zin.pos < zin.size )
			{
			zout.dst = ob; zout.size = ZSTDOUTBLK; zout.pos = 0;
			r = ZSTD_decompressStream( ctx, &zout, &zin );
			if( ZSTD_isError( r ) )
				{ goto fail; }
			if( zout.pos != 0 )
				fwrite( ob, 1, zout.pos, fout );
			if( r==0 )
				{ frameDone = true; anyFrame = true; }
			else
				frameDone = false;
			}
		consumed += zin.pos;
		}
	// strict tail: every input byte consumed, last frame complete
	if( consumed != fsize || !frameDone || !anyFrame )
		{ goto fail; }
	delete [] ib;
	delete [] ob;
	ZSTD_freeDCtx( ctx );
	fclose( fin );
	fclose( fout );
	return true;
fail:
	delete [] ib;
	delete [] ob;
	ZSTD_freeDCtx( ctx );
	fclose( fin );
	fclose( fout );
	return false;
}

bool CZstdTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	(void)fname;
	if( siz < 6 )
		return false;
	// frame magic is the LE32 number 0xFD2FB528: bytes 28 B5 2F FD
	return hdr[0]==0x28 && hdr[1]==0xB5 && hdr[2]==0x2F && hdr[3]==0xFD;
}

bool CZstdTool::Extract( const char* fname,const char* ddir )
{
	char outname[MAX_PATH];
	(void)ddir;
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"zst") ? NULL : "unzst" );
	if (m_Endflg) return true;
	if (!CheckStr(outname))return true;
	char *name = kiutil::pathMake( outname );
	if( !ZstdDecodeFile( fname, name ) )
		return false;
	return true;
}
