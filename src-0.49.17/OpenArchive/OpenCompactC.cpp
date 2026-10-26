#include "stdafx.h"
#include "OpenCompactC.h"

bool COpenCompactC::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	char outname[256];
	kiutil::getOriginalName( outname,COpenCompactC::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenCompactC::szArcFileName),"C") ? NULL : "uncompact" );
	lstrcpy(lpSubInfo->szFileName,outname);
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(fp));
	lstrcpy(lpSubInfo->szMode,"LZW");
	return true;
}

bool COpenCompactC::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}