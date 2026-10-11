#ifndef AFX_OPENMIME_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENMIME_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenMime : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenMime() : COpenArchive( )
	{COpenMime::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool DoHeader( char* name,int count=1000000000 );
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);
	void skip();
};

#endif