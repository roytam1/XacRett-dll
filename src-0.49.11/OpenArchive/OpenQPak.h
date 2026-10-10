#ifndef AFX_OPENQPAK_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENQPAK_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenQPak : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenQPak() : COpenArchive( )
	{COpenQPak::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

private:
	bool Open_QPak();
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);
	DWORD count;
	struct PakIndex
	{
		char  fname[0x78];
		DWORD offset;
		DWORD size;
	};
};

#endif