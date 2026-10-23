#ifndef AFX_OPENALZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENALZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include "..\AlzDecode.h"

class COpenAlz : public COpenArchive
{
public: //-- COpenArchive shared interface -----------

	COpenAlz() : COpenArchive( )
		{}
	~COpenAlz()
		{chain.Close();}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

	AlzChain chain;

protected:
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);
};

#endif