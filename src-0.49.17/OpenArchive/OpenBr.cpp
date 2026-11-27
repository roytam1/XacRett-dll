#include "stdafx.h"
#include "OpenBr.h"
#include "..\BrotliDecode.h"

bool COpenBr::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	char outname[MAX_PATH];
	long fsize;

	fseek( COpenBr::fp, 0, SEEK_END );
	fsize = ftell( COpenBr::fp );
	if( fsize==0 )
		return false;
	// Brotli carries no size field and no magic; a bounded decode is the
	// only honest check, and it never reads the whole member.
	if( !BrotliCheckFile( COpenBr::szArcFileName, (unsigned long)fsize ) )
		return false;
	kiutil::getOriginalName( outname,COpenBr::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenBr::szArcFileName),"br") ? NULL : "unbr" );
	lstrcpy(lpSubInfo->szFileName,outname);
	lpSubInfo->dwOriginalSize = 0;		// not stored in the format
	lpSubInfo->dwCompressedSize = (DWORD)fsize;
	lpSubInfo->dwCRC = 0;
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lstrcpy(lpSubInfo->szMode,"brotli");
	lpSubInfo->wDate = 0;
	lpSubInfo->wTime = 0;
	return true;
}

bool COpenBr::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	(void)lpSubInfo;
	return false;
}
