
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

// Decode an in-memory uudecode payload straight to a file.
// The output name comes from the payload's `begin` line, which GNU
// shar always emits.  Returns 1 on success.  Additive: Extract()
// above is untouched, so the standalone .uue/.xxe providers behave
// exactly as before.
int UnixDecodeUueToFile( const char* data, long len, const char* dir )
{
	char table[128];
	char outname[5000]="";
	static char line[XACR_BUFSIZE];
	const char *p = data, *end = data + len;
	int i, mode, haveName = 0, ok = 0;
	FILE* op = NULL;

	(void)dir;
	if( len<=0 )
		return 0;

	memset( table,0,sizeof(table) );
	for( i=0; i!=64; i++ )
		table[ ' '+i ] = (char)i;

	while( p<end )
	{
		const char* nl = (const char*)memchr( p,'\n',(size_t)(end-p) );
		size_t n = nl ? (size_t)(nl-p) : (size_t)(end-p);
		int cnt;
		const char* q;

		// leave room for the NUL and the 4-byte groups we read past
		// the end of a short line.
		if( n>sizeof(line)-80 )
			break;
		memcpy( line, p, n );
		line[n] = '\0';
		p = nl ? nl+1 : end;
		if( n && line[n-1]=='\r' )
			line[n-1]='\0';

		if( !haveName )
		{
			if( 0==strncmp(line,"begin ",6)
			 // two conversions, so sscanf returns 2, not 1.
			 && 2==sscanf(line,"begin %o %4999s",&mode,outname)
			 && *outname )
			{
				haveName = 1;
				kiutil::pathMake83( outname );
				op = fopen( outname,"wb" );
				if( op==NULL )
					return 0;
			}
			continue;
		}

		if( 0==strncmp(line,"end",3) )
			break;
		cnt = (unsigned char)table[ (unsigned char)line[0] & 0x7f ];
		if( cnt==0 )
			break;				// the '`' terminator

		for( q=line+1 ; cnt>0 ; q+=4, cnt-=3 )
		{
			fputc( table[(unsigned char)q[0]&0x7f]<<2
				      | table[(unsigned char)q[1]&0x7f]>>4 , op );
			if( cnt==1 ) continue;
			fputc( table[(unsigned char)q[1]&0x7f]<<4
				      | table[(unsigned char)q[2]&0x7f]>>2 , op );
			if( cnt==2 ) continue;
			fputc( table[(unsigned char)q[2]&0x7f]<<6
				      | table[(unsigned char)q[3]&0x7f] , op );
		}
	}

	if( op )
		fclose(op);
	ok = (op!=NULL);
	return ok;
}
