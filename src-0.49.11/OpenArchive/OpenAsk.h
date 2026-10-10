#ifndef AFX_OPENASK_H_INCLUDED_
#define AFX_OPENASKL_H_INCLUDED_


#include "OpenArchive.h"
#include <vector>

class COpenAsk : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenAsk() : COpenArchive( ){}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	std::vector<INDIVIDUALINFO> m_info;
	std::vector<INDIVIDUALINFO>::iterator m_cur_info;
};

#endif