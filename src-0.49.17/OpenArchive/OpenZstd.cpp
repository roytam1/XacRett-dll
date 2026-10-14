#include "stdafx.h"
#include "OpenZstd.h"
extern "C" {
#include "zstd/zstd.h"
}

bool COpenZstd::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	BYTE magic[4];
	char outname[MAX_PATH];
	long fsize;
	BYTE *buf;
	size_t pos;
	unsigned __int64 usum;
	bool unknown;

	fseek( COpenZstd::fp, 0, SEEK_END );
	fsize = ftell( COpenZstd::fp );
	if( fsize < 6 )
		return false;
	fseek( COpenZstd::fp, 0, SEEK_SET );
	if( 4 != fread(magic,1,4,COpenZstd::fp) )
		return false;
	if( magic[0]!=0x28 || magic[1]!=0xB5 || magic[2]!=0x2F || magic[3]!=0xFD )
		return false;
	// exact sizes by walking frames (no decode); any unknown size -> 0
	buf = new BYTE[fsize];
	fseek( COpenZstd::fp, 0, SEEK_SET );
	if( (size_t)fsize != fread(buf,1,fsize,COpenZstd::fp) )
		{ delete [] buf; return false; }
	pos = 0;
	usum = 0;
	unknown = false;
	while( pos < (size_t)fsize )
		{
		unsigned __int64 fcs;
		size_t csz;
		fcs = ZSTD_getFrameContentSize( buf+pos, (size_t)fsize-pos );
		if( fcs == ZSTD_CONTENTSIZE_ERROR )
			{ delete [] buf; return false; }
		csz = ZSTD_findFrameCompressedSize( buf+pos, (size_t)fsize-pos );
		if( ZSTD_isError( csz ) || csz==0 )
			{ delete [] buf; return false; }
		if( fcs == ZSTD_CONTENTSIZE_UNKNOWN )
			unknown = true;
		else
			usum += fcs;
		pos += csz;
		}
	delete [] buf;
	if( pos != (size_t)fsize )
		return false;
	kiutil::getOriginalName( outname,COpenZstd::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenZstd::szArcFileName),"zst") ? NULL : "unzst" );
	lstrcpy(lpSubInfo->szFileName,outname);
	lpSubInfo->dwOriginalSize = (unknown || usum > 0xFFFFFFFF) ? 0 : (DWORD)usum;
	lpSubInfo->dwCompressedSize = (DWORD)fsize;
	lpSubInfo->dwCRC = 0;
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lstrcpy(lpSubInfo->szMode,"zstd");
	lpSubInfo->wDate = 0;
	lpSubInfo->wTime = 0;
	return true;
}

bool COpenZstd::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	(void)lpSubInfo;
	return false;
}
