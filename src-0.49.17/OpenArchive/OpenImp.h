#ifndef AFX_OPENIMP_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENIMP_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include "../ImpTool.h"

class COpenImp : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenImp() : COpenArchive( )
	{COpenImp::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	unsigned long ent;
	
	bool Find_Sub(INDIVIDUALINFO *lpSubInfo);
	CImpTool imp;
};

#endif