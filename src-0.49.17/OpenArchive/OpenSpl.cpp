#include "stdafx.h"
#include "OpenSpl.h"

bool COpenSpl::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenSpl::common_buf;

	// read header
	if( 7!=fread(buf,1,7,COpenSpl::fp) )
		return false;
	if( buf[0]!=0x93 || buf[1]!=0xb9 || buf[2]!=0x06 )
		return false;
	// orig-size but not use
	DWORD usize = (buf[6]<<24)|(buf[5]<<16)|(buf[4]<<8)|(buf[3]);

	// get original filename
	int c,i;
	for( i=0; i!=33; i++ )
	{
		c = getc(COpenSpl::fp);
		if( (buf[i]=c)=='\0' )
			 break;
	}
	if( i==33 )
		return false;

	lpSubInfo->dwCompressedSize=(DWORD)filelength(fileno(COpenSpl::fp));
	lpSubInfo->dwOriginalSize=usize;
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	//lstrcpy(lpSubInfo->szMode,"");
	lstrcpy(lpSubInfo->szFileName,(char*)buf);

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	return true;
}

bool COpenSpl::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}