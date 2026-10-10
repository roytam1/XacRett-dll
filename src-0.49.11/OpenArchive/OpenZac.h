#ifndef AFX_OPENZAC_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENZAC_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenZac : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenZac() : COpenArchive( )
	{COpenZac::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool zVer2;
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
};

#endif