#ifndef AFX_OPENPackz_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENPackz_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenPackz : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenPackz() : COpenArchive( )
	{COpenPackz::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

};

#endif