#ifndef AFX_OPENBlz_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENBlz_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenBlz : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenBlz() : COpenArchive( )
	{COpenBlz::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	unsigned long get_uint32(const byte *p);
};

#endif