#include "stdafx.h"
#include "OpenJam.h"

bool COpenJam::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// o—Íæ
	char outname[MAX_PATH];
	kiutil::getOriginalName( outname, COpenJam::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenJam::szArcFileName),"jam") ? NULL : "unjam" );

	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(COpenJam::fp));
	lstrcpy(lpSubInfo->szFileName,outname);
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	lstrcpy(lpSubInfo->szMode,"JAM");
	//lstrcpy(lpSubInfo->szMode,"");
	return true;
}

bool COpenJam::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}