#ifndef AFX_OPENBOO_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENBOO_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenBoo : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenBoo() : COpenArchive( )
	{COpenBoo::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

};

#endif
