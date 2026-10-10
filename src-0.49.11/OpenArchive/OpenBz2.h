#ifndef AFX_OPENBZ2_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENBZ2_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenBz2 : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenBz2() : COpenArchive( )
	{COpenBz2::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif