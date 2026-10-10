#ifndef AFX_OPENARJ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENARJ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_

#include "OpenArchive.h"

#define ARJ_FNAME_MAX 512

class COpenArj : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenArj() : COpenArchive( )
		{
			for( DWORD r,j,i=0; i!=256; i++ )
			{
				r = i;
				for( j=8; j!=0; j-- )
				{
					if( r&1 )	r = (r>>1)^0xEDB88320L;//CRCPOLY
					else		r >>= 1;
				}
				crctable[i] = r;
			}
			COpenArj::fp=NULL;
		};
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

private:
	struct ArjHeader
	{
		// ヘッダ
		WORD h_Size;	//headersize
		BYTE h_1stSize;	//first_hdr_size
		BYTE h_ArjVer;	//arj_nbr
		BYTE h_ArjXVer;	//arj_x_nbr
		BYTE h_OS;		//host_os
		BYTE h_Flags;	//arj_flags
		BYTE h_Method;	//method
		BYTE h_FileType;//file_type
		//BYTE h_PassInfo;
		DWORD h_TimeStamp;
		DWORD h_Compsize;
		DWORD h_Origsize;
		DWORD h_FileCRC;
		WORD  h_EntryPos;
		WORD  h_Attrib;
		WORD  h_HostData;
		//BYTE[n] 追加情報。(h_Sizeでここを含んだ長さが求まる)
		char h_Filename[ARJ_FNAME_MAX];	// ファイル名(end with 0)
		//char[n] コメント(end with 0)
		DWORD h_CRC;	//header_crc
		//以下、拡張ヘッダ繰り返し
	};
	ArjHeader ahdr;
	int FindHeader( const unsigned char* hdr, unsigned long siz );
	bool ReadHeader();
	bool IsValidFlags();

	bool FindSub(INDIVIDUALINFO *lpSubInfo);

	// CRC
	DWORD crc;
	DWORD crctable[256];
	void UpdateCRC(const BYTE* buf,int len)
		{while(len--)crc = crctable[((BYTE)(crc)^(*buf++))&0xff]^(crc>>8);}
	size_t fread_crc(void* p,size_t n,FILE* f);
	void  fwrite_crc(BYTE* p,int n);
};

#endif
