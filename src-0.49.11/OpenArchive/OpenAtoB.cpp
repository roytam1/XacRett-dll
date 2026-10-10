#include "stdafx.h"
#include "OpenAtoB.h"

bool COpenAtoB::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	kiutil::getOriginalName( lpSubInfo->szFileName,COpenAtoB::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenAtoB::szArcFileName),"b2a") ? NULL : "a2b" );
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(fp));
	//lstrcpy(lpSubInfo->szMode,"");
	return true;
}

bool COpenAtoB::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}
