
#include "stdafx.h"
#include "FishTool.h"

static const char fish[]=
"\x88\xb1\x88\xbc\x88\xf1\x89V\x8a\x82\x8a\x8f\x8b\x9b\x8c~\x8c\xef\x8d\xf8\x8eI\x8eL\x91\xe2\x92L\x95h\x95\xa9\x96\x8e\x96\x90\x98k\xe9\xb5\xe9\xb6\xe9\xb7\xe9\xb8\xe9\xb9\xe9\xba\xe9\xbb\xe9\xbc\xe9\xbd\xe9\xbe\xe9\xbf\xe9\xc0\xe9\xc1\xe9\xc2\xe9\xc3\xe9\xc4\xe9\xc5\xe9\xc6\xe9\xc7\xe9\xc8\xe9\xc9\xe9\xca\xe9\xcb\xe9\xcc\xe9\xcd\xe9\xce\xe9\xcf\xe9\xd0\xe9\xd1\xe9\xd2\xe9\xd3\xe9\xd4\xe9\xd5\xe9\xd6\xe9\xd7\xe9\xd8\xe9\xd9\xe9\xda\xe9\xdb\xe9\xdc\xe9\xdd\xe9\xde\xe9\xdf\xe9\xe0\xe9\xe1\xe9\xe2\xe9\xe3\xe9\xe4\xe9\xe5\xe9\xe6\xe9\xe7";

static const char nish[]=
"\x81\x8f\x81\x90\x81\x89\x81\x8a\x81`\x81[\x81""E\x83""A\x83""C\x83""E\x83G\x83I\x83J\x83L\x83N\x83P\x83R\x83T\x83V\x83X\x83Z\x83\x5c\x83^\x83`\x83""c\x83""e\x83g\x83i\x83j\x83k\x83l\x83m\x83n\x83q\x83t\x83w\x83z\x83}\x83~\x83\x80\x83\x81\x83\x82\x83\x84\x83\x86\x83\x88\x83\x89\x83\x8a\x83\x8b\x83\x8c\x83\x8d\x83\x8f\x83\x92\x83\x93\x83@\x83""B\x83""D\x83""F\x83H\x83""b\x83\x83\x83\x85\x83\x87\x81J\x81K";

static int FishJstrcspn(int c, const char *js)
{
	char c1 = (char)(c&0xff), c2 = (char)(c>>8);
	for( int i=0; i!=128; i+=2 )
		if( c1==js[i] && c2==js[i+1] )
			return i;
	return -1;
}

bool CFishTool::Header( const char* str, int* mhd, char* outname )
{
	if( 0!=strncmp( str,"begin ",6 ) )
		return false;

	const char* fnm = NULL;
	if( 0==strncmp( str+6,"86~ ",4 ) )
		*mhd = 86, fnm = str+10;
	else if( 0==strncmp( str+6,"24~ ",4 ) )
		*mhd = 24, fnm = str+10;
	else if( 0==strncmp( str+6,"4649 ",5 ) )
		*mhd = 4649, fnm = str+11;
	if( !fnm || !(*fnm) || *fnm=='\n' )
		return false;

	while( *fnm!='\0' && *fnm!='\n' )
		*outname++ = *fnm++;
	*outname = '\0';
	return true;
}

void CFishTool::decode86( unsigned char* str, FILE* out )
{
	// 行頭におさかな文字以外があったらスキップする
	if( -1==FishJstrcspn( str[0]|(str[1]<<8), fish ) )
		return;

	int c1,c2,c3,c4;
	for( unsigned char* s=str; *s!='\n' && *s!='\0'; )
	{
		c1 = FishJstrcspn( s[0]|(s[1]<<8), fish );
		s += 2;
		if( -1==c1 )
			return;
		c2 = FishJstrcspn( s[0]|(s[1]<<8), fish );
		s += 2;
		if( -1==c2 )
			return;

		if( *s=='\n' || *s=='\0' )
			return;
		else
		{
			c3 = FishJstrcspn( s[0]|(s[1]<<8), fish );
			s += 2;
			if( -1==c3 )
				return;
		}

		if( *s=='\n' || *s=='\0' )
			c4 = -1;
		else
		{
			c4 = FishJstrcspn( s[0]|(s[1]<<8), fish );
			s += 2;
			if( -1==c4 )
				return;
		}

		fputc( (c1<<1)|((c2&0x60)>>5), out );
		fputc( ((c2&0x1e)<<3)|((c3&0x78)>>3), out );
		if( -1==c4 )
			return;
		fputc( ((c3&0x06)<<5)|(c4>>1), out );
	}
}

void CFishTool::decode464924( unsigned char* str,
							 FILE* out, const char* xish )
{
	// 行頭におさかな文字以外があったらスキップする
	if( -1==FishJstrcspn( str[0]|(str[1]<<8), xish ) )
		return;

	int c1,c2;
	for( unsigned char* s=str; *s!='\n' && *s!='\0'; )
	{
		c1 = FishJstrcspn( s[0]|(s[1]<<8), xish );
		s += 2;
		if( -1==c1 )
			return;
		c2 = FishJstrcspn( s[0]|(s[1]<<8), xish );
		s += 2;
		if( -1==c2 )
			return;

		fputc( (c1<<5)|(c2>>1), out );
	}
}

bool CFishTool::Check( const char* fname, unsigned long fsize )
{
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	const unsigned char *ptr = CTool::common_buf,
						*end = CTool::common_buf + siz - 12;
//--------------------------------------------------------------------//

	// begin 86~/24~/4649 な行があるか？
	char temp[MAX_PATH];
	int m;
	bool first=true;
	for( ; ptr<end; ptr++ )
	{
		if( *ptr=='\0' )
			break;

		if( first )
			first=false, ptr--;
		else if( *ptr!='\r' && *ptr!='\n' )
			continue;

		if( ptr[1]!='b' || ptr[2]!='e' || ptr[3]!='g'
		 || ptr[4]!='i' || ptr[5]!='n' || ptr[6]!=' ' )
			continue;

		return Header((const char*)ptr+1,&m,temp);
	}
	return false;
}

bool CFishTool::Extract( const char* fname, const char* ddir )
{
	char* str=(char*)CTool::common_buf;
//--------------------------------------------------------------------//

	FILE* fsh = fopen( fname,"rb" );
	if( !fsh ) return false;

	int method;
	char outname[MAX_PATH];

	while( kiutil::getline( str,XACR_BUFSIZE,fsh ) )
	{
		if( !Header( str,&method,outname ) )
			continue;

		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(outname)) continue;//追加 By Bonty

		FILE* out = fopen( kiutil::pathMake(outname),"wb" );
		if( !out )
			continue;
		while( kiutil::getline( str,XACR_BUFSIZE,fsh ) )
		{
			// end
			if( 0==strcmp( str,"``" ) )
				break;

			// decode
			switch( method )
			{
			case 86:
				decode86( (unsigned char*)str,out );
				break;
			case 24:
				decode464924( (unsigned char*)str,out,nish );
				break;
			case 4649:
				decode464924( (unsigned char*)str,out,fish );
				break;
			}
		}
		fclose( out );
	}

	return true;
}
