#ifndef AFX_OPENAIN_H__C3D4E5F6_A7B8_4C9D_0E1F_2A3B40516272__INCLUDED_
#define AFX_OPENAIN_H__C3D4E5F6_A7B8_4C9D_0E1F_2A3B40516272__INCLUDED_


#include "OpenArchive.h"

struct AinEntry
{
	char name[MAX_PATH];
	DWORD orig_size;
	DWORD comp_size;
	DWORD timestamp;
	DWORD crc;
};

class COpenAin : public COpenArchive
{
public: //-- COpenArchive interface -----------

	COpenAin() : COpenArchive( )
	{COpenAin::fp=NULL; COpenAin::m_entries=NULL; COpenAin::m_count=0;
	 COpenAin::m_cur=0; COpenAin::m_method=0;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
	bool LoadIndex(void);
	AinEntry* m_entries;
	int m_count;
	int m_cur;
	BYTE m_method;
};

#endif
