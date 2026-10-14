#ifndef AFX_OPENZSTD_H__3E8B1A5C_7D29_4F2E_9C4A_1B6D8E0F2A53__INCLUDED_
#define AFX_OPENZSTD_H__3E8B1A5C_7D29_4F2E_9C4A_1B6D8E0F2A53__INCLUDED_


#include "OpenArchive.h"

class COpenZstd : public COpenArchive
{
public:
	COpenZstd() : COpenArchive( )
	{COpenZstd::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif
