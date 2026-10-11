
#include "stdafx.h"
#include "OpenJc1.h"
#include "../Jc1Tool.h"

#ifndef _DEBUG
bool COpenJc1::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// LongFileName取得
	WIN32_FIND_DATA find;
	HANDLE h = FindFirstFile( COpenJc1::szArcFileName,&find );
	if( h==INVALID_HANDLE_VALUE )
	{
		return false;
	}
	FindClose( h );
	//lstrcpy(lpSubInfo->szFileName,find.cFileName );
	kiutil::getOriginalName( lpSubInfo->szFileName,find.cFileName,
		!strcmp(kiutil::pathExt(find.cFileName),"jcalg1") ? NULL
			: !strcmp(kiutil::pathExt(find.cFileName),"jc") ? NULL
			: !strcmp(kiutil::pathExt(find.cFileName),"jc1") ? NULL: "unjcalg1" );
	lpSubInfo->dwCRC = 0;

	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(COpenJc1::fp));
	//元のバイト数の計算
	lpSubInfo->dwOriginalSize = GetSize(COpenJc1::szArcFileName);
	lstrcpy(lpSubInfo->szAttribute ,"A--W-");//何か謎^^;
	
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	lstrcpy(lpSubInfo->szMode,"JCALG1");
	FILETIME lt;
	if(!::FileTimeToLocalFileTime(&find.ftLastWriteTime,&lt))				 return false;
	if(!::FileTimeToDosDateTime(&lt,&lpSubInfo->wDate,&lpSubInfo->wTime))return false;
	return true;
}
bool COpenJc1::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}
DWORD COpenJc1::GetSize(LPCSTR file){
	MemmapFile mf;
	if( !mf.Open(file) )
		return 0;
	const BYTE* beg = mf.base();
	unsigned int nFilesize;
	
	nFilesize=JCALG1_GetUncompressedSizeOfCompressedBlock((void *)beg);
	mf.Close();
	return nFilesize;
}
#endif