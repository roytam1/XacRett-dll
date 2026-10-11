
#include "stdafx.h"
#include "YncTool.h"

bool CYncTool::Check( const char* fname, unsigned long fsize )
{
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	const unsigned char *ptr = CTool::common_buf,
						*end = CTool::common_buf + siz - 6;
//--------------------------------------------------------------------//

	// "=ybegin " で始まる行があるか？
	bool first=true;
	for( ; ptr<end-9; ptr++ )
	{
		if( *ptr=='\0' )
			break;
		if( first )
			first=false, ptr--;
		else if( *ptr!='\r' && *ptr!='\n' )
			continue;

		if( ptr[1]=='=' && ptr[2]=='y' && ptr[3]=='b'
		 && ptr[4]=='e' && ptr[5]=='g' && ptr[6]=='i'
		 && ptr[7]=='n' && ptr[8]==' ' )
		{
			// その行に " name=" はあるか？
			int step = 0;
			const unsigned char* p = ptr+8;
			while( p<end && *p!='\r' && *p!='\n' )
			{
				if( *p == " name="[step] )
					step++;
				else
					step=0;
				if( step==6 )
					return true;
				++p;
			}
		}
	}
	return false;
}

bool CYncTool::Extract( const char* fname, const char* ddir )
{
	char* str=(char*)CTool::common_buf;
//--------------------------------------------------------------------//

	// ファイルを開く
	FILE* fp=fopen(fname,"rb");
	if( !fp ) return false;

	while( !feof(fp) )
	{
		// ファイル名読みとり
		char outname[5000];
		while( !feof(fp) )
		{
			kiutil::getline( str,XACR_BUFSIZE,fp );
			if( 0==strncmp(str,"=ybegin ",8) )
				if( char* nm=strstr(str," name=") )
				{
					strcpy( outname, nm+6 );
					for( int sp=strlen(outname)-1;
						sp>=0 && outname[sp]==' '; --sp )
						outname[sp] = '\0';
					break;
				}
		}
		if( feof(fp) || outname[0]=='\0' ) break;

		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(outname))//追加 By Bonty
		{
			continue;
		}
		// 出力先ファイルを作る
		FILE* op=fopen(kiutil::pathMake(outname),"wb");
		if( !op ){fclose(fp);return false;}

		// デコード
		bool escape=false;
		while( !feof(fp) )
		{
			kiutil::getline( str,XACR_BUFSIZE,fp );
			if( 0==strncmp(str,"=yend",5) )
				break;
			for( char* p=str; *p; ++p )
				if( escape )
				{
					fputc( *p-42-64, op );
					escape = false;
				}
				else if( *p == '=' )
				{
					escape = true;
				}
				else
				{
					fputc( *p-42, op );
				}
		}
		fclose(op);
	}

	// 終了
	fclose(fp);
	return true;
}
