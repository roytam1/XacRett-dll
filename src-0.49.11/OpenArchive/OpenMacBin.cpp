#include "stdafx.h"
#include "OpenMacBin.h"

bool COpenMacBin::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	path[0]=0;
	return Find_sub(lpSubInfo);
}

bool COpenMacBin::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}
bool COpenMacBin::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	static int next=0;
	int offset, dirflag;
	unsigned char* hdr = COpenMacBin::common_buf;

	if( 128 != fread( hdr, 1, 128, COpenMacBin::fp ) )
	{
		next=0;
		return false;
	}

	// 簡易チェック
	if( !easyCheck( hdr ) )
	{next=0;return false;}

	char name[MAX_PATH];
	memcpy( name, hdr+2, 63 ), name[hdr[1]]='\0'; // 名前
	DWORD siz = x68Long(hdr+83);		// データサイズ
	DWORD dtime = x68Long(hdr+95);	// 更新日時

	// オフセット
	offset = x68Long(hdr+83) + x68Long(hdr+87) + x68Word(hdr+99);
	int _2ndH = x68Word(hdr+120);
	_2ndH = (_2ndH&0x4f) ? (((_2ndH>>7)+1)<<7) : _2ndH;
	fseek( COpenMacBin::fp, _2ndH, SEEK_CUR );

	// ディレクトリフラグ
	dirflag = 0;
	if( hdr[0]==1 )
	{
		if( x68Long(hdr+69)==0xffffffff )
			dirflag = 1;
		else
			dirflag = 2;
	}

	next = ftell(COpenMacBin::fp) + offset;

	if( dirflag==1 ) // fold-START
		{
			strcat( path, name );
			strcat( path, "\\" );
			return Find_sub(lpSubInfo);
		}
		else if( dirflag==2 ) // fold-END
		{
			char *p,*pp,*ppp;
			for( p=path,pp=path,ppp=path; *p; p=CharNext(p) )
				if( *p=='\\' )
					{ppp=pp;pp=p+1;}
			*ppp=0;
			return Find_sub(lpSubInfo);
		}
		else
		{
			strcpy( lpSubInfo->szFileName, path );
			strcat( lpSubInfo->szFileName, name );
			lpSubInfo->dwCompressedSize=siz+126;//126はヘッダー分
			lpSubInfo->dwOriginalSize=siz;
			dtime-=2082844800l;
			struct tm* time=localtime((long*)&dtime);
			if( time!=NULL )
			{
				FILETIME ft;
				SYSTEMTIME sys;

				sys.wYear =      time->tm_year+1900;
				sys.wMonth =     time->tm_mon+1;
				sys.wDayOfWeek = time->tm_wday;
				sys.wDay =       time->tm_mday;
				sys.wHour =      time->tm_hour;
				sys.wMinute =    time->tm_min;
				sys.wSecond =    time->tm_sec;
				sys.wMilliseconds = 0;
				SystemTimeToFileTime(&sys,&ft);
				FileTimeToDosDateTime(&ft,&lpSubInfo->wDate,&lpSubInfo->wTime);
			}
			double tmp1 = (double)lpSubInfo->dwOriginalSize;
			double tmp2 = (double)lpSubInfo->dwCompressedSize;
			if ((lpSubInfo->dwOriginalSize > 0 )&& (lpSubInfo->dwCompressedSize > 0)){
				lpSubInfo->wRatio = (WORD)((tmp2*1000 / tmp1));
			}
			lstrcpy(lpSubInfo->szAttribute ,"A---W");
		}

		fseek( COpenMacBin::fp, next, SEEK_SET ); // 次のヘッダへ
		//lstrcpy(lpSubInfo->szMode,"");
	return true;
}

bool COpenMacBin::easyCheck( const unsigned char* hdr )
{
	// 共通チェック
	if( !( 1<=hdr[1] && hdr[1]<=63 ) || (hdr[74]!=0x00) || (hdr[82]!=0x00) )
		return false;
	if( hdr[0] == 0x01 )
	{
		if( 0!=memcmp( hdr+65, "fold", 4 ) || x68Long(hdr+69)<0xfffffffe )
			return false;
	}
	else if( hdr[0] != 0x00 )
		return false;

	// バージョン分岐
	int ver = 1;
		 if( hdr[122]==0   ) ver=1;
	else if( hdr[122]==129 ) ver=2;
	else if( hdr[122]==130 ) ver=3;
	else              return false;
		 if( hdr[0]==1 )     ver=20;
	switch( ver )
	{
	case 1:	if( !isAllZero( hdr+99 ,hdr+126 ) ) return false; break;
	case 2:
	case 20:if( !isAllZero( hdr+102,hdr+116 ) ) return false; break;
	case 3:	if( !isAllZero( hdr+108,hdr+116 ) || 0!=memcmp( hdr+102, "mBIN", 4 ) ) return false; break;
	}

	return true;
}