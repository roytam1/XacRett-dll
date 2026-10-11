
#include "stdafx.h"
#include "AtoBTool.h"

bool CAtoBTool::Check( const char* fname, unsigned long fsize )
{
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	const unsigned char *ptr = CTool::common_buf,
						*end = CTool::common_buf + siz - 12;
//--------------------------------------------------------------------//

	// xbtoa Begin な行があるか？
	bool first=true;
	for( ; ptr<end; ptr++ )
	{
		if( *ptr=='\0' )
			break;

		if( first )
			first=false, ptr--;
		else if( *ptr!='\r' && *ptr!='\n' )
			continue;

		if( ptr[1]!='x' || ptr[2]!='b'
		 || ptr[3]!='t' || ptr[4]!='o' || ptr[5]!='a' )
			continue;

		if( 0==strncmp( (char*)ptr+6," Begin",6 )
		 && (ptr[12]=='\r' || ptr[12]=='\n') )
			return true;
	}
	return false;
}

#define START_LETTER ('!')
#define DE(c) ((c)-START_LETTER)
#define times85(x) ((((((x<<2)+x)<<2)+x)<<2)+x)

bool CAtoBTool::Extract( const char* fname,const char* ddir )
{
	char* str=(char*)CTool::common_buf;
//--------------------------------------------------------------------//

	FILE* fp=fopen(fname,"rb");
	if( fp==NULL )return false;

	// Begin 行を探す
	while( true )
	{
		kiutil::getline(str,XACR_BUFSIZE,fp);
		if( 0==strcmp(str,"xbtoa Begin") )break;
		if( feof(fp) ){fclose(fp);return false;}
	}

	// 保存ファイルネーム作成
	char outname[MAX_PATH];
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"b2a") ? NULL : "a2b" );

	if (m_Endflg) return true;//追加 By Bonty
	if (!CheckStr(outname)){fclose(fp); return true;}//追加 By Bonty

	// 開く
	FILE* op=fopen(outname,"wb");
	if( op==NULL ){fclose(fp);return false;}

	// decode
	int c,dword=0,bcount=0;
	while( (c=fgetc(fp))!=EOF )
	{
		if( c=='x' )
			break;
		else if( (c<START_LETTER) || ((START_LETTER+85)<=c) && c!='z' )
			continue;

		if( bcount==5 )
		{
			fputc( (dword>>24)&0xff,op );
			fputc( (dword>>16)&0xff,op );
			fputc( (dword>>8) &0xff,op );
			fputc(  dword     &0xff,op );
			dword = 0;
			bcount = 0;
		}

		if( c=='z' )
			fputc(0,op),fputc(0,op),fputc(0,op),fputc(0,op);
		else
		{
			dword = times85(dword) + DE(c);
			bcount++;
		}
	}

	if( bcount==5 )
	{
		long size=-1;
		fscanf( fp,"btoa End N %ld ",&size );
		if( size!=-1 )
		{
			switch( size&3 )
			{
			case 1:
				fputc( (dword>>24)&0xff,op );
				break;
			case 2:
				fputc( (dword>>24)&0xff,op );
				fputc( (dword>>16)&0xff,op );
				break;
			case 3:
				fputc( (dword>>24)&0xff,op );
				fputc( (dword>>16)&0xff,op );
				fputc( (dword>>8)&0xff,op );
				break;
			case 0:
				fputc( (dword>>24)&0xff,op );
				fputc( (dword>>16)&0xff,op );
				fputc( (dword>>8)&0xff,op );
				fputc(  dword&0xff,op );
				break;
			}
		}
	}

	fclose(op);
	fclose(fp);
	return true;
}
