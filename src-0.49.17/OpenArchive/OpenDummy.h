#ifndef AFX_OPENDUMMY_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENDUMMY_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenDummy : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenDummy() : COpenArchive( )
	{COpenDummy::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
	bool OpenArchive(LPCSTR szFileName);
};

#endif