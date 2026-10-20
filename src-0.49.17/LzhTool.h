#ifndef AFX_LZHTOOL_H__31F8AA01_3F84_11D4_8D96_8AB5A6462337__INCLUDED_
#define AFX_LZHTOOL_H__31F8AA01_3F84_11D4_8D96_8AB5A6462337__INCLUDED_

#include "Tool.h"

class CLzhTool : public CTool
{
//
// see LzhDecoder2.h
//

public:
	CLzhTool() : CTool( "Lzh 解凍" )
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
	}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"lzh")||0==stricmp(ext,"pma");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	int FindHeader( const char* fname, const BYTE* hdr, DWORD siz );
	bool ReadHeader( unsigned char* buf );

	WORD crctable[256];
	WORD crc;
	BYTE sum;
	void UpdateCRC( BYTE *p,int n )
		{while( n-- )crc = crctable[(crc^(*p++))&0xff] ^ (crc >> 8);}
	void UpdateSum( BYTE *p,int n )
		{while( n-- )sum+=*(p++);}
	int  fread_crc( BYTE* p,int n)
		{n=fread(p,1,n,lzh);if(n==-1)n=0;
		UpdateCRC(p,n);UpdateSum(p,n);return n;}

// 現在処理中の格納ファイルの情報がメンバに入る
	FILE *lzh,*out;

	BYTE  h_Level;					// ヘッダレベル
	char  h_FileName[MAX_PATH*2];	// ファイル名(パス付き)
	char  h_Method[6];				// 圧縮法 "-lh5-" など。
	DWORD h_CompSize;				// 圧縮されたサイズ
	DWORD h_OrigSize;				// 元のサイズ
	WORD  h_Attrib;					// 属性
	DWORD h_Update;					// 更新日時(h0,h1:ftime  h2:time_t)
	WORD  h_LvlField;					// LArc 7 level field
};

#endif
