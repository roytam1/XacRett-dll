
#include "stdafx.h"
#include "OpenaPLib.h"

bool COpenaPLib::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	APKHEAD hdr;
	fread(&hdr,sizeof(APKHEAD),1,fp);
	//一応チェック
	if(!(hdr.sig[0]==0x41 && 
				hdr.sig[1]==0x50 && hdr.sig[2]==0x33 && 
				hdr.sig[3]==0x32))
	{
		return false;
	}

	// LongFileName取得
	WIN32_FIND_DATA find;
	HANDLE h = FindFirstFile( COpenaPLib::szArcFileName,&find );
	if( h==INVALID_HANDLE_VALUE )
	{
		return false;
	}
	FindClose( h );
	kiutil::getOriginalName( lpSubInfo->szFileName,find.cFileName,
		!strcmp(kiutil::pathExt(find.cFileName),"apk") ? NULL : "unapk" );
	lpSubInfo->dwCRC = hdr.crc;

	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(fp));
	//元のバイト数の計算
	lpSubInfo->dwOriginalSize = hdr.o_size;//GetSize(COpenaPLib::szArcFileName);
	lstrcpy(lpSubInfo->szAttribute ,"A--W-");//何か謎^^;
	
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	//lstrcpy(lpSubInfo->szMode,"");
	FILETIME lt;
	if(!::FileTimeToLocalFileTime(&find.ftLastWriteTime,&lt))				 return false;
	if(!::FileTimeToDosDateTime(&lt,&lpSubInfo->wDate,&lpSubInfo->wTime))return false;
	return true;
}
bool COpenaPLib::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}
