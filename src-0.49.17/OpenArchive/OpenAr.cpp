#include "stdafx.h"
#include "OpenAr.h"
//–¢Š®¬!!
bool COpenAr::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
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

	// ƒwƒbƒ_“Ç‚İ‚İ
	if( 60!=fread( buf, 1, 60, ar ) || buf[58]!='`' || buf[59]!='\n' )
		return false;

	// LongFileName
	if( buf[0]=='/' && buf[1]=='/' && buf[2]==' ' )
	{
		long namelen = strtol( (char*)buf+48, NULL,10 );
		long vr = namelen>MAX_PATH-1 ? MAX_PATH-1 : namelen;
		namelen -= fread( outname, 1, vr, ar );
		outname[vr]='\0';
		fseek( ar, namelen, SEEK_CUR );
	}
	else
	{
		int i;
		for( i=0; buf[i]!=' ' && i!=16; i++ )
			outname[i] = (char)buf[i];
		outname[i]='\0';
	}

	date = (unsigned long)strtol( (char*)buf+16, NULL,10 );
	origsiz =(unsigned long)strtol( (char*)buf+48, NULL,10 );
	unsigned long nextpos = ftell(ar) + origsiz + (origsiz&1);

	fseek( ar, nextpos, SEEK_SET );

	lstrcpy(lpSubInfo->szFileName,outname);
	lpSubInfo->dwOriginalSize=origsiz;

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