#ifndef AFX_OPEN7Z_H__8E42B1C6_7D93_40FA_9C58_2B16E4D07A35__INCLUDED_
#define AFX_OPEN7Z_H__8E42B1C6_7D93_40FA_9C58_2B16E4D07A35__INCLUDED_

#include "OpenArchive.h"
#include "SevenZDecode.h"

class COpenSevenZ : public COpenArchive
{
public:
	COpenSevenZ() : COpenArchive( )
	{	COpenSevenZ::fp=NULL;
		m_az = new CSevenZ;
	}
	~COpenSevenZ()
	{	delete m_az;}

	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

private:
	CSevenZ*	m_az;
	DWORD		m_index;
	bool		m_open;
};

#endif