#ifndef AFX_OPENLZX_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENLZX_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenLzx : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenLzx() : COpenArchive( )
	{COpenLzx::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
	unsigned int crc;
	
	unsigned int sum;
	static const unsigned int crc_table[256];
	void crc_calc(unsigned char *memory, unsigned int length);
};

#endif