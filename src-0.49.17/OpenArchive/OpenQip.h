#ifndef AFX_OPENQIP_H__QIP_OPEN_INCLUDED__
#define AFX_OPENQIP_H__QIP_OPEN_INCLUDED__


#include "OpenArchive.h"
#include "../QipDecode.h"

class COpenQip : public COpenArchive
{
public: //-- COpenArchive

	COpenQip() : COpenArchive( )
		{COpenQip::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	QipArchive	arc;
	int			FileNum;
	int			Index;
	bool		Opened;
	bool		set_Find(INDIVIDUALINFO *lpSubInfo);
};

#endif
