#ifndef AFX_OPENASD_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENASD_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenAsd : public COpenArchive
{
private:
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
	int num_of_files;
	int m_ver;		// '1' = ASD 0.1.x, '2' = ASD 0.2.x
	long m_hdrpos;	// offset of the first member header
	int m_idx;		// current member index

public: //-- COpenArchive 共通インターフェイス -----------

	COpenAsd() : COpenArchive( )
	{COpenAsd::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif