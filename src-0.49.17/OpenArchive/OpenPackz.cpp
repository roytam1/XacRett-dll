#include "stdafx.h"
#include "OpenPackz.h"

bool COpenPackz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	char outname[256];
	kiutil::getOriginalName( outname,COpenPackz::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenPackz::szArcFileName),"z") ? NULL : "unpack" );
	lstrcpy(lpSubInfo->szFileName,outname);
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(fp));
	//lstrcpy(lpSubInfo->szMode,"");
	return true;
}

bool COpenPackz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}