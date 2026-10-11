
#include "stdafx.h"
#include "MimeTool.h"

bool CMimeTool::DoHeader( FILE* in,char* name,int count )
{
	*name=0;

	char t,str[5000];
	int dmy,mode=0;//0=DEF,1=InCT,2=InCD,3=InTE
	bool bB64Found=false;

	while( kiutil::getline(str,sizeof(str),in) && count!=0 )
	{
// sharutils形式…
		if( str==strstr(str,"begin-base64") )
		{bB64Found=true;sscanf(str,"begin-base64 %d %s",&dmy,name);break;}
///////////////////
		count--;
		if( bB64Found && *str=='\0' )
			break;

		t=str[13];str[13]=0;
		if( 0==stricmp(str,"Content-Type:") )mode=1;
		str[13]=t;

		t=str[20];str[20]=0;
		if( 0==stricmp(str,"Content-Disposition:") )mode=2;
		str[20]=t;

		t=str[26];str[26]=0;
		if( 0==stricmp(str,"Content-Transfer-Encoding:") )mode=3;
		str[26]=t;

		switch( mode )
		{
		case 1:
		case 2:{
			kiutil::getline(str,sizeof(str),in);//追加 By Bonty
			char *p,*e;
			for( p=str; *p!=0; p++ )
			{
				if( *p!='n' && *p!='N' )continue;
				if( *(p+1)!='a' && *(p+1)!='A' )continue;
				t=p[5];p[5]=0;
				if( 0!=stricmp(p,"name=") ){p[5]=t;continue;}
				p[5]=t;
				break;
			}
			if( p!=NULL )
			{
				p+=5;
				while(*p==' ')p++;
				if( *(p++)!='\"' )
					break;
				for( e=p; *e!='\0' && *e!='\"'; e=CharNext(e) );
				*e=0;
				strncpy( name,p,MAX_PATH );
			}}break;
		case 3:{
			for( char* p=str; *p!=0; p++ )
			{
				if( *p!='b' && *p!='B' )continue;
				if( *(p+1)!='a' && *(p+1)!='A' )continue;
				t=p[6];p[6]=0;
				if( 0!=stricmp(p,"base64") ){p[6]=t;continue;}
				p[6]=t;bB64Found=true;
				break;
			}}break;
		}

		if( mode!=0 && str[strlen(str)-2]!=';' )
			mode=0;
	}

	return bB64Found;
}

static BYTE table[256];

void CMimeTool::DoData( FILE* in,FILE* out )
{
	char* str = (char*)CTool::common_buf;
//--------------------------------------------------------------------//

	char tmp[3];
	DWORD outbuf=0,i=0;

	while( kiutil::getline(str,XACR_BUFSIZE,in) )
	{
		if( str==strstr(str,"--") )
			break;

		char* p;
		for( p=str; *p!='\0'; p++ )
		{
			if( *p=='=' )
			{
				tmp[0] = (char)((outbuf>>16));
				tmp[1] = (char)((outbuf>>8)&0xff);
				tmp[2] = (char)((outbuf)&0xff);
					 if( i==2 )fwrite( tmp,1,1,out );
				else if( i==3 )fwrite( tmp,1,2,out );
				break;
			}

			if( 0xff==table[*p] )
				continue;

			outbuf |= (((DWORD)table[*p])<<(18-6*i));
			i++;
			if( i==4 )
			{
				tmp[0] = (char)((outbuf>>16));
				tmp[1] = (char)((outbuf>>8)&0xff);
				tmp[2] = (char)((outbuf)&0xff);
				fwrite( tmp,1,3,out );
				outbuf=0;
				i=0;
			}
		}

		if( *p!='\0' )
			break;
	}
}

bool CMimeTool::Check( const char* fname, unsigned long fsize )
{
	// 拡張子b64じゃなかったら却下
	/*では無い^^; ByBonty
	const char* ext = kiutil::pathExt(fname);
	if( 0!=stricmp(ext,"b64") && 0!=stricmp(ext,"mim")
		&& 0!=stricmp(ext,"mime") )
		return false;
	*/
	// ヘッダを読んでみる
	FILE* fp=fopen(fname,"rb");
	if( fp==NULL )return false;
	char dummy[MAX_PATH];
	bool ans=DoHeader(fp,dummy,100);
	fclose(fp);
	return ans;
}

bool CMimeTool::Extract( const char* fname,const char* ddir )
{
	FILE* fp=fopen(fname,"rb");
	if( fp==NULL )return false;
	char outname[MAX_PATH]={0};

	// テーブル初期化
	int i;
	for( i=0; i!=256; i++ )table[i] = 0xff;
	for( i=0; i!=26; i++ )table['A'+i] = i;
	for( i=0; i!=26; i++ )table['a'+i] = 26+i;
	for( i=0; i!=10; i++ )table['0'+i] = 52+i;
	table['+']=62;	table['/']=63;

	while( DoHeader(fp,outname) )
	{
		if( *outname==0 )
		{
			kiutil::getOriginalName( outname,fname,
				0==stricmp(kiutil::pathExt(fname),"b64") ? NULL : "b64dec" );
		}
		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(outname))//追加 By Bonty
		{
			skip(fp);
			continue;
		}
		FILE* out=fopen(outname,"wb");
		DoData(fp,out);
		fclose(out);
	}

	fclose(fp);
	return true;
}

void CMimeTool::skip(FILE* fp)
{
char* str = (char*)CTool::common_buf;
//--------------------------------------------------------------------//


	while( kiutil::getline(str,XACR_BUFSIZE,fp ) )
	{
		if( str==strstr(str,"--") )
			break;
	}

}