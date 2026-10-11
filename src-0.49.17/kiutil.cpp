
#include "stdafx.h"
#include "kiutil.h"

///////////////
void kiutil::timeSet( const char* fname, FILETIME* pft )
{
	HANDLE han = CreateFile( fname,
							 GENERIC_READ | GENERIC_WRITE,
							 FILE_SHARE_READ,NULL,
							 OPEN_EXISTING,
							 FILE_ATTRIBUTE_NORMAL,
							 NULL );
	if( han==INVALID_HANDLE_VALUE )
		return;

	SetFileTime( han,pft,NULL,pft );

	CloseHandle( han );
}

void kiutil::timeSet( const char* fname, DWORD sec )
{
	struct tm* time=gmtime((long*)&sec);
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
		timeSet( fname,&ft );
	}
}

void kiutil::timeSet( const char* fname,WORD date,WORD time )
{
	FILETIME ft,lc;
	if( DosDateTimeToFileTime( date, time, &lc ) )
	{
		if( LocalFileTimeToFileTime( &lc, &ft ) )
			timeSet( fname,&ft );
	}
}

//////////////

void kiutil::wndFront( HWND wnd )
{
	static DWORD Ver = GetVersion();

	if( ( (Ver&0x80000000) && LOBYTE(LOWORD(Ver))>=4 && HIBYTE(LOWORD(Ver))>=10 ) ||
	    (!(Ver&0x80000000) && LOBYTE(LOWORD(Ver))>=5 )) // 新しいWindows
	{
		DWORD pid;
		DWORD thread1 = GetWindowThreadProcessId( GetForegroundWindow(),&pid );
		DWORD thread2 = GetCurrentThreadId();
		AttachThreadInput( thread2, thread1, TRUE );
		SetForegroundWindow( wnd );
		AttachThreadInput( thread2, thread1, FALSE );
		BringWindowToTop( wnd );
	}
	else  // 古いWindows
		SetForegroundWindow( wnd );
}

///////////////

char kiutil::lb[256];
bool kiutil::m_nodir=false;

void kiutil::pathInit()
{
	lb[0] = 0;
	for( int c=1; c!=256; c++ )
		lb[c] = (IsDBCSLeadByte(c) ? 2 : 1);
}

#define isdblb(c) (lb[(unsigned char)(c)]==2)
#define step(p) (p+=lb[(unsigned char)*(p)])

char* kiutil::pathMake( char* path )
{
	char* st = path;

	while( *st=='/' || *st=='\\' || *st=='?' ) st++;
	if( st[0]!='\0' && st[1]==':' )            st+=2;
	while( *st=='/' || *st=='\\' || *st=='?' ) st++;

	for( char *p=st,*q=st; *p!='\0'; step(p) )
	{
		if( isdblb(*p) )
			continue;

		if( *p=='\\' || *p=='/' )
		{
			// q-pが全部.だったら_に変換
			#define DO_DTV_CHECK
			#ifdef DO_DTV_CHECK
				bool all_dots = true;
				for(char* r=q; r!=p; ++r)
					if( *r != '.' )
						{ all_dots = false; break; }
				if( all_dots && p-q>=2 )
					for(char* r=q; r!=p; ++r)
						*r = '_';
			#endif
			// ディレクトリ作成
			*p='\0';
			if(!m_nodir)
				CreateDirectory( st, NULL );
			*p='\\';
			// qを更新
			q = p+1;
		}
		else if( *p<' ' || ( *p>'~' && !( 0xa0<=*p && *p<=0xdf ) ) || strchr(":*?\"<>|",*p) )
			*p = '_';
	}

	if(m_nodir)
	{
		char b[MAX_PATH];
		lstrcpy(b,pathName(st));
		lstrcpy(st,b);
	}

	return st;
}

void kiutil::pathMakeAbs_no_dtv_check( char* path )
{
	int i=0;
	for( char* p=path; *p!='\0'; step(p) )
	{
		if( i++ < 4 ) // 最初の４文字以内の \ はドライブを表す、ということにしておく。
			continue;

		if( *p=='\\' )
		{
			*p='\0';
			CreateDirectory( path, NULL );
			*p='\\';
		}
	}
}

void kiutil::pathSplit( const char* path, int* y, int* d )
{
	*y=-1, *d=-1;
	for( const char* x=path; *x!='\0'; step(x) ) 
	{
		if( *x=='\\' || *x=='/' )	*y=x-path,*d=-1;
		else if( *x=='.' )			*d=x-path;
	}
}

const char* kiutil::pathExt( const char* path )
{
	int y,d;
	kiutil::pathSplit( path,&y,&d );
	return (d!=-1) ? path+d+1 : path+strlen(path);
}

const char* kiutil::pathName( const char* path )
{
	int y,d;
	kiutil::pathSplit( path,&y,&d );
	return path+y+1;
}

#undef step
#undef isdblb

//////////////

char* kiutil::getline( char* str, int size, FILE* fp )
{
	if( size>0 )
	{
		char* p=str;

		while( --size )
		{
			int c = fgetc(fp);

			if( c<=0 ) // EOF || '\0'
				break;
			else if( c=='\n' )
				break;
			else if( c=='\r' )
			{
				c = fgetc(fp);
				if( c!=EOF && c!='\n' )
					ungetc( c,fp );
				break;
			}
			else
				*(p++) = c;
		}

		*p='\0';
	}

	return feof(fp) ? NULL : str;
}
void kiutil::getOriginalName( char* nw, char const * od,char* ext )
{
	int y,d;
	kiutil::pathSplit( od,&y,&d );
	strcpy( nw,od+y+1 );

	if( ext )
	{
		strcat( nw,"." );
		strcat( nw,ext );
	}
	else
		nw[d-y-1]='\0';
}
bool kiutil::pathExist( LPCTSTR lpFileName )
{
    bool bRet = ::GetFileAttributes(lpFileName) != 0xFFFFFFFF ;
    return bRet;
}
LPTSTR kiutil::pathAddBackslash( LPTSTR lpFileName )
{
	int l=lstrlen(lpFileName);
	if(lpFileName[l-1]!='\\')
	{
		lpFileName[l]='\\';
		lpFileName[l+1]=0;
	}
	return lpFileName;
}
bool kiutil::pathIsDirectory( LPCTSTR lpPath )
{
	DWORD ret=GetFileAttributes(lpPath);

	if(ret!=0xFFFFFFFF)
	{
		if(ret & FILE_ATTRIBUTE_DIRECTORY)
			return true;
		else
			return false;
	}else
	{
		return false;
	}
}
DWORD kiutil::getfilesize(LPCSTR file,DWORD* hight)
{
	if(::GetFileAttributes(file)&FILE_ATTRIBUTE_DIRECTORY)return 0xffffffff;

	HANDLE hFile;
	DWORD FileSizeLow, FileSizeHight;
	hFile = CreateFile(file,0,FILE_SHARE_READ | FILE_SHARE_WRITE,
	NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	FileSizeLow = GetFileSize(hFile, &FileSizeHight);
	CloseHandle(hFile);
	if(hight)
		*hight=FileSizeHight;
	return FileSizeLow;
}
#define next_char(ptn) IsDBCSLeadByte(*ptn)?ptn+2:ptn+1
int kiutil::StrMatch( const char *ptn, const char *str )
{
	switch( *ptn )
	{
		case '\0':
			return '\0' == *str;
		case '*':
			return StrMatch( next_char(ptn), str ) || (('\0' != *str) && StrMatch( ptn, next_char(str) ));
		case '?':
			return ('\0' != *str) && StrMatch( next_char(ptn), next_char(str) );
		case '/':
		case '\\':
			return (('/'==(unsigned char)*str) || ('\\'==(unsigned char)*str)) && StrMatch( next_char(ptn), next_char(str) );
		default:
			return ((unsigned char)*ptn==(unsigned char)*str) && StrMatch( next_char(ptn), next_char(str) );
	}
}
#undef next_char