#ifndef AFX_OPENDZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENDZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenDz : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenDz() : COpenArchive( )
	{COpenDz::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

private:
	struct DzfileInf
	{
		unsigned int ptr;
		unsigned int csz, usz, isz;
		char         nam[MAX_PATH];

		unsigned short pak;
		unsigned int crc;
		unsigned int type;
		unsigned int date;
		

	};

	bool read_header(INDIVIDUALINFO *lpSubInfo);

	unsigned int ver1;
	unsigned int fileNum;
};

#endif