#ifndef AFX_OPENREZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENREZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_

#include "OpenArchive.h"

class COpenRez : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenRez() : COpenArchive( )
	{COpenRez::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	int DevFileNum;

};

#endif
