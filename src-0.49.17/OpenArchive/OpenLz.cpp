#include "stdafx.h"
#include "OpenLz.h"
#include "LzDecode.h"


// A .lz file holds one logical member regardless of how many lzip members it
// is built from, so listing reports a single entry whose size is the sum of
// the member data sizes.  Those come from the trailer index, so listing never
// has to decompress anything.  An inconsistent index (trailing data, a
// corrupt file) reports size 0 rather than a guess.
bool COpenLz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	BYTE        hdr[6];
	char        outname[MAX_PATH];
	LzU64   total = 0;
	DWORD       nmem  = 0;
	DWORD       fsize = 0;
	BOOL        ok;

	if( 6 != fread(hdr,1,6,COpenLz::fp) )
		return false;
	if( memcmp(hdr,"LZIP",4)!=0 || hdr[4]!=1 )
		return false;
	fclose( COpenLz::fp );

	{
		FILE *fp = fopen( COpenLz::szArcFileName, "rb" );
		if( !fp )
			return false;
		fsize = (DWORD)filelength( fileno(fp) );
		fclose( fp );
		}

	kiutil::getOriginalName( outname,COpenLz::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenLz::szArcFileName),"lz") ? NULL : "unlz" );
	lstrcpy( lpSubInfo->szFileName, outname );

	ok = LzScanFile( COpenLz::szArcFileName, &total, &nmem );
	lpSubInfo->dwOriginalSize =
		(ok && total > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD)total;
	lpSubInfo->dwCompressedSize = fsize;
	// lzip's CRC is per member and only checkable after decompressing, so
	// there is no single CRC to show up front.
	lpSubInfo->dwCRC = 0;
	lstrcpy( lpSubInfo->szAttribute, "A---W" );
	lstrcpy( lpSubInfo->szMode, "lz" );
	lpSubInfo->wDate = 0;
	lpSubInfo->wTime = 0;
	(void)nmem;
	return true;
}

bool COpenLz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	(void)lpSubInfo;
	return false;
}