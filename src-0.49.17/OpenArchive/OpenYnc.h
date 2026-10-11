#ifndef AFX_OPENYENC_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENYENC_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenYnc : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenYnc() : COpenArchive( )
	{COpenYnc::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);
	int  StdPlus_xtoi( char* hex )
	{
		char*  s = hex;
		char    c;
		DWORD    num = 0;
		
		while ( *s != '\0' ) {
			c = *s;
			if ( c >= '0' && c <= '9' )       num = num * 16 + (c - '0');
			else if ( c >= 'A' && c <= 'F' )  num = num * 16 + (c - 'A' + 10);
			else if ( c >= 'a' && c <= 'f' )  num = num * 16 + (c - 'a' + 10);
			else  return  num;
			s++;
		}
		return  num;
	}

};

#endif