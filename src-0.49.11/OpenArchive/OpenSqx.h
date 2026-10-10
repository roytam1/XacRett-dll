#ifndef AFX_OPENSQX_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENSQX_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

#if !defined(__CX_TYPE_H)
#include "../unsqx/cx_type.h"
#endif

#if !defined(__CX_ERR_H)
#include "../unsqx/cx_err.h"
#endif

#if !defined(__SQX_VARS_H)
#include "../unsqx/sqx_vars.h"
#endif

#if !defined(__SQX_OS_H)
#include "../unsqx/os/sqx_os.h"
#endif

#include <vector>

class COpenSqx : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenSqx() : COpenArchive( )
	{COpenSqx::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

private:
	//開放とか面倒なのでvectorに読み込ませとく^^;
	std::vector<INDIVIDUALINFO> m_info;
	std::vector<INDIVIDUALINFO>::iterator m_cur_info;

	void AddAlNode(ARC_NODE* al);
};

#endif