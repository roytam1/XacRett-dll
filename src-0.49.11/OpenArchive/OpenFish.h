#ifndef AFX_OPENFISH_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENFISH_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenFish : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenFish() : COpenArchive( )
	{COpenFish::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	int readsize;
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
	bool Header(const char* str,char* outname  ,int& size);
};

#endif