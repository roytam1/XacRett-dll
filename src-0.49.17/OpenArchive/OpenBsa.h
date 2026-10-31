#ifndef AFX_OPENBSA_H__BSA_OPEN_INCLUDED__
#define AFX_OPENBSA_H__BSA_OPEN_INCLUDED__


#include "OpenArchive.h"
#include "../BsaDecode.h"

class COpenBsa : public COpenArchive
{
public: //-- COpenArchive

	COpenBsa() : COpenArchive( )
	{COpenBsa::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	BsaArchive	arc;
	int			FileNum;
	int			Index;
	bool		Opened;
	bool		set_Find(INDIVIDUALINFO *lpSubInfo);
};

#endif
