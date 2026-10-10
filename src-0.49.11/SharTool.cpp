
#include "stdafx.h"
#include "SharTool.h"

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
		if( 0==strncmp(line,"sed",3)
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
