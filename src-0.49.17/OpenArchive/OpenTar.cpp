#include "stdafx.h"
#include "OpenTar.h"

static DWORD tar_s2i(char* d,int l)
{
	DWORD s=0;
	for( int i=0; i!=l; i++ )
	{
		if( '0'<=d[i] && d[i]<='7' )
			s = (s<<3)+(d[i]-'0');
		else if( d[i]!=' ' )
			break;
	}
	return s;
}
bool COpenTar::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	CloseArchive();
	if( !TarOpen(COpenTar::szArcFileName) )
		{TarClose();return false;}

	return Find_sub(lpSubInfo);
}

bool COpenTar::FindNext(INDIVIDUALINFO *lpSubInfo)
{

	return Find_sub(lpSubInfo);
}

bool COpenTar::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenTar::common_buf;
	TarHeader* head;
	bool bLongName=false;
	char LongBuf[MAX_PATH];
	BYTE zeroheader[512];
	memset( zeroheader,0,sizeof(zeroheader) );

	char* realfname;
	DWORD realfsize;
	DWORD realattrib;
	DWORD realupdate;
	DWORD realcreatetime;

read:
	if(512!=TarRead(buf,512))
	{TarClose();return false;}

	// ヘッダ読み込み
	head = (TarHeader*)buf;
	if( 0==memcmp( zeroheader,buf,512 ) )
		{TarClose();return false;}

	// ファイルネーム処理
	if( bLongName )	realfname=LongBuf;
	else			realfname=head->filename;
	bLongName = false;

	// ブロックサイズ
	realfsize=tar_s2i(head->filesize,12);

	// タイプで処理分け
	switch( head->type )
	{
	case '1':
	case '2':
	case '3':
	case '4':
	case '6':
	case '7':
	case 'M':
	case 'S':
	case 'V'://とりあえずハンドリング不能と判断したもの全部
		if( realfsize!=0 )
			SkipBlock( (realfsize>>9)+1 );
		goto read;
	case 'L':
	case 'N'://longname
		if( realfsize!=0 && 512==TarRead(buf,512) )
		{
			bLongName=true;
			strncpy(LongBuf,(char*)buf,MAX_PATH);
			SkipBlock( realfsize>>9 );
		}
		goto read;
	case '5':
	case 'D'://ディレクトリだと思う
		realattrib|=(1<<14);
	default:{//展開
		realattrib = tar_s2i(head->attrib,8);
		realupdate = tar_s2i(head->update,12);
		if( head->magic_and_ver[6]==' '
		 && head->magic_and_ver[7]==' ' )
			realcreatetime = tar_s2i(head->creationdate,12);
		else
			realcreatetime = realupdate;

		// ファイル名が変なら飛ばす
		//ディレクトリメンバだったら飛ばす
		if( *realfname==0 || realattrib&(1<<14))
		{
			if( realfsize!=0 )
				SkipBlock( (realfsize>>9)+1 );
			goto read;
		}
		
		lpSubInfo->dwCompressedSize=0;
		lpSubInfo->dwOriginalSize = realfsize;

		int st_mode=realattrib;
		realattrib=((st_mode & S_IWRITE)==0)*FA_RDONLY /* Read Only */
		+((st_mode & S_IREAD)==0)*FA_HIDDEN /* Hidden */
		+ 0 *FA_SYSTEM						/* System File */
		+ 0 *FA_LABEL						/* volume label */
		+((st_mode & S_IFDIR)!=0)*FA_DIREC /* Directory */
		+0*FA_ARCH;							/* Archive Attribute */
		/*lstrcpy(lpSubInfo->szAttribute ,"----");
		if (realattrib & FILE_ATTRIBUTE_READONLY)
			lpSubInfo->szAttribute[2] = 'R';
		if (realattrib & FILE_ATTRIBUTE_HIDDEN)
			lpSubInfo->szAttribute[1] = 'H';
		if (realattrib & FILE_ATTRIBUTE_SYSTEM)
			lpSubInfo->szAttribute[0] = 'S';
		if (realattrib & FILE_ATTRIBUTE_DIRECTORY)
			lpSubInfo->szAttribute[3] = 'D';
		if (realattrib & FILE_ATTRIBUTE_ARCHIVE)
			lpSubInfo->szAttribute[3] = 'A';
			*/
		lpSubInfo->szAttribute[0]= (realattrib & FA_ARCH)	? 'A' : '-'; /* Archive */
		lpSubInfo->szAttribute[1]= (realattrib & FA_SYSTEM)	? 'S' : '-'; /* System */
		lpSubInfo->szAttribute[2]= (realattrib & FA_HIDDEN)	? 'H' : '-'; /* Hide */
		lpSubInfo->szAttribute[3]= (realattrib & FA_RDONLY)	? 'R' : '-'; /* Read Only*/
		lpSubInfo->szAttribute[4]='\0';

		//lstrcpy(lpSubInfo->szMode,"");
		lstrcpy(lpSubInfo->szFileName,realfname);

		struct tm* time=localtime((long*)&realupdate);
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
		while( realfsize>>9 )
		{
			TarRead(buf,512);
			realfsize-=512;
		}
		if( realfsize!=0 )
		{
			TarRead(buf,512);
		}

		};
		}break;
	}

	return true;
}

void COpenTar::SkipBlock(int num)
{
	static char dummy[512];
	for( ; num!=0; num-- )
		TarRead(dummy,512);
}

/******ごく普通なTarのRead/Write****************/

bool COpenTar::TarOpen(LPCSTR fname)
{
	TarClose();
	fp = fopen( fname,"rb" );
	return (fp!=NULL);
}

size_t COpenTar::TarRead(void* red,size_t size)
{
	if( fp==NULL )return 0;
	size_t ans=fread( red,1,size,fp );
	return (ans==-1)?0:ans;
}

void COpenTar::TarClose()
{
	if( fp!=NULL )
		fclose(fp),fp=NULL;
}

/*********Tar+Bzip2のRead****************/

bool COpenTbz::TarOpen(LPCSTR fname)
{
	TarClose();
	bfp=BZ2_bzopen( fname,"rb" );
	return (bfp!=NULL);
}

size_t COpenTbz::TarRead(void* red,size_t size)
{
	if( bfp==NULL )return 0;
	size_t ans=BZ2_bzread( bfp,red,size );
	return (ans==-1)?0:ans;
}

void COpenTbz::TarClose()
{
	if( bfp!=NULL )
		BZ2_bzclose( bfp ),bfp=NULL;
}

/*********Tar+GzipのRead*****************/

bool COpenTgz::TarOpen(LPCSTR fname)
{
	TarClose();

	BYTE Magic[10];
	FILE* _fp=fopen( fname,"rb" );
	if( 10!=fread(Magic,1,10,_fp) )
	{fclose(_fp);return false;}
	fclose(_fp);
	if( Magic[0]!=0x1f || (Magic[1]!=0x8b && Magic[1]!=0x9e) )
		return false;

	gfp=gzopen( fname,"rb" );
	return (gfp!=NULL);
}

size_t COpenTgz::TarRead(void* red,size_t size)
{
	if( gfp==NULL )return 0;
	size_t ans=gzread( gfp,red,size );
	return (ans==-1)?0:ans;
}

void COpenTgz::TarClose()
{
	if( gfp!=NULL )
		gzclose( gfp ),gfp=NULL;
}

/*********Tar+CompressのRead*****************/

bool COpenTaZ::TarOpen(LPCSTR fname)
{
	TarClose();

	BYTE Magic[10];
	FILE* fp=fopen( fname,"rb" );
	if( 10!=fread(Magic,1,10,fp) )
	{fclose(fp);return false;}
	fclose(fp);
	if( Magic[0]!=0x1f || Magic[1]!=0x9d )
		return false;

	zfp=Z_open( fname,"rb" );
	return (zfp!=NULL);
}

size_t COpenTaZ::TarRead(void* red,size_t size)
{
	if( zfp==NULL )return 0;
	size_t ans=Z_read( red,size,zfp );
	return (ans==-1)?0:ans;
}

void COpenTaZ::TarClose()
{
	if( zfp!=NULL )
		Z_close( zfp ),zfp=NULL;
}

/*********Tar+PackのRead*****************/

bool COpenTaz::TarOpen(LPCSTR fname)
{
	TarClose();

	BYTE Magic[10];
	FILE* fp=fopen( fname,"rb" );
	if( 10!=fread(Magic,1,10,fp) )
	{fclose(fp);return false;}
	fclose(fp);
	if( Magic[0]!=0x1f || Magic[1]!=0x1e )
		return false;

	pfp=P_open( fname,"rb" );
	return (pfp!=NULL);
}

size_t COpenTaz::TarRead(void* red,size_t size)
{
	if( pfp==NULL )return 0;
	size_t ans=P_read( red,size,pfp );
	return (ans==-1)?0:ans;
}

void COpenTaz::TarClose()
{
	if( pfp!=NULL )
		P_close( pfp ),pfp=NULL;
}
