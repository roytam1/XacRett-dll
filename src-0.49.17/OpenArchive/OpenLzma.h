#ifndef AFX_OPENLZMA_H__3F8A2B11_9C44_42E7_8D1F_5A6B7C8D9E02__INCLUDED_
#define AFX_OPENLZMA_H__3F8A2B11_9C44_42E7_8D1F_5A6B7C8D9E02__INCLUDED_


#include "OpenArchive.h"

class COpenLzma : public COpenArchive
{
public:
	COpenLzma() : COpenArchive( )
	{COpenLzma::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif
