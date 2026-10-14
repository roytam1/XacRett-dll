#ifndef AFX_OPENXZ_H__4C5D2E3F_6A44_43B2_9F0A_4D9E6A8B1133__INCLUDED_
#define AFX_OPENXZ_H__4C5D2E3F_6A44_43B2_9F0A_4D9E6A8B1133__INCLUDED_


#include "OpenArchive.h"

class COpenXz : public COpenArchive
{
public:
	COpenXz() : COpenArchive( )
	{COpenXz::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif
