#ifndef AFX_OPENLZH_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENLZH_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenLzh : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenLzh() : COpenArchive( )
		{
			// CRCテーブル初期化
			for( WORD i=0,j,r; i!=256; i++ )
			{
				r = i;
				for( j=0; j!=8; j++ )
				{
					if( r&1 ) r = ((r>>1)^0xA001);//CRCPOLY
					else	  r>>=1;
				}
				crctable[i] = r;
			}
			COpenLzh::fp=NULL;
		};
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

private:
	int FindHeader( const BYTE* hdr, DWORD siz );
	bool ReadHeader( unsigned char* buf );

	bool FindSub(INDIVIDUALINFO *lpSubInfo);

	WORD crctable[256];
	WORD crc;
	BYTE sum;
	void UpdateCRC( BYTE *p,int n )
		{while( n-- )crc = crctable[(crc^(*p++))&0xff] ^ (crc >> 8);}
	void UpdateSum( BYTE *p,int n )
		{while( n-- )sum+=*(p++);}
	int  fread_crc( BYTE* p,int n)
		{n=fread(p,1,n,COpenLzh::fp);if(n==-1)n=0;
		UpdateCRC(p,n);UpdateSum(p,n);return n;}

	BYTE  h_Level;					// ヘッダレベル
	char  h_FileName[MAX_PATH*2];	// ファイル名(パス付き)
	char  h_Method[6];				// 圧縮法 "-lh5-" など。
	DWORD h_CompSize;				// 圧縮されたサイズ
	DWORD h_OrigSize;				// 元のサイズ
	WORD  h_Attrib;					// 属性
	DWORD h_Update;					// 更新日時(h0,h1:ftime  h2:time_t)
	WORD  h_filecrc;				// CRC
	BYTE  h_OS;
};

#endif