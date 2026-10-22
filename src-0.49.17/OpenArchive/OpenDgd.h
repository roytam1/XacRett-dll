#ifndef AFX_OPENDGD_H__DGD_SPANNED_FILE_INCLUDED__
#define AFX_OPENDGD_H__DGD_SPANNED_FILE_INCLUDED__

#include "OpenArchive.h"

class COpenDgd : public COpenArchive
{
public:
	COpenDgd() : COpenArchive( )
	{COpenDgd::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif
