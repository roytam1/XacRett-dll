#ifndef AFX_OPENMACBIN_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENMACBIN_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenMacBin : public COpenArchive
{
	#define x68Long(_p) (((_p)[0]<<24)+((_p)[1]<<16)+((_p)[2]<<8)+((_p)[3]))
	#define x68Word(_p) (((_p)[0]<<8)+((_p)[1]))

public: //-- COpenArchive 共通インターフェイス -----------

	COpenMacBin() : COpenArchive( )
	{COpenMacBin::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);
	bool easyCheck( const unsigned char* hdr );
	inline bool isAllZero( const unsigned char* st, const unsigned char* ed )
	{
		for( ; st!=ed; st++ )
			if( *st )
				return false;
		return true;
	}

	char path[MAX_PATH];
};

#endif