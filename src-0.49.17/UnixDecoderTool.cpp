
#include "stdafx.h"
#include "UnixDecoderTool.h"

bool CUnixDecoderTool::Check( const char* fname, unsigned long fsize )
{
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	const unsigned char *ptr = CTool::common_buf,
						*end = CTool::common_buf + siz - 6;
//--------------------------------------------------------------------//

	// 拡張子 uue/xxe じゃなかったら却下
	const char* ext = kiutil::pathExt( fname );
	if( (ext[0]!= x && ext[0]!= X )
	 || (ext[1]!= x && ext[1]!= X )
	 || (ext[2]!='e'&& ext[2]!='E' && ext[2]!=0 ) )
		return false;

	// begin で始まる行があるか？
	bool first=true;
	for( ; ptr<end-6; ptr++ )
	{
		if( *ptr=='\0' )
			break;
		if( first )
			first=false, ptr--;
		else if( *ptr!='\r' && *ptr!='\n' )
			continue;

		if( ptr[1]=='b' && ptr[2]=='e'
		 && ptr[3]=='g' && ptr[4]=='i' && ptr[5]=='n' )
			return true;
	}
	return false;
}

#define DEC(c) (table[(c)&0x7f])

bool CUnixDecoderTool::Extract( const char* fname,const char* ddir )
{
	char* str=(char*)CTool::common_buf;
//--------------------------------------------------------------------//

	// ヘッダを読む
	FILE* fp=fopen(fname,"rb");
	if( !fp ) return false;
	char outname[5000]="";
	while( !feof(fp) )
	{
		kiutil::getline( str,XACR_BUFSIZE,fp );
		if( str==strstr(str,"begin") )
		{
			int dummy;
			sscanf(str,"begin %o %s",&dummy,outname);
			break;
		}
	}
	if( feof(fp) ){fclose(fp);return false;}

	// 出力先ファイルを作る
	if( *outname==0 )
	{
		char newext[] = {x,x,'d','e','c','o','d','e'};

		const char* ext = kiutil::pathExt( fname );
		if( (ext[0]!=x && ext[0]!=X)
		 || (ext[1]!=x && ext[1]!=X) )
			kiutil::getOriginalName( outname,fname,newext );
		else
			kiutil::getOriginalName( outname,fname,NULL );
	}

	if (m_Endflg)//追加 By Bonty
	{
		fclose(fp);
		return true;
	}
	//ワイルドカード処理
	if (!CheckStr(outname))//追加 By Bonty
	{
		// close
		fclose(fp);
		return true;
	}

	kiutil::pathMake83(outname);
	FILE* op=fopen(outname,"wb");
	if( !op ){fclose(fp);return false;}

	// デコード
	InitTable();

	while( !feof(fp) )
	{
		kiutil::getline( str,XACR_BUFSIZE,fp );
		char* p=str;

		int n = DEC(*p);
		if( n==0 )
			break;

		for( ++p; n>0; p+=4,n-=3 )
		{
			fputc( DEC(p[0])<<2 | DEC(p[1])>>4 , op );
			if( n==1 ) continue;
			fputc( DEC(p[1])<<4 | DEC(p[2])>>2 , op );
			if( n==2 ) continue;
			fputc( DEC(p[2])<<6 | DEC(p[3])    , op );
		}
	}

	// 終了
	fclose(op);
	fclose(fp);
	return true;
}
