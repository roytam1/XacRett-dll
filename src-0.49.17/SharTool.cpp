
#include "stdafx.h"
#include "SharTool.h"
#include "UnixDecoderTool.h"

static bool getline( FILE* in, bool bNoWhite, char* str )
{
	int ch;
	*str='\0';

	if( bNoWhite )
    {
		while( ((ch=fgetc(in))==' ') || (ch=='\t') );
		if( ch==EOF )  return false;
		if( ch=='\n' ) return true;
		*str++ = ch;
    }
	while( (ch=fgetc(in))!='\n' )
	{
		if( ch==EOF ){ *str='\0'; return false; }
		*str++ = ch;
	}
	*str='\0';
	return true;
}

static void getstring( char* from,char* to )
{
	while( (*from==' ') || (*from=='\t') )from++;

	switch(*from)
	{
    case '\0':
		break;
    case '\'':
		from++; while( *from!='\'' ) *to++ = *from++; *to='\0'; break;
    case '\"':
		from++; while( *from!='\"' ) *to++ = *from++; *to='\0'; break;
    default:
		while( (*from!=' ') && (*from!='\t') )
			if( *from!='\\' ) *to++ = *from++;
			else from++;
		break;
	}
	*to='\0';
}

// Pull the `<< 'TERM'` terminator out of a shell command line.
// Handles <<'T', <<"T", <<\T and << T.
static bool getheredoc( const char* str, char* term, int termSize )
{
	const char* p = strstr( str, "<<" );
	int i = 0;
	if( p==NULL )
		return false;
	p += 2;
	if( *p=='\\' )
		p++;
	while( *p==' ' || *p=='\t' )
		p++;
	// GNU shar quotes the terminator:  << 'SHAR_EOF'
	if( *p=='\'' || *p=='"' )
		p++;
	while( *p && *p!='\'' && *p!='"' && *p!=' ' && *p!='\t'
		&& i<termSize-1 )
		term[i++] = *p++;
	term[i] = '\0';
	return i>0;
}

static void getparameters( char* str,char* lead,char* file,char* word )
{
	*lead = *file = *word = '\0';

	char *p=str;
	while( *p )
	{
		switch( *p )
		{
		case '<':	if( *(++p)=='<' )	++p;
					getstring( p++,word );	break;
		case '>':	getstring( ++p,file );	break;
		case 's':
			if( p[1]=='/' && p[2]=='^' )
			{
				p+=3;
				while( *p!='/' && *p!='\0' )
					*lead++ = *p++;
				*lead='\0';
			}
			// fall through...
		default:	p++;					break;
		}
	}
}

bool CSharTool::Check( const char* fname, unsigned long fsize )
{
	// 拡張子チェック
	const char* ext = kiutil::pathExt(fname);
	return ( 0==stricmp(ext,"shar") || 0==stricmp(ext,"sh") );
}

bool CSharTool::Extract( const char* fname,const char* ddir )
{
	FILE *sh,*out;
	char *line = (char*)CTool::common_buf,
		lead[256], file[256], end[256];
	int llen, elen;

	if( !(sh=fopen(fname,"rt")) )
		return false;

	while( true )
	{
		if( !getline( sh,true,line ) )
			break;

		// コマンド名が来たらそこから格納ファイルと見なす
		if( NULL!=strstr(line,"uudecode") )
		{
			// GNU shar stores the payload as a heredoc piped into a
			// decoder.  The common form is
			//     sed 's/^X//' << 'SHAR_EOF' | uudecode &&
			// where the sed strips a defensive 'X' that shar may
			// prepend to each line.  This archive has none, but honour
			// it anyway.  The output name comes from the uudecode
			// `begin` line.  Tested BEFORE the sed branch, because
			// this line starts with "sed".  Official 0.49.17 does not
			// support this at all, so it is a deliberate divergence.
			char term[128];
			char *buf;
			long used = 0, cap, n;
			bool stripX;

			if( !getheredoc( line,term,sizeof(term) ) )
				continue;

			if (m_Endflg) return true;// append-only flag

			fseek( sh,0,SEEK_END );
			cap = ftell( sh );
			fseek( sh,0,SEEK_SET );
			if( cap<0 )
				cap = 0;
			if( cap>16777216L )
				cap = 16777216L;
			buf = new char[cap+2];

			stripX = ( NULL!=strstr(line,"s/^X//") );
			while( true )
			{
				if( !getline( sh,false,line ) )
					break;
				if( 0==strcmp( line,term ) )
					break;
				n = (long)strlen(line);
				if( stripX && n && *line=='X' )
				{
					line++;
					n--;
				}
				if( used+n+2>cap )
				{
					used = -1;
					break;
				}
				memcpy( buf+used, line, n );
				used += n;
				buf[used++] = '\n';
			}

			if( used>0 )
				UnixDecodeUueToFile( buf, used, ddir );
			delete [] buf;
		}
		else if( 0==strncmp(line,"sed",3)
			 || 0==strncmp(line,"gres",4)
			 || 0==strncmp(line,"cat",3) )
		{
			// ↓こんな感じになっている。
			// sed "s/^XX//" >'gzip-1.2.4/README' <<'END_OF_FILE'

			getparameters( line,lead,file,end );
			llen = strlen( lead );
			elen = strlen( end );

			if (m_Endflg) return true;//追加 By Bonty
			//ワイルドカード処理
			if (!CheckStr(file))//追加 By Bonty
			{
				// データ部飛ばし
				while( true )
				{
					if( !getline(sh,false,line ) )
						break;
					if( 0==strncmp( line,end,elen ) )
						break;	
				}
				continue;
			}
			
			// 出力先開く
			if( !(out=fopen( kiutil::pathMake( file ),"wt" )) )
				continue;

			// 書き出す
			while( true )
			{
				bool eof=( !getline( sh,false,line ) );

				if( 0==strncmp( line,end,elen ) )
					break;

				fprintf( out,"%s\n",
					0==strncmp(line,lead,llen) ? line+llen : line );

				if( eof )
					break;
			}

			// 出力先閉じる
			fclose( out );
		}
	}

	fclose( sh );
	return true;
}
