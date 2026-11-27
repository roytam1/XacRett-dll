#ifndef AFX_OPENBR_H__7A1F3C9E_2B48_4D5A_8E1D_5F6A7B8D2__INCLUDED_
#define AFX_OPENBR_H__7A1F3C9E_2B48_4D5A_8E1D_5F6A7B8D2__INCLUDED_

#include "OpenArchive.h"

class COpenBr : public COpenArchive
{
public:
	COpenBr() : COpenArchive() {};
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif
