#ifndef AFX_OPENDGC_H__DGC_OPEN_INCLUDED__
#define AFX_OPENDGC_H__DGC_OPEN_INCLUDED__


#include "OpenArchive.h"
#include "../DgcDecode.h"

class COpenDgc : public COpenArchive
{
public: //-- COpenArchive

	COpenDgc() : COpenArchive( )
	{COpenDgc::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	DgcArchive	arc;
	int			FileNum;
	int			Index;
	bool		Opened;
	bool set_Find(INDIVIDUALINFO *lpSubInfo);
};

#endif
