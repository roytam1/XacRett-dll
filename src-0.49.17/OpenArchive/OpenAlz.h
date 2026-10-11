#ifndef AFX_OPENALZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENALZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenAlz : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenAlz() : offset(0),COpenArchive( )
	{COpenAlz::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);
	long offset;

	// ローカルヘッダ
	
	// 圧縮メソッドの種類
	enum ZipMethod
	{
		Stored,		// 0
		Shrunk,		// 1
		Reduced1,	// 2-5
		Reduced2,
		Reduced3,
		Reduced4,
		Imploded,	// 6
		Tokenized,	// 7 ( not supported )
		Deflated,	// 8
		EnhDeflated,// 9 ( not supported )
		DclImploded,//10 ( not supported )

		Err=178     // this value is used by xacrett (^^;
	};

	struct ALZ
	{
		char	sig[4];
		DWORD	ver;
	};
	struct BLZ
	{
		char	sig[4];
		BYTE	fn_len;
		WORD	atter;//		属性
		WORD	time;
		WORD	date;
		WORD	size_len;//*のバイト数 80:8Byte 40:4Byte 20:2Byte 00だったらこれ以降無し
		WORD	method;//			圧縮方法　0無圧縮　2普通
		DWORD	crc;//	CRC32
		//char*	etc;//*	 展開ファイルサイズ	これは桁によってサイズが変わる
					//* 圧縮ファイルサイズ	これは桁によってサイズが変わる
					//xBYTE ファイル名
	};

	struct BLZ_8
	{
		BLZ	blz;
		LONGLONG ext_size;
		LONGLONG cmp_size;
		unsigned char* fname;
	};

	struct CLZ
	{
		char	sig[4];
	};

	bool doHeader();
	bool read_header();

	BLZ_8	hdr;
};

#endif