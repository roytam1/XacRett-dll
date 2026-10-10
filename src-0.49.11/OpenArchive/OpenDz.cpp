#include "stdafx.h"
#include "OpenDz.h"

bool COpenDz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenDz::common_buf;

	if( 12 != fread( buf, 1, 12, COpenDz::fp ) )
		return false;

	COpenDz::ver1 = (buf[2]==1 ? 8 : 0);
	unsigned int indexPos = buf[4]+(buf[5]<<8)+(buf[ 6]<<16)+(buf[ 7]<<24);
	COpenDz::fileNum  = buf[8]+(buf[9]<<8)+(buf[10]<<16)+(buf[11]<<24);

	if (COpenDz::fileNum==0)
		return false;
	if (0!=fseek( COpenDz::fp, indexPos, SEEK_SET ))
		return false;

	return COpenDz::read_header(lpSubInfo);
}

bool COpenDz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenDz::read_header(lpSubInfo);
}

bool COpenDz::read_header(INDIVIDUALINFO *lpSubInfo)
{
	static unsigned int i =0;
	DzfileInf fi;
	unsigned char* buf = COpenDz::common_buf;

	if (i >= COpenDz::fileNum)
	{
		i =0;
		return false;
	}
	if( 32-ver1 != fread( buf, 1, 32-ver1, COpenDz::fp ) )
	{
		i =0;
		return false;
	}
	fi.ptr = buf[ 0]+(buf[ 1]<<8)+(buf[ 2]<<16)+(buf[ 3]<<24);
	fi.csz = buf[ 4]+(buf[ 5]<<8)+(buf[ 6]<<16)+(buf[ 7]<<24);
	fi.usz = buf[ 8]+(buf[ 9]<<8)+(buf[10]<<16)+(buf[11]<<24);
	fi.crc = buf[16]+(buf[17]<<8)+(buf[18]<<16)+(buf[19]<<24);
	fi.pak = buf[14]+(buf[15]<<8);
	fi.type= buf[20]+(buf[21]<<8)+(buf[22]<<16)+(buf[23]<<24);
	if( COpenDz::ver1 )
	{
		fi.date = 0;
		fi.isz  = fi.csz;
	}
	else
	{
		fi.date= buf[24]+(buf[25]<<8)+(buf[26]<<16)+(buf[27]<<24);
		fi.date += 0x00200000;
		fi.isz = buf[28]+(buf[29]<<8)+(buf[30]<<16)+(buf[31]<<24);
	}
	unsigned int fnlen = buf[12]+(buf[13]<<8);
	fread( buf, 1, fnlen, COpenDz::fp );
	fnlen = (fnlen>MAX_PATH-1 ? MAX_PATH-1 : fnlen);
	memcpy( fi.nam, buf, fnlen );
	fi.nam[fnlen] = '\0';

	lpSubInfo->dwCompressedSize = fi.csz;

	lpSubInfo->dwOriginalSize = fi.usz;
	lpSubInfo->dwCRC = fi.crc;

	lstrcpy(lpSubInfo->szAttribute,"A---W");
	//lstrcpy(lpSubInfo->szMode,"");

	lstrcpy(lpSubInfo->szFileName,fi.nam);
	if (!COpenDz::ver1)
	{
		lpSubInfo->wDate = (fi.date & 0xFFFF0000) >> 16;
		lpSubInfo->wTime = (fi.date & 0xFFFF) > 0x7FFF ? (fi.date & 0xFFFF) - 0x10000:fi.date & 0xFFFF;
	}

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0)
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));

	i++;
	return true;
}