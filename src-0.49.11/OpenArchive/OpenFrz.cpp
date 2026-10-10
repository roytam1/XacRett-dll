#include "stdafx.h"
#include "OpenFrz.h"

bool COpenFrz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	char oriname[MAX_PATH];
	const char* ext = kiutil::pathExt(COpenFrz::szArcFileName);
	kiutil::getOriginalName( oriname,COpenFrz::szArcFileName,
		0==stricmp(ext,"f")||0==stricmp(ext,"xf")||0==stricmp(ext,"frz")
		? NULL : "unfreeze" );

	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(COpenFrz::fp));
	lstrcpy(lpSubInfo->szFileName,oriname);
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	//lstrcpy(lpSubInfo->szMode,"");

	return true;
}

bool COpenFrz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}