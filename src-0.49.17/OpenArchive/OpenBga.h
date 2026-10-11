#ifndef AFX_OPENBGA_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENBGA_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenBga : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenBga() : COpenArchive( )
	{COpenBga::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	struct BgaHeader
	{
		DWORD checksum;			// signed で type-fname の和
		char  type[4];			// "GZIP" または "BZ2"
		DWORD compressed_size;	// 圧縮後のデータサイズ( ヘッダは含まない )
		DWORD original_size;	// 元のファイルサイズ
		WORD  date;				// ファイルの更新日付( DOS形式 )
		WORD  time;				// ファイルの更新時刻( DOS形式 )
		BYTE  attrib;			// ファイルの属性
								// ( 1:RO 2:Hid 4:Sys 8:Vol 16:Dir 32:Arc )
		BYTE  header_type;		// ヘッダの種類( 現在は常に 0 )
		WORD  arc_type;			// アーカイブタイプ
								// 0:(ext==↓ ? 非圧縮 : 圧縮) 1:圧縮 2:非圧縮
								//	.ARC, .ARJ, .BZ2, .BZA, .CAB, .GZ, .GZA, .LZH,
								//	.LZS, .PAK, .RAR, .TAZ, .TBZ, .TGZ, .Z, .ZIP, .ZOO
		WORD  dir_name_len;		//
		WORD  file_name_len;	//
		char  name[MAX_PATH];	// dir_name_len + file_name_len ( no '\0' )
	};
	int  FindHeader();
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
	bool ReadHeader(BgaHeader* hdr );

};

#endif