#ifndef AFX_OPENDMG_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENDMG_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

#pragma warning( disable : 4786 )
# include <deque>

class COpenDmg : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------
	COpenDmg();
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool   zlibDecode( const BYTE* beg, const BYTE* end, FILE* fp );
	bool    hfsExpand( const BYTE* beg, const BYTE* end);
	void inflateBlock( const BYTE* buf, size_t siz, FILE* fp, size_t outs );
	std::deque<INDIVIDUALINFO>	m_info;
	UINT						m_curinfo;
};

#endif