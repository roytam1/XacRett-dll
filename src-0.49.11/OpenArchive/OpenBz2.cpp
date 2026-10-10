#include "stdafx.h"
#include "OpenBz2.h"

bool COpenBz2::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	/*LongFileName取得
	WIN32_FIND_DATA find;
	HANDLE h = FindFirstFile( COpenBz2::szArcFileName,&find );
	if( h==INVALID_HANDLE_VALUE )
	{
		return false;
	}
	FindClose( h );
	lstrcpy(lpSubInfo->szFileName,find.cFileName );*/
	char outname[256];
	kiutil::getOriginalName( outname,COpenBz2::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenBz2::szArcFileName),"bz2") ? NULL : "unbzip2" );
	lstrcpy(lpSubInfo->szFileName,outname );

	lpSubInfo->dwCRC = 0;
	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(COpenBz2::fp))-2;
	//元のバイト数の計算
	lpSubInfo->dwOriginalSize = 0;
	lstrcpy(lpSubInfo->szAttribute ,"A---W");//何か謎^^;
	lpSubInfo->wDate = 0;
	lpSubInfo->wRatio = 0;
	lpSubInfo->wTime = 0;
	//lstrcpy(lpSubInfo->szMode,"");

	return true;
}

bool COpenBz2::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}