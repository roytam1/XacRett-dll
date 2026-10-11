#include "stdafx.h"
#include "OpenBel.h"

bool COpenBel::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// LongFileName取得
	WIN32_FIND_DATA find;
	HANDLE h = FindFirstFile( COpenBel::szArcFileName,&find );
	if( h==INVALID_HANDLE_VALUE )
	{
		return false;
	}
	FindClose( h );
	lstrcpy(lpSubInfo->szFileName,find.cFileName );
	lpSubInfo->dwCRC = 0;

	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(COpenBel::fp));
	//元のバイト数の計算
	lpSubInfo->dwOriginalSize = COpenBel::common_buf[0]+COpenBel::common_buf[1]*256+COpenBel::common_buf[2]*65536;
	lstrcpy(lpSubInfo->szAttribute ,"A--W-");//何か謎^^;
	lpSubInfo->wDate = 0;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	lstrcpy(lpSubInfo->szMode,"BEL");
	lpSubInfo->wTime = 0;

	return true;
}

bool COpenBel::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}