#include "stdafx.h"
#include "OpenYnc.h"

bool COpenYnc::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	return COpenYnc::Find_sub(lpSubInfo);
}

bool COpenYnc::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenYnc::Find_sub(lpSubInfo);
}

bool COpenYnc::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	char* str=(char*)COpenYnc::common_buf;
	// ƒtƒ@ƒCƒ‹–¼“Ç‚Ý‚Æ‚è
	static int readed=0;

	char outname[FNAME_MAX32*2]=" ";
	char size[FNAME_MAX32*2+20];
	readed=ftell(COpenYnc::fp);

	while( !feof(COpenYnc::fp) )
	{
		kiutil::getline( str,XACR_BUFSIZE,COpenYnc::fp );
		if( 0==strncmp(str,"=ybegin ",8) )
		{
			char* nm1;
			if( (nm1=strstr(str," size=")) != 0 )
			{
				strcpy(size, nm1+6 );
				for( int sp1=strlen(size)-1;
					sp1>=0 && size[sp1]==' '; --sp1 )
					size[sp1] = '\0';
			}
			char* nm;
			if( (nm=strstr(str," name=")) != 0 )
			{
				strcpy( outname, nm+6 );
				for( int sp=strlen(outname)-1;
					sp>=0 && outname[sp]==' '; --sp )
					outname[sp] = '\0';
				break;
			}
		}
	}
	if( feof(COpenYnc::fp) || outname[0]=='\0' )
	{
		readed=0;return false;
	}

	// Data•”•ª”ò‚Î‚µ
	char crc32[9];
	while( !feof(COpenYnc::fp) )
	{
		kiutil::getline( str,XACR_BUFSIZE,COpenYnc::fp );
		if( 0==strncmp(str,"=yend",5) )
		{
			char* nm3;
			if( (nm3=strstr(str," crc32=")) != 0 )
			{
				strcpy(crc32, nm3+7 );
				for( int sp3=strlen(crc32)-1;
					sp3>=0 && crc32[sp3]=='\n'; --sp3 )
					crc32[sp3] = '\0';
			}
			break;
		}
	}

	lstrcpy(lpSubInfo->szFileName,outname);
	lpSubInfo->dwCRC=(DWORD)StdPlus_xtoi(crc32);
	lpSubInfo->dwOriginalSize=(DWORD)atoi(size);
	lpSubInfo->dwCompressedSize=ftell(fp)-readed;

	lstrcpy(lpSubInfo->szAttribute ,"A---W");

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	lstrcpy(lpSubInfo->szMode,"stored");

	return true;
}