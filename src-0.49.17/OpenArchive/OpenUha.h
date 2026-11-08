#ifndef AFX_OPENUHA_H__UHA_OPEN_INCLUDED__
#define AFX_OPENUHA_H__UHA_OPEN_INCLUDED__


#include "OpenArchive.h"
#include "../uharc/UhaDecode.h"

class COpenUha : public COpenArchive
{
public: //-- COpenArchive

	COpenUha() : COpenArchive( )
	{COpenUha::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	UhaArchive	arc;
	int			FileNum;
	int			Index;
	bool		Opened;
	bool		set_Find(INDIVIDUALINFO *lpSubInfo);
};

#endif
