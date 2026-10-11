
#include "stdafx.h"
#include "MSCompressTool.h"
#include <lzexpand.h>
#include "XacRett.h"

bool CMSCompressTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<14 )
		return false;

	const BYTE h_v5[9]={ 0x53,0x5A,0x44,0x44,0x88,0xF0,0x27,0x33,0x41 };
	const BYTE h_v6[9]={ 0x4B,0x57,0x41,0x4A,0x88,0xF0,0x27,0xD1,0x03 };

	if( hdr[0]==h_v6[0] )
	{
		// version 6 header。未確認(^^;
		for( int i=1; i!=9; i++ )
			if( hdr[i]!=h_v6[i] )
				return false;
	}
	else
	{
		// version 5 header
		// SZDD芋'3A[char ext_3rd][DWORD original_size]
		for( int i=0; i!=9; i++ )
			if( hdr[i]!=h_v5[i] )
				return false;

		// ↓原理的に、１６倍という圧縮率は出ない。
		if( (fsize<<4 ) < (unsigned)
			(hdr[10]|(hdr[11]<<8)|(hdr[12]<<16)|(hdr[13]<<24)) )
			return false;
	}

	return true;
}

bool CMSCompressTool::Extract( const char* fname, const char* ddir )
{
	// ヘッダ読み込み
	FILE* msc = fopen( fname,"rb" );
	unsigned char head[10];
	if( !msc )
		return false;
	if( 10!=fread( head,1,10,msc ) )
	{
		fclose( msc );
		return false;
	}
	fclose( msc );

	// ファイル名復元までの長い道のり(笑)
	char oname[MAX_PATH];
	strcpy( oname, kiutil::pathName(fname) );

	// 最後が '_' なら消しておく。
	bool bIs_=false;
	char* p;
	for( p=oname; *p!='\0'; p=CharNext(p) ) 
		bIs_ = (*p=='_');
	if( bIs_ )
		*(p-1)='\0';

	// ver 5 ヘッダなら３文字目を付加
	if( head[0]=='S' )
	{
		char ext[2]={ (char)head[9],'\0' };
		strcat( oname,ext );
	}

	// 拡張子自動補完
	char* ext = const_cast<char*>( kiutil::pathExt( oname ) );
	if( strlen(ext)==2 )
	{
		CharLower(ext);
			 if( 0==strcmp(ext,"ex") )strcat(ext,"e");
		else if( 0==strcmp(ext,"co") )strcat(ext,"m");
		else if( 0==strcmp(ext,"sc") )strcat(ext,"r");

		else if( 0==strcmp(ext,"dl") )strcat(ext,"l");
		else if( 0==strcmp(ext,"oc") )strcat(ext,"x");
		else if( 0==strcmp(ext,"dr") )strcat(ext,"v");
		else if( 0==strcmp(ext,"vx") )strcat(ext,"d");
		else if( 0==strcmp(ext,"38") )strcat(ext,"6");
		else if( 0==strcmp(ext,"sy") )strcat(ext,"s");
		else if( 0==strcmp(ext,"cp") )strcat(ext,"l");
		else if( 0==strcmp(ext,"li") )strcat(ext,"b");
		else if( 0==strcmp(ext,"tt") )strcat(ext,"f");

		else if( 0==strcmp(ext,"ch") )strcat(ext,"m");
		else if( 0==strcmp(ext,"hl") )strcat(ext,"p");
		else if( 0==strcmp(ext,"cn") )strcat(ext,"t");
		else if( 0==strcmp(ext,"da") )strcat(ext,"t");

		else if( 0==strcmp(ext,"tx") )strcat(ext,"t");
		else if( 0==strcmp(ext,"wr") )strcat(ext,"i");
		else if( 0==strcmp(ext,"wa") )strcat(ext,"v");
		else if( 0==strcmp(ext,"mi") )strcat(ext,"d");
		else if( 0==strcmp(ext,"rm") )strcat(ext,"i");
		else if( 0==strcmp(ext,"bm") )strcat(ext,"p");
		else if( 0==strcmp(ext,"rl") )strcat(ext,"e");
		else if( 0==strcmp(ext,"cu") )strcat(ext,"r");
		else if( 0==strcmp(ext,"do") )strcat(ext,"c");
		else if( 0==strcmp(ext,"ic") )strcat(ext,"o");
		else if( 0==strcmp(ext,"re") )strcat(ext,"g");
		else if( 0==strcmp(ext,"rt") )strcat(ext,"f");
		else if( 0==strcmp(ext,"ht") )strcat(ext,"m");
		else						  strcat(ext,"#");
	}
	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(oname))//追加 By Bonty
	{
		return true;
	}	
	if(arcproc)//SetOwnerWindowのための処理
	{
		static EXTRACTINGINFO	info;
		::memset(&info,0,sizeof(EXTRACTINGINFO));
		lstrcpy(info.szSourceFileName,oname);
		if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
			return true;
	}
	// 解凍
	OFSTRUCT of;
	of.cBytes = sizeof(of);
	int FROM = LZOpenFile( const_cast<char*>(fname),&of,OF_READ );
	int  TO  = LZOpenFile( oname,&of,OF_WRITE|OF_CREATE );
	LZCopy( FROM , TO );
	LZClose( FROM );
	LZClose( TO );

	//日付を書庫に合わせる
	WIN32_FIND_DATA find_f;
	HANDLE h = FindFirstFile( fname,&find_f );
	if( h==INVALID_HANDLE_VALUE )
		return false;
	FindClose( h );
	kiutil::timeSet(oname,&find_f.ftLastWriteTime);

	return true;
}

