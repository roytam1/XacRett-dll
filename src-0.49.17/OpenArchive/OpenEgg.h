#ifndef AFX_OPENEGG_H__EGG_LISTING_INCLUDED__
#define AFX_OPENEGG_H__EGG_LISTING_INCLUDED__


#include "OpenArchive.h"
#include "..\\EggDecode.h"

class COpenEgg : public COpenArchive
{
public: //-- COpenArchive shared interface -----------

	COpenEgg() : COpenArchive( )
		{m_index = 0;}
	~COpenEgg()
		{arc.Close();}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

	EggArchive arc;
	int        m_index;
};

#endif
