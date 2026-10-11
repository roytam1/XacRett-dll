#include "stdafx.h"
#include "OpenBoo.h"

bool COpenBoo::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	kiutil::getline(lpSubInfo->szFileName,MAX_PATH,fp);
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(fp));
	//lstrcpy(lpSubInfo->szMode,"");
	return true;
}

bool COpenBoo::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}
