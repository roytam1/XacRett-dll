#include "stdafx.h"
#include "OpenZac.h"

bool COpenZac::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenZac::common_buf;

	if( 4!=fread( buf, 1, 4, COpenZac::fp ) )
		return false;

	zVer2 = (buf[3]==2);

	return FindSub(lpSubInfo);
}

bool COpenZac::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}

bool COpenZac::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenZac::common_buf;

	if(36!=fread( buf, 1, 36, COpenZac::fp))
		return false;
	if( buf[0]!=0xd1 )
		return false;

	unsigned long  zLen = buf[32] + (buf[33]<<8) + (buf[34]<<16) + (buf[35]<<24);

	buf[32] = '\0';

	lstrcpy(lpSubInfo->szFileName,(char*)buf+1);
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	//lstrcpy(lpSubInfo->szMode,"");
	lpSubInfo->dwOriginalSize=zLen;

	fseek(COpenZac::fp, zLen, SEEK_CUR );
	if( zVer2 )
	{
		fread( buf, 2, 1, COpenZac::fp);// チェックサム
		//fseek(COpenZac::fp, 2, SEEK_CUR ); // チェックサム読
		lpSubInfo->dwCRC=buf[0]+ (buf[1]<<8);
		lpSubInfo->dwCompressedSize=zLen+37;
	}else{
		lpSubInfo->dwCompressedSize=zLen+35;
	}

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	return true;
}