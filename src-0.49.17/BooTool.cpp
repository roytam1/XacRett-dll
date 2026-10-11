
#include "stdafx.h"
#include "BooTool.h"

bool CBooTool::Check( const char* fname, unsigned long fsize )
{
	return 0==stricmp( kiutil::pathExt(fname),"boo" );
}

#define fixchr(x) ((x)-'0')
#define NULLCHR	fixchr('~')

bool CBooTool::Extract( const char* fname,const char* ddir )
{
	char* str=(char*)CTool::common_buf;
//--------------------------------------------------------------------//

	FILE* fp=fopen(fname,"rb");
	if( fp==NULL )return false;
	int ct=0;

	// 保存ファイルネーム作成
	char outname[MAX_PATH];
	kiutil::getline(outname,sizeof(outname),fp);

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(outname)){fclose(fp); return true;}//追加 By Bonty

	FILE* op=fopen(kiutil::pathMake(outname),"wb");
	if( op==NULL ){fclose(fp);return false;}

	while( kiutil::getline(str,XACR_BUFSIZE,fp) )
	{
		unsigned int index = 0;
		while( str[index]!='\0' )
		{
			if( fixchr(str[index]) == NULLCHR)
			{
				index++;
				int rptcnt = fixchr(str[index]);
				for(int i=0; i<rptcnt; i++ )
					fputc('\0',op);
				index++;
			}
			else
			{
				int a = fixchr(str[index++]);
				int b = fixchr(str[index++]);
				int c = fixchr(str[index++]);
				int d = fixchr(str[index++]);

				fputc( ((a<<2)+(b>>4))&0xff, op);
				fputc( ((b<<4)+(c>>2))&0xff, op);
				fputc( ((c<<6)+ d    )&0xff, op);
			}
		}
	}

	fclose(op);
	fclose(fp);
	return true;
}
