#include "stdafx.h"
#include "OpenJak.h"

#define sizeof_JakHeader 43

//Jak32.DLLとは実装が違います。
//FindFirstで結合後のファイルの情報を返します。
bool COpenJak::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenJak::common_buf;

	int hpos = FindHeader(buf);
	if( hpos==-1 )
		return false;

	JakHeader* pHead = (JakHeader*)(buf+hpos);

	int fnln = 
		MAX_PATH > pHead->FileNameLen ? pHead->FileNameLen : MAX_PATH;
	memcpy( lpSubInfo->szFileName, buf+hpos+sizeof_JakHeader, fnln );
	lpSubInfo->szFileName[ fnln ] = '\0';

	FileTimeToDosDateTime(&pHead->FileTime,&lpSubInfo->wDate,&lpSubInfo->wTime);

	lpSubInfo->dwOriginalSize = pHead->FileSize;
	COpenJak::OriginalSize = pHead->FileSize;

	for(int j = 0;COpenJak::szArcFileName[j]!=0;j++);
	for(;COpenJak::szArcFileName[j]!='\\';j--);
	lstrcpy(COpenJak::basedir,COpenJak::szArcFileName);
	COpenJak::basedir[j+1]=0;
	strcpy( bodyname,&COpenJak::szArcFileName[j+1]);

	//lstrcpy(lpSubInfo->szMode,"");

	char extn[18];

	for(;COpenJak::szArcFileName[j]!='.';j++);
	lstrcpy(extn,&COpenJak::szArcFileName[j+1]);

	sprintf( COpenJak::tmp, "%d.%s", pHead->No, extn );

	char* p;
	for( p=bodyname; *p!=0; p=CharNext(p) )
		if( 0==strcmpi(p,tmp) )
			break;
	*p='\0';
	return true;
}
//Jak32.DLLとは実装が違います。
//FindNextで分割されたファイルの情報を返します。
bool COpenJak::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	static int i=1;
	static int findfilesize = 0;

	if (findfilesize>=COpenJak::OriginalSize){
		findfilesize = 0;
		i=1;
		return false;
	}

	char jackfile[MAX_PATH*2];
	strcpy( jackfile,basedir );
	strcat( jackfile,bodyname );
	sprintf( tmp,"%d.jak",i );
	strcat( jackfile,tmp );

	WIN32_FIND_DATA find;
	HANDLE h = FindFirstFile( jackfile,&find );
	FindClose( h );
	if( h==INVALID_HANDLE_VALUE )
	{
		strcpy( lpSubInfo->szFileName,"ファイルが足りません");
		lstrcpy(lpSubInfo->szMode,"  ×  ");
		lpSubInfo->dwCompressedSize=0;
		lpSubInfo->wDate=NULL;
		lpSubInfo->wTime=NULL;
		findfilesize=COpenJak::OriginalSize+1;
		return true;
	}

	strcpy( lpSubInfo->szFileName,find.cFileName);
	lpSubInfo->dwCompressedSize=find.nFileSizeLow;
	lstrcpy(lpSubInfo->szMode,"  ○  ");
	findfilesize+=find.nFileSizeLow;
	//lstrcpy(lpSubInfo->szMode,"");

	i++;
	return true;
}

int COpenJak::FindHeader( const BYTE* hdr)
{
	DWORD size = (DWORD)filelength(fileno(COpenJak::fp));
	static const char Magic[]="This_Is_Jack_File";

	if( size<sizeof_JakHeader )
		return -1;

	if( 0==strcmp((char*)hdr,Magic) )
	{
		if( sizeof_JakHeader+((JakHeader*)hdr)->FileNameLen<size )
			return 0;
	}
	else if( hdr[0]=='M' && hdr[1]=='Z' )
	{
		DWORD prev = 0xffffffff;
		for( DWORD i=0; i<size-sizeof_JakHeader; i++ )
		{
			if( hdr[i]!='T' )continue;
			if( hdr[i+1]!='h' )continue;
			if( hdr[(++i)+1]!='i' )continue;
			if( hdr[(++i)+1]!='s' )continue;
			if( hdr[(++i)+1]!='_' )continue;
			if( 0!=strcmp((char*)hdr+(++i)+1,Magic+5) )continue;

			// スタブ内の文字列に引っかかることがあるので、
			// 二個"This_Is_..."があるときは一個Skip
			if( prev==0xffffffff )
				prev = (i-4);
			else
				return (i-4);
		}
		if( prev!=0xffffffff )
			return prev;
	}
	return -1;
}