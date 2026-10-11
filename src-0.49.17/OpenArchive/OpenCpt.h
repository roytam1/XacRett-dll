#ifndef AFX_OPENCPT_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENCPT_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenCpt : public COpenArchive
{
	#define F_FNAME          0
	#define F_FOLDER        32
	#define F_FOLDERSIZE	33
	#define F_VOLUME        35
	#define F_FILEPOS       36
	#define F_FTYPE         40
	#define F_CREATOR       44
	#define F_CREATIONDATE  48
	#define F_MODDATE       52
	#define F_FNDRFLAGS     56
	#define F_FILECRC       58
	#define F_CPTFLAG       62
	#define F_RSRCLENGTH    64
	#define F_DATALENGTH    68
	#define F_COMPRLENGTH   72
	#define F_COMPDLENGTH   76
	#define FILEHDRSIZE     80

public: //-- COpenArchive 共通インターフェイス -----------

	COpenCpt() : COpenArchive( )
	{COpenCpt::fp=NULL;make_crc_table();}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);

	bool ReadCptHdr();

	WORD entries;
	char path[MAX_PATH*3];

	bool isMacBin;

	DWORD crc_table[256];
	void make_crc_table()
		{
			for( DWORD c,n=0; n!=256; n++ )
			{
				c = n;
				for( DWORD k=8; k; k-- )
					c = (c&1) ? ((0xedb88320L)^(c>>1)) : (c>>1);
				crc_table[n] = c;
			}
		}
	DWORD updcrc( DWORD crc,BYTE* cp,int cnt )
		{
			while( cnt-- )
				crc = (crc>>8)^crc_table[(crc&0xff)^*cp++];
			return crc;
		}
};

#endif