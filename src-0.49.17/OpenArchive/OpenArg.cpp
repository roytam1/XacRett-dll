#include "stdafx.h"
#include "OpenArg.h"

bool COpenArg::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}

bool COpenArg::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}

bool COpenArg::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenArg::common_buf;

	if( fgetc(COpenArg::fp)!=0 )
		return false;
	if( 25!=fread( buf,1,25,COpenArg::fp ) )
		return false;
	// blank entry name is invalid (e.g. CP/M LBR directory padding)
	{ int i; for(i=0;i<12;i++) if(buf[i]!=0 && buf[i]!=0x20) break;
	  if(i>=12) return false; }

	lpSubInfo->dwCompressedSize=(buf[21])+(buf[22]<<8)+(buf[23]<<16)+(buf[24]<<24);
	lpSubInfo->dwOriginalSize=(buf[17])+(buf[18]<<8)+(buf[19]<<16)+(buf[20]<<24);
	buf[12]='\0';
	lstrcpy(lpSubInfo->szFileName,(char*)buf);
	lstrcpy(lpSubInfo->szMode,"ARG");
	lpSubInfo->wDate=(buf[15])+(buf[16]<<8);
	lpSubInfo->wTime=(buf[13])+(buf[14]<<8);
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	fseek( COpenArg::fp,lpSubInfo->dwCompressedSize,SEEK_CUR );

	return true;

}