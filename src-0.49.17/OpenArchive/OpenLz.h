#ifndef AFX_OPENLZ_H__5E3B8A17_9D24_4C60_8F31_B7A2E6C05D9B__INCLUDED__
#define AFX_OPENLZ_H__5E3B8A17_9D24_4C60_8F31_B7A2E6C05D9B__INCLUDED__


#include "OpenArchive.h"

class COpenLz : public COpenArchive
{
public:
	COpenLz() : COpenArchive( )
	{COpenLz::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};


#endif