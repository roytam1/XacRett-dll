#ifndef AFX_OPENMSCOMPRESS_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENMSCOMPRESS_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenMSCompress : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenMSCompress() : COpenArchive( )
	{COpenMSCompress::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool GetLzFileName(LPSTR buf);
	char m_missingchar;	// the character COMPRESS.EXE replaced
	void lower(char *c) ;
};

#endif