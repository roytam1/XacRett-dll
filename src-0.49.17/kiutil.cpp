
#include "stdafx.h"
#include "kiutil.h"
#include <stdlib.h>
#include "dbglog.h"

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

//=============================================================
// 8.3 fallback for file systems that cannot hold long names
//=============================================================
// Windows 3.1 + Win32s (and Windows 95 with "lfn=off") has no long name
// support at all: CreateFile/fopen refuse a name that does not fit 8.3,
// so extracting such an archive dies with ERROR_CANNOT_WRITE (0x8013).
// Ask the file system once - on demand, the first time a name that does
// not fit turns up - and when long names are unavailable mangle the name
// the way the short name generators of that era did:
//
//     LongFileName.txt  ->  LONGFI~1.TXT
//     short.LONGEXT     ->  short.LON
//     CON.txt           ->  CON~1.TXT
//
// Intermediate components are mangled from the string alone, so every
// member of one archive lands in the same directory; only the last
// component is probed against the disk, which keeps ~N unique over
// repeated extractions.  Where long names do work this is one cached
// flag test and nothing is ever renamed.
//
// XACRETT_8DOT3 in the environment forces the mangling on, which is how
// the fallback can be exercised on a machine that does have long names.

int kiutil::m_lfn = -1;

// Create this file, open the same name back, and read the name back
// with FindFirstFile.  All three halves matter: a 16 bit file system
// driver can accept a create whose name it cannot represent and then
// keep the file under some mangled name, and a driver that truncates
// consistently would even pass the re-open - but it cannot return the
// untruncated name from FindFirstFile.  The file is always removed
// again.  Any failure means "no long names": the directory proved
// writable by the short-name control, so renaming can only help.
static bool p83probecreate( const char* name )
{
	HANDLE h = CreateFileA( name, GENERIC_READ|GENERIC_WRITE,
	                        FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
	                        CREATE_ALWAYS,
	                        FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_TEMPORARY,
	                        NULL );
	bool ok = false;

	if( h!=INVALID_HANDLE_VALUE )
		{
		CloseHandle( h );
		h = CreateFileA( name, 0,
		                   FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
		                   OPEN_EXISTING,
		                   FILE_ATTRIBUTE_NORMAL, NULL );
		if( h!=INVALID_HANDLE_VALUE )
			{
			CloseHandle( h );
			WIN32_FIND_DATA fd;
			HANDLE fh = FindFirstFileA( name, &fd );
			if( fh!=INVALID_HANDLE_VALUE )
				{
				FindClose( fh );
				ok = ( 0==stricmp( fd.cFileName, name ) );
				}
			}
		}
	DeleteFileA( name );
	return ok;
}

bool kiutil::longNameOK()
{
	if( m_lfn>=0 )
		return m_lfn!=0;

	if( getenv("XACRETT_8DOT3") )
	{
		m_lfn = 0;
		return false;
	}

	/* Differential probe: try the long name AND a short one in the same
	   directory.  Deciding from the error code instead does not work -
	   on the platforms that need this fallback at all, a 16 bit FAT
	   driver is free to report the refusal as ACCESS_DENIED or
	   SHARING_VIOLATION, and every such guess has to mean "long names
	   are fine" to protect a read-only output dir, which silently
	   switches the fallback back off exactly where it is needed.
	   The short name is the control: if it lands, the directory is
	   writable and only the long name was refused. */
	bool lng = p83probecreate( "XacRettLongNameProbe.tmp" );
	DbgLog("longNameOK: long-probe=%d", lng ? 1 : 0);
	if( lng )
	{
		m_lfn = 1;
		return true;
	}

	/* Both refused: nothing can be written here at all, so the
	   extraction is going to fail on its own and renaming would
	   only make the message less clear.  Assume long names. */
	bool sht = p83probecreate( "XPRB~1.TMP" );
	DbgLog("longNameOK: short-control=%d decision=%d", sht ? 1 : 0, sht ? 0 : 1);
	m_lfn = sht ? 0 : 1;
	return m_lfn!=0;
}

// DBCS aware: a name is never cut in the middle of a double byte char.
static int p83width( const char* s )
{
	if( kiutil::lb[(unsigned char)*s]!=2 || s[1]=='\0' )
		return 1;
	return 2;
}

static int p83chars( const char* s )
{
	int n = 0;
	while( *s )
	{
		n++;
		s += p83width(s);
	}
	return n;
}

static int p83bytes( const char* s, int chars )
{
	int n = 0;
	while( chars-- > 0 && *s )
	{
		int w = p83width(s);
		n += w;
		s += w;
	}
	return n;
}

// A name the kernel turns into a device instead of a file.
static bool p83device( const char* base, int len )
{
	static const char* const dev[] = {
		"CON","PRN","AUX","NUL","CLOCK$","CONIN$","CONOUT$",
		"COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9",
		"LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9" };
	int i, j;

	// Win32 ignores trailing dots and blanks before the extension
	while( len>0 && (base[len-1]==' ' || base[len-1]=='.') )
		len--;
	if( len<3 )
		return false;

	for( i=0; i!=int(sizeof(dev)/sizeof(dev[0])); i++ )
	{
		int dl = lstrlen(dev[i]);
		if( dl!=len )
			continue;
		for( j=0; j<dl; j++ )
		{
			char a = base[j], b = dev[i][j];
			if( 'a'<=a && a<='z' ) a = (char)(a-'a'+'A');
			if( 'a'<=b && b<='z' ) b = (char)(b-'a'+'A');
			if( a!=b )
				break;
		}
		if( j==dl )
			return true;
	}
	return false;
}

// Does prefix+comp already name something on disk?
static bool p83taken( const char* prefix, const char* comp )
{
	char full[MAX_PATH*2];
	if( prefix==NULL )
		return false;
	lstrcpyn( full,prefix,sizeof(full) );
	lstrcat( full,comp );
	return GetFileAttributes(full)!=0xFFFFFFFF;
}

// Would this component need an 8.3 name at all?  A pure string test, so
// that the file system probe above only ever runs when there really is
// something to mangle.
static bool p83need( const char* comp )
{
	char base[MAX_PATH];
	int  blen, bc, ec, d = -1, i, el;

	for( i=lstrlen(comp)-1; i>=1; i-- )
		if( comp[i]=='.' ) { d=i; break; }

	if( d<0 )
	{
		lstrcpyn( base,comp,sizeof(base) );
		el = lstrlen(comp);
	}
	else
	{
		memcpy( base,comp,d ); base[d] = '\0';
		el = d+1;
	}

	for( i=0; base[i]; i++ )
		if( base[i]<' ' || strchr(":*?\"<>|",base[i]) )
			return true;

	blen = lstrlen(base);
	bc   = p83chars(base);
	ec   = p83chars( comp+el );
	return bc>8 || ec>3 || strchr(base,'.')!=NULL ||
	       ( blen>0 && (base[blen-1]==' ' || base[blen-1]=='.') ) ||
	       p83device(base,blen);
}

// Writes the 8.3 form of one path component.  prefix is the part of the
// path already built, and is NULL unless this is the last component, so
// that only a file we are about to create has to differ from what the
// disk already holds.
static void p83fit( const char* comp, const char* prefix, char* out )
{
	char base[MAX_PATH];
	char ext[MAX_PATH];
	char cand[MAX_PATH];
	int  blen, d = -1, i, bc, ec, ne;

	// the extension starts at the last dot that is not the first char
	for( i=lstrlen(comp)-1; i>=1; i-- )
		if( comp[i]=='.' ) { d=i; break; }

	if( d<0 )
	{
		lstrcpyn( base,comp,sizeof(base) );
		ext[0] = '\0';
	}
	else
	{
		memcpy( base,comp,d ); base[d] = '\0';
		lstrcpyn( ext,comp+d+1,sizeof(ext) );
	}

	// characters no 8.3 name can hold (pathMake does this as well, but
	// this entry point is also used on names that never went through it)
	for( i=0; base[i]; i++ )
		if( base[i]<' ' || strchr(":*?\"<>|",base[i]) )
			base[i] = '_';
	for( i=0; ext[i]; i++ )
		if( ext[i]<' ' || strchr(":*?\"<>|",ext[i]) )
			ext[i] = '_';

	blen = lstrlen(base);
	bc   = p83chars(base);
	ec   = p83chars(ext);
	ne   = (ec>3) ? p83bytes(ext,3) : lstrlen(ext);

	if( bc<=8 && strchr(base,'.')==NULL &&
	    !( blen>0 && (base[blen-1]==' ' || base[blen-1]=='.') ) &&
	    !p83device(base,blen) )
	{
		// the name itself is fine, so keep it and only trim an extension
		// that is too long; a file already on disk forces the ~N form
		int nb = blen;
		memcpy( cand,base,blen );
		if( ec>0 )
		{
			cand[nb] = '.';
			memcpy( cand+nb+1,ext,ne );
			nb += 1+ne;
		}
		cand[nb] = '\0';
		if( !p83taken(prefix,cand) )
		{
			lstrcpyn( out,cand,MAX_PATH );
			return;
		}
	}

	// base(6 or 5 chars) + "~N", N counted up from 1
	for( int n=1; n<100; n++ )
	{
		int nd = (n<10) ? 1 : 2;
		int room = 8-1-nd;
		int nb = 0;

		for( i=0; i<room && base[i]; i += p83width(base+i) )
		{
			if( base[i]=='.' )
				cand[nb++] = '_';
			else
			{
				int w = p83width(base+i);
				memcpy( cand+nb,base+i,w );
				nb += w;
			}
		}
		cand[nb++] = '~';
		if( nd==2 )
			cand[nb++] = (char)('0'+n/10);
		cand[nb++] = (char)('0'+n%10);
		if( ec>0 )
		{
			cand[nb++] = '.';
			memcpy( cand+nb,ext,ne );
			nb += ne;
		}
		cand[nb] = '\0';

		if( !p83taken(prefix,cand) )
		{
			lstrcpyn( out,cand,MAX_PATH );
			return;
		}
	}

	// every name in the ~1..~99 range is taken: overwrite ~99
	lstrcpyn( out,cand,MAX_PATH );
}

char* kiutil::pathMake83( char* path )
{
	char res[MAX_PATH];
	char comp[MAX_PATH];
	char fit[MAX_PATH];
	int  rn = 0;
	int  i;
	char* src = path;

	// "X:\" and friends are kept as they are
	while( *src=='/' || *src=='\\' || *src=='?' )
	{
		if( rn<MAX_PATH-1 ) res[rn++] = *src;
		src++;
	}
	if( src[0]!='\0' && src[1]==':' )
	{
		if( rn<MAX_PATH-3 )
		{
			res[rn++] = src[0];
			res[rn++] = ':';
			res[rn++] = '\\';
		}
		src += 2;
	}

	for( ;; )
	{
		int cn = 0;

		while( *src!='\0' && *src!='\\' && *src!='/' )
		{
			if( cn<MAX_PATH-1 ) comp[cn++] = *src;
			src++;
		}
		comp[cn] = '\0';

		if( cn==0 )		// a separator, or the end of the path
		{
			if( *src=='\0' )
				break;
			if( rn<MAX_PATH-1 ) res[rn++] = '\\';
			src++;
			continue;
		}

		// the file system is asked about a name only when it really does
		// not fit, and only when long names are unavailable
		if( strcmp(comp,".")==0 || strcmp(comp,"..")==0 ||
		    !p83need(comp) || longNameOK() )
		{
			for( i=0; i<cn; i++ )
				if( rn<MAX_PATH-1 ) res[rn++] = comp[i];
		}
		else
		{
			// only the last component has to be free on disk
			res[rn] = '\0';
			p83fit( comp, *src=='\0' ? (const char*)res : NULL, fit );
			DbgLog("pathMake83: [%s] -> [%s]", comp, fit);
			for( i=0; fit[i]; i++ )
				if( rn<MAX_PATH-1 ) res[rn++] = fit[i];
		}

		if( *src=='\0' )
			break;
		if( rn<MAX_PATH-1 ) res[rn++] = '\\';
		src++;
	}

	res[rn] = '\0';
	memcpy( path,res,rn+1 );
	return path;
}

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
			*p='\\';
			// qを更新
			q = p+1;
		}
		else if( *p<' ' || ( *p>'~' && !( 0xa0<=*p && *p<=0xdf ) ) || strchr(":*?\"<>|",*p) )
			*p = '_';
	}

	// long file name fallback (a no-op wherever long names work).
	// The FULL path has to go in, not "st": st skips the drive, and the
	// collision probe inside pathMake83 is a GetFileAttributes on
	// prefix+candidate, so a drive-relative prefix never resolves and
	// two members whose mangled names collide silently overwrite each
	// other (seen with -j, where the caller memmoves the drive-stripped
	// result back over the buffer).  pathMake83 keeps a leading X: intact
	// and returns the same buffer, so "st" stays valid afterwards.
	pathMake83( path );

	// the directories are made from the finished path, so that a mangled
	// name is the one that actually gets created
	if( !m_nodir )
	{
		for( char *p=st; *p!='\0'; step(p) )
		{
			if( *p!='\\' )
				continue;
			*p='\0';
			CreateDirectory( st, NULL );
			*p='\\';
		}
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
#ifndef _WCHAR_T_DEFINED /* VC6 has no native wchar_t */
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif
// UTF-8 to ANSI without CP_UTF8 (missing on some retro Windows).
// Strict decode; returns true with dst NUL-terminated, false (dst untouched) otherwise.
bool kiutil::utf8toAnsi(char *dst, int dstsize, const char *src)
{
	unsigned short wbuf[MAX_PATH];
	int si = 0, wi = 0;
	while (src[si] != '\0') {
		unsigned char c = (unsigned char)src[si];
		unsigned long cp;
		int len;
		if (c < 0x80) { cp = c; len = 1; }
		else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
		else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
		else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
		else return false;
		for (int i = 1; i < len; i++) {
			unsigned char cc = (unsigned char)src[si + i];
			if (cc == '\0' || (cc & 0xC0) != 0x80) return false;
			cp = (cp << 6) | (cc & 0x3F);
		}
		if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000))
			return false;
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			return false;
		if (cp < 0x10000) {
			if (wi + 1 >= MAX_PATH) return false;
			wbuf[wi++] = (unsigned short)cp;
		} else {
			if (wi + 2 >= MAX_PATH) return false;
			cp -= 0x10000;
			wbuf[wi++] = (unsigned short)(0xD800 | (cp >> 10));
			wbuf[wi++] = (unsigned short)(0xDC00 | (cp & 0x3FF));
		}
		si += len;
	}
	wbuf[wi] = 0;
	if (WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)wbuf, -1, dst, dstsize, NULL, NULL) <= 0)
		return false;
	return true;
}

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