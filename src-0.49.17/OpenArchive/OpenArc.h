#ifndef AFX_OPENARC_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENARC_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenArc : public COpenArchive
{
private:
	#define ARC_TABSIZE 4096

	struct ArcHead
	{
		unsigned char method;
		char name[13];
		unsigned long csize;
		unsigned short date;
		unsigned short time;
		unsigned short crc;
		unsigned long usize;
	};

	struct arc_entry
	{
		char used;
		unsigned char follower;
		unsigned short next;
		unsigned short predecessor;
	};
	ArcHead hdr;
	char path[MAX_PATH];
	char *pass,*ppos;

	int SkipSfxStub( const BYTE* dat,unsigned long siz,int curpos );
	bool ReadHeader();
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
public: //-- COpenArchive 共通インターフェイス -----------

	COpenArc() : COpenArchive( )
	{COpenArc::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
};

#endif