#include "stdafx.h"
#include "OpenCompactC.h"

bool COpenCompactC::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	char outname[256];
	kiutil::getOriginalName( outname,COpenCompactC::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenCompactC::szArcFileName),"z") ? NULL : "unpack" );
	lstrcpy(lpSubInfo->szFileName,outname);
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(fp));
	lstrcpy(lpSubInfo->szMode,"LZW");
	return false;
}

bool COpenCompactC::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}