#ifndef AFX_OPENARG_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENARG_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_

#include "OpenArchive.h"

class COpenArg : public COpenArchive
{
private:
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
public: //-- COpenArchive 共通インターフェイス -----------

	COpenArg() : COpenArchive( )
	{COpenArg::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif