#ifndef AFX_OPENHQX_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENHQX_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenHqx : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenHqx() : COpenArchive( )
	{COpenHqx::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	char outname[MAX_PATH];
	DWORD fsize;

	bool FindBeginning(int limit);
	bool ProcessHeader();

	int GetByte();
	int GetByteRaw();

	bool bStarted;
	bool bEOF;
	int prevc;
	int repeating;
	DWORD outbuf,i;

	WORD crc;
	void update_crc(int c)
	{
		DWORD temp=crc;
		for( int i=0; i!=8; i++ )
		{
			c <<= 1;
			if( (temp<<=1) & 0x10000 )
				temp = (temp & 0xffff) ^ 0x1021;
			temp ^= (c >> 8);
			c &= 0xff;
		}
		crc = (WORD)temp;
	}
};

#endif