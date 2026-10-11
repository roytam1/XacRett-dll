#ifndef AFX_OPENAR_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENAR_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenAr : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenAr() : COpenArchive( )
	{COpenAr::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
};

#endif