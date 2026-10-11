#ifndef _DEBUG
#ifndef AFX_OPENJC1_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENJC1_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenJc1 : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenJc1() : COpenArchive( )
	{COpenJc1::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	DWORD GetSize(LPCSTR file);
};

#endif
#endif