#ifndef AFX_OPENSHAR_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENSHAR_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenShar : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenShar() : COpenArchive( )
	{COpenShar::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);

	bool getline( FILE* in, bool bNoWhite, char* str )
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
	void getstring( char* from,char* to )
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
	void getparameters( char* str,char* lead,char* file,char* word )
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
};

#endif