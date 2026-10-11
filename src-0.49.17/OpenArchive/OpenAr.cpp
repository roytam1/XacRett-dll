#include "stdafx.h"
#include "OpenAr.h"
//–¢Š®¬!!
bool COpenAr::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// fresh string table per listing (freed here, leaked only at unload)
	if(COpenAr::m_strtab){ delete[] COpenAr::m_strtab; COpenAr::m_strtab=NULL; }
	COpenAr::m_strtablen=0;
	unsigned char* buf = COpenArchive::common_buf;
	if( 8!=fread( buf, 1, 8, fp ) )
	{
		fclose( COpenAr::fp );
		return false;
	}
	return FindSub(lpSubInfo);
}

bool COpenAr::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}

bool COpenAr::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	char outname[MAX_PATH];
	unsigned long origsiz;
	unsigned long date;
	unsigned char* buf = COpenArchive::common_buf;
	FILE* ar =fp;

	if(feof(ar)) return false;

	// ƒwƒbƒ_“Ç‚Ýž‚Ý
	if( 60!=fread( buf, 1, 60, ar ) || buf[58]!='`' || buf[59]!='\n' )
		return false;

	date = (unsigned long)strtol( (char*)buf+16, NULL,10 );
	origsiz =(unsigned long)strtol( (char*)buf+48, NULL,10 );
	unsigned long nextpos = ftell(ar) + origsiz + (origsiz&1);

	// string table "//": capture for "/offset" long names, never list it
	if( buf[0]=='/' && buf[1]=='/' && buf[2]==' ' )
	{
		long tablen = (long)origsiz;
		if(COpenAr::m_strtab){ delete[] COpenAr::m_strtab; COpenAr::m_strtab=NULL; COpenAr::m_strtablen=0; }
		if( tablen>0 && tablen<0x100000 )
		{
			COpenAr::m_strtab = new char[tablen+1];
			if( COpenAr::m_strtab )
			{
				long got = fread( COpenAr::m_strtab, 1, tablen, ar );
				COpenAr::m_strtab[got]='\0';
				COpenAr::m_strtablen = got;
			}
		}
		fseek( ar, nextpos, SEEK_SET );
		return FindSub(lpSubInfo);
	}
	// linker member "/" and other specials (e.g. "/SYMDEF/"): skip
	if( buf[0]=='/' && (buf[1]==' ' || buf[1]<'0' || buf[1]>'9') )
	{
		fseek( ar, nextpos, SEEK_SET );
		return FindSub(lpSubInfo);
	}
	if( buf[0]=='/' )
	{
		// "/offset" long name via the string table
		long off = strtol( (char*)buf+1, NULL,10 );
		if( COpenAr::m_strtab && off>=0 && off<COpenAr::m_strtablen )
		{
			lstrcpyn(outname, COpenAr::m_strtab+off, MAX_PATH);
			char* p; for(p=outname;*p && *p!='\n';p=CharNext(p)); *p='\0';
			int n=lstrlen(outname); if(n>0&&outname[n-1]=='/')outname[n-1]='\0';
		}
		else
			wsprintf(outname,"%.16s",buf);
	}
	else
	{
		int i;
		for( i=0; buf[i]!=' ' && i!=16; i++ )
			outname[i] = (char)buf[i];
		outname[i]='\0';
		// strip trailing '/' of the short "name/" form
		if( i>0 && outname[i-1]=='/' ) outname[i-1]='\0';
	}

	fseek( ar, nextpos, SEEK_SET );

	lstrcpy(lpSubInfo->szFileName,outname);
	lpSubInfo->dwOriginalSize=origsiz;
	lstrcpy(lpSubInfo->szMode,"stored");

	struct tm* time=localtime((long*)&date);
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
		::FileTimeToDosDateTime(&ft,&lpSubInfo->wDate,&lpSubInfo->wTime);
	}
	return true;
}