
#include "stdafx.h"
#include "FishTool.h"

static const char fish[]=
"鯵鮎鰯鰻鰍鰹魚鯨鯉鮭鯖鮫鯛鱈鰭鮒鮪鱒鰐魴鮓鮃鮑\
鮖鮗鮟鮠鮨鮴鯀鯊鮹鯆鯏鯑鯒鯣鯢鯤鯔鯡鰺鯲鯱鯰鰕鰔\
鰉鰓鰌鰆鰈鰒鰊鰄鰮鰛鰥鰤鰡鰰鱇鰲鱆鰾鱚鱠鱧鱶鱸";

static const char nish[]=
"￥＄♂♀～ー・アイウエオカキクケコサシスセソタチツテトナニヌ\
ネノハヒフヘホマミムメモヤユヨラリルレロワヲンァィゥェォッャュョ゛゜";

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
