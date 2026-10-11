#ifndef AFX_BGATOOL_H__5C536C62_46D0_11D4_8D96_AE068B524B38__INCLUDED_
#define AFX_BGATOOL_H__5C536C62_46D0_11D4_8D96_AE068B524B38__INCLUDED_

#include "Tool.h"

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

class CBgaTool : public CTool
{
//
// using
//
// zlib 1.1.3
//   http://www.info-zip.org/pub/infozip/zlib/
// Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler
//
// libbzip2 1.0.1
//   http://sources.redhat.com/bzip2/
// Copyright (C) 1996-2000 Julian R Seward.  All rights reserved.
//
// see "zlib/zlib.h" and "libbzip2/LICENSE"
//
//
// ヘッダ構造の情報ありがとうございました！ ＞ 木村利靖さん
//

public:
	CBgaTool() : CTool( "Bza/Gza 展開" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"gza") || 0==stricmp(ext,"bza");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool GzDec( FILE* in, FILE* out, unsigned long origsize );
	bool BzDec( FILE* in, FILE* out, unsigned long origsize );
	bool NcDec( FILE* in, FILE* out, unsigned long origsize );

	bool IsCompressed( BgaHeader* hdr );

	bool ReadHeader( FILE* in, BgaHeader* hdr );
	int  FindHeader( const unsigned char* dat, unsigned long siz );
};

#endif
