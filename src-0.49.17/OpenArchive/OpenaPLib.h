#ifndef AFX_OPENAPLIB_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENAPLIB_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenaPLib : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenaPLib() : COpenArchive( )
	{COpenaPLib::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	DWORD GetSize(LPCSTR file);

	typedef struct {
		unsigned char	sig[4];
		DWORD			ver;//?
		DWORD			d;//?
		DWORD			d2;//?
		DWORD			o_size;//?
		DWORD			crc;//?
	} APKHEAD;
};
#endif