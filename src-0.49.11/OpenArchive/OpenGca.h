#ifndef AFX_OPENGCA_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENGCA_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include "../GcaSDK/GcaSDK.h"
# include <string>

#pragma comment( lib, "GcaSDK/GcaSDK.lib" )

class COpenGca : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenGca() : COpenArchive( )
	{COpenGca::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	CGca gca;
	int FileNum;
	bool set_Find(INDIVIDUALINFO *lpSubInfo);
};

#endif