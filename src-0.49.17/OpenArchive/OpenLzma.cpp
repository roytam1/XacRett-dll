#include "stdafx.h"
#include "OpenLzma.h"

bool COpenLzma::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	BYTE hdr[13];
	DWORD lo, hi;
	char outname[MAX_PATH];

	if( 13 != fread(hdr,1,13,COpenLzma::fp) )
		return false;
	if( hdr[0] > 224 )
		return false;
	lo = (DWORD)(hdr[5]|(hdr[6]<<8)|(hdr[7]<<16)|(hdr[8]<<24));
	hi = (DWORD)(hdr[9]|(hdr[10]<<8)|(hdr[11]<<16)|(hdr[12]<<24));
	kiutil::getOriginalName( outname,COpenLzma::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenLzma::szArcFileName),"lzma") ? NULL : "unlzma" );
	lstrcpy(lpSubInfo->szFileName,outname);
	if( hi==0xFFFFFFFF && lo==0xFFFFFFFF )
		lpSubInfo->dwOriginalSize = 0;
	else if( hi != 0 )
		lpSubInfo->dwOriginalSize = 0xFFFFFFFF;
	else
		lpSubInfo->dwOriginalSize = lo;
	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(COpenLzma::fp)) - 13;
	lpSubInfo->dwCRC = 0;
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lstrcpy(lpSubInfo->szMode,"lzma");
	lpSubInfo->wDate = 0;
	lpSubInfo->wTime = 0;
	return true;
}

bool COpenLzma::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	(void)lpSubInfo;
	return false;
}
