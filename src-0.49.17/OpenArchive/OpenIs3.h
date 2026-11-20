#ifndef AFX_OPENIS3_H__IS3_OPEN_INCLUDED__
#define AFX_OPENIS3_H__IS3_OPEN_INCLUDED__

#include "OpenArchive.h"
#include "../Is3Decode.h"

class COpenIs3 : public COpenArchive
{
public: //-- COpenArchive

	COpenIs3() : COpenArchive( )
		{COpenIs3::fp=NULL; Opened=false; FileNum=0; Index=-1;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	Is3Archive	arc;
	int			FileNum;
	int			Index;
	bool		Opened;
	bool		set_Find(INDIVIDUALINFO *lpSubInfo);
};

#endif
