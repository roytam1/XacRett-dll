#ifndef AFX_ARJTOOL_H__8E99B3E3_3B05_11D4_8D96_96700B556430__INCLUDED_
#define AFX_ARJTOOL_H__8E99B3E3_3B05_11D4_8D96_96700B556430__INCLUDED_

#include "Tool.h"

#define ARJ_FNAME_MAX 512

class CArjTool : public CTool
{
//
// original source is
//
// Unarj 2.43 ( 07/29/96 )
//   http://www.arjsoftware.com/
// Copyright (c) 1991-97 by ARJ Software, Inc.  All rights reserved.
//

public:
	CArjTool() : CTool( "Arj 展開" )
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
		}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"arj");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	int FindHeader( const unsigned char* hdr, unsigned long siz );
	bool ReadHeader();
	bool IsValidFlags();

	void Decode_f();

	FILE *arj,*out;

// デコードサブルーチン
	WORD bitbuf;
	WORD subbitbuf;
	int  bitcount;
	void init_getbits();
	void fillbuf(int n);
	short  getlen;
	short  getbuf;

	short decode_ptr();
	short decode_len();

// CRC
	DWORD crc;
	DWORD crctable[256];
	void UpdateCRC(const BYTE* buf,int len)
		{while(len--)crc = crctable[((BYTE)(crc)^(*buf++))&0xff]^(crc>>8);}
	size_t fread_crc(void* p,size_t n,FILE* f);
	void  fwrite_crc(BYTE* p,int n);

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

#endif
