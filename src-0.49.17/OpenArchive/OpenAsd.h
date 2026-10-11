#ifndef AFX_OPENASD_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENASD_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenAsd : public COpenArchive
{
private:
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
	int num_of_files;

public: //-- COpenArchive 共通インターフェイス -----------

	COpenAsd() : COpenArchive( )
	{COpenAsd::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif