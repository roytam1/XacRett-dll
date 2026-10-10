
#include "stdafx.h"
#include "JakTool.h"

#define sizeof_JakHeader 43
struct JakHeader
{
	char Magic[18];		// This_Is_Jack_File\0
	char dummy1[2];
	DWORD No;			// 切り裂きファイルの連番( 1～ )
	DWORD FileNameLen;	// ファイル名の長さ
	DWORD FileSize;		// 切り裂く前のファイルサイズ
	FILETIME FileTime;	// ファイルの更新日付
	char dummy2[3];
//	char* OrigFileName;	// ファイル名 ( NULL 終端なし )
};

int CJakTool::FindHeader( const BYTE* hdr,DWORD size )
{
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

bool CJakTool::Check(  const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//
	char szFileName[MAX_PATH];

	int hpos = FindHeader(hdr,siz>32768 ? 32768 : siz);
	if( hpos==-1 )
		return false;

	JakHeader* pHead = (JakHeader*)(hdr+hpos);

	int fnln = 
		MAX_PATH > pHead->FileNameLen ? pHead->FileNameLen : MAX_PATH;
	memcpy( szFileName, hdr+hpos+sizeof_JakHeader, fnln );
	szFileName[ fnln ] = '\0';
	return (strlen(szFileName));
}

bool CJakTool::Extract( const char* fname, const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	int y,d;
	kiutil::pathSplit( fname,&y,&d );
	char basedir[MAX_PATH+1]={'\0'};
	char bodyname[MAX_PATH+1];
	char outname[MAX_PATH+1];
	if( y!=-1 )
		strncpy( basedir,fname,y+1 );
	strcpy( bodyname,fname+y+1 );

// とりあえず、ファイル名に頼って検索するバージョン

	// ヘッダを探し出す
	FILE* jak = fopen( fname,"rb" );
	if( !jak )
		return false;
	int hpos = FindHeader( buf,fread( buf,1,32768,jak ) );
	fclose( jak );
	if( hpos==-1 )
		return false;
	JakHeader* pHead = (JakHeader*)(buf+hpos);

	// ヘッダ読みとり
	int fnln = 
		MAX_PATH > pHead->FileNameLen ? pHead->FileNameLen : MAX_PATH;
	memcpy( outname, buf+hpos+sizeof_JakHeader, fnln );
	outname[ fnln ] = '\0';
	FILETIME updttime = pHead->FileTime;

	// Jackファイル名から番号以降を除いた部分を保存しておく
	char tmp[18];
	sprintf( tmp, "%d.%s", pHead->No, kiutil::pathExt(fname) );
	char* p;
	for( p=bodyname; *p!=0; p=CharNext(p) )
		if( 0==strcmpi(p,tmp) )
			break;
	*p='\0';

	// 復元開始！
	FILE* out = fopen( kiutil::pathMake(outname),"wb" );
	if( !out )
		return false;

	for( int i=1; i!=(1<<31); i++ )
	{
		// i 番目のファイル名
		char jackfile[MAX_PATH*2];
		strcpy( jackfile,basedir );
		strcat( jackfile,bodyname );
		sprintf( tmp,"%d.jak",i );
		strcat( jackfile,tmp );

		CheckStr(jackfile,false);
		// i 番目のファイルを開く
		jak = fopen( jackfile,"rb" );
		if( !jak )
		{
			strcpy( jackfile,basedir );
			strcat( jackfile,bodyname );
			sprintf( tmp,"%d.exe",i );
			strcat( jackfile,tmp );
			if( !(jak=fopen( jackfile,"rb" )) )
				break;
		}
		hpos = FindHeader( buf,fread( buf,1,32768,jak ) );
		if( hpos==-1 )
		{
			fclose( jak );
			break;
		}

		// ファイル名とヘッダとの整合性確認
		pHead = (JakHeader*)(buf+hpos);
		if( pHead->No!=(DWORD)i )
		{
			fclose( jak );
			break;
		}
		char str[MAX_PATH];
		fnln = 
			MAX_PATH > pHead->FileNameLen ? pHead->FileNameLen : MAX_PATH;
		memcpy( str, buf+hpos+sizeof_JakHeader, fnln );
		str[ fnln ] = '\0';
		if( 0!=stricmp( outname,str ) )
		{
			fclose( jak );
			break;
		}

		// 書き出す
		int len;
		fseek( jak,hpos+sizeof_JakHeader+pHead->FileNameLen,SEEK_SET );
		while( -1!=(len=fread( buf,1,XACR_BUFSIZE,jak )) && 0!=len )
			fwrite( buf,1,len,out );

		fclose( jak );
	}

	fclose( out );
	kiutil::timeSet( outname, &updttime );
	return true;
}
