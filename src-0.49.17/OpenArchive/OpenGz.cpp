#include "stdafx.h"
#include "OpenGz.h"

bool COpenGz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenGz::common_buf;
	GzHeader head;

	if( 1 != fread( &head,10,1,COpenGz::fp ) )
		return false;

	if( head.Flag&(1<<3) )//ƒtƒ@ƒCƒ‹–¼‚ª“ü‚Á‚Ä‚¢‚é‚Æ‚«
	{
		if( head.Flag&(1<<1) )fgetc(COpenGz::fp),fgetc(COpenGz::fp);
		if( head.Flag&(1<<2) )
		{WORD splen=0;fread(&splen,2,1,COpenGz::fp);fread(buf,1,splen,COpenGz::fp);}
		int c;
		for( int i=0; i!=65536; i++ )
		{
			c=fgetc(fp);
			if( c==-1 || c==0 ){buf[i]=0;break;}
			buf[i]=c;
		}
		lstrcpy(lpSubInfo->szFileName,(LPCSTR)buf);
	}
	else
	{
		/* LongFileNameŽæ“¾
		WIN32_FIND_DATA find;
		HANDLE h = FindFirstFile( COpenGz::szArcFileName,&find );
		if( h==INVALID_HANDLE_VALUE )
		{
			return false;
		}
		FindClose( h );
		lstrcpy(lpSubInfo->szFileName,find.cFileName );*/
		char outname[MAX_PATH];
		kiutil::getOriginalName( outname,COpenGz::szArcFileName,
			0==stricmp(kiutil::pathExt(COpenGz::szArcFileName),"gz") ? NULL : "ungzip" );
		lstrcpy(lpSubInfo->szFileName,outname );

		for (int i =0 ; lpSubInfo->szFileName[i]!=0;i++);
		if (i <FNAME_MAX32-7)
			lstrcpy(&lpSubInfo->szFileName[i],".ungzip");
		else if (i <FNAME_MAX32-1)
			lstrcpy(&lpSubInfo->szFileName[i],"#");
	}
	if (head.Update!=0){
		struct tm* time=localtime((long*)&head.Update);
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
	}
	fseek(COpenGz::fp,filelength(fileno(COpenGz::fp))-8,SEEK_SET );
	fread(&lpSubInfo->dwCRC,4,1,COpenGz::fp);
	fread(&lpSubInfo->dwOriginalSize,4,1,COpenGz::fp);
	lpSubInfo->dwCompressedSize=(DWORD)filelength(fileno(COpenGz::fp))-10;
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	//lstrcpy(lpSubInfo->szMode,"");
	return true;
}

bool COpenGz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}
