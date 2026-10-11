#include "stdafx.h"
#include "OpenCompressZ.h"

bool COpenCompressZ::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	char outname[256];
	kiutil::getOriginalName( outname,COpenCompressZ::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenCompressZ::szArcFileName),"z") ? NULL : "unpack" );
	lstrcpy(lpSubInfo->szFileName,outname);
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(fp));
	lstrcpy(lpSubInfo->szMode,"LZW");
	return true;
}

bool COpenCompressZ::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}