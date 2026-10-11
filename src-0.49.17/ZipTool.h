#ifndef AFX_ZIPTOOL_H__4C5F3720_4483_11D4_8D96_88668194683D__INCLUDED_
#define AFX_ZIPTOOL_H__4C5F3720_4483_11D4_8D96_88668194683D__INCLUDED_

#include "Tool.h"

// ローカルヘッダ
struct ZipLocalHeader
{
	// 'P' 'K' 03 04
	 WORD ver; // version_needed_to_extract
	 WORD flg; // general_purpose_bit_flag
	 WORD mhd; // compression_method
	 WORD tim; // last_modified_file_time
	 WORD dat; // last_modified_file_date
	DWORD crc; // crc32
	DWORD csz; // compressed-size
	DWORD usz; // uncompressed-size
	 WORD fnl; // filename-len
	 WORD exl; // extra_field_length

	char fnm[MAX_PATH];
//	BYTE ext[];
};

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

// シャノン-ファノ木 ( for explosion )
struct sf_entry
{ 
	WORD Code; 
	BYTE Value; 
	BYTE BitLength; 
};
struct sf_tree
{
	sf_entry entry[256];
	int      entries;
	int      MaxLength;
}; 

class CZipTool : public CTool
{
//
// some of the codes are taken from
//
// Info-Unzip 5.41 ( 2000-Apr-09 )
//   ftp://ftp.info-zip.org/pub/infozip/
// Copyright (c) 1990-2000 Info-ZIP.  All rights reserved.
//

/*---------------------------------------------------------------------------
This is version 2000-Apr-09 of the Info-ZIP copyright and license.
The definitive version of this document should be available at
ftp://ftp.info-zip.org/pub/infozip/license.html indefinitely.


Copyright (c) 1990-2000 Info-ZIP.  All rights reserved.

For the purposes of this copyright and license, "Info-ZIP" is defined as
the following set of individuals:

   Mark Adler, John Bush, Karl Davis, Harald Denker, Jean-Michel Dubois,
   Jean-loup Gailly, Hunter Goatley, Ian Gorman, Chris Herborth, Dirk Haase,
   Greg Hartwig, Robert Heath, Jonathan Hudson, Paul Kienitz, David Kirschbaum,
   Johnny Lee, Onno van der Linden, Igor Mandrichenko, Steve P. Miller,
   Sergio Monesi, Keith Owens, George Petrov, Greg Roelofs, Kai Uwe Rommel,
   Steve Salisbury, Dave Smith, Christian Spieler, Antoine Verheijen,
   Paul von Behren, Rich Wales, Mike White

This software is provided "as is," without warranty of any kind, express
or implied.  In no event shall Info-ZIP or its contributors be held liable
for any direct, indirect, incidental, special or consequential damages
arising out of the use of or inability to use this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. Redistributions of source code must retain the above copyright notice,
       definition, disclaimer, and this list of conditions.

    2. Redistributions in binary form must reproduce the above copyright
       notice, definition, disclaimer, and this list of conditions in
       documentation and/or other materials provided with the distribution.

    3. Altered versions--including, but not limited to, ports to new operating
       systems, existing ports with new graphical interfaces, and dynamic,
       shared, or static library versions--must be plainly marked as such
       and must not be misrepresented as being the original source.  Such
       altered versions also must not be misrepresented as being Info-ZIP
       releases--including, but not limited to, labeling of the altered
       versions with the names "Info-ZIP" (or any variation thereof, including,
       but not limited to, different capitalizations), "Pocket UnZip," "WiZ"
       or "MacZip" without the explicit permission of Info-ZIP.  Such altered
       versions are further prohibited from misrepresentative use of the
       Zip-Bugs or Info-ZIP e-mail addresses or of the Info-ZIP URL(s).

    4. Info-ZIP retains the right to use the names "Info-ZIP," "Zip," "UnZip,"
       "WiZ," "Pocket UnZip," "Pocket Zip," and "MacZip" for its own source and
       binary releases.
  ---------------------------------------------------------------------------*/

public: //-- CTool 共通インターフェイス -----------

	CZipTool() : CTool( "Zip 展開" )
		{}

	bool IsType( const char* ext )
		{ return 0==stricmp(ext,"zip"); }
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private: //-- Zip内部処理用 -----------------------

// CRC, 暗号解除

	DWORD crc32( DWORD crc, const BYTE* dat, int len );

	#define zdecode(c) update_keys(c^=decrypt_byte())
	int decrypt_byte();
	int update_keys(int c);
	DWORD keys[3];

// ファイルIO

	FILE *zip,*out;

	void write_init()
		{ wrtcrc = 0; }
	void zipwrite( BYTE* dat, int len )
		{
			wrtcrc = crc32( wrtcrc, dat,len );
			fwrite( dat, 1, len, out );
		}
	DWORD wrtcrc;

	bool read_init( const char* pwd=NULL,ZipLocalHeader* hdr=NULL );
	int zipread( BYTE* dat, int len )
		{
			len = fread( dat, 1, len, zip );
			if( needdec )
				for( int i=0; i<len; i++ )
					zdecode( dat[i] );
			return len;
		}
	bool needdec;

// bit-reader
	unsigned long bitbuf;
	int bits_left;
	bool bits_eof;

	void initbits()
		{
			bits_eof=false, bits_left=0, bitbuf=0;
		}
	int getbits( int n )
		{
			if( n <= bits_left )
			{
				int c = (int)(bitbuf & ((1<<n)-1));
				bitbuf >>= n;
				bits_left -= n;
				return c;
			}
			return fillbits( n );
		}
	int fillbits( int n )
		{
			BYTE next;

			if( !zipread( &next,1 ) )
				bits_eof = true;
			else
			{
				bitbuf |= (next<<bits_left);
				bits_left += 8;

				if( zipread( &next,1 ) )
				{
					bitbuf |= (next<<bits_left);
					bits_left += 8;
				}
			}

			int c = (int)(bitbuf & ((1<<n)-1));
			bitbuf >>= n;
			bits_left -= n;
			return c;
		}

// 展開アルゴリズム

	void Unstore( DWORD usz, DWORD csz );
	void Inflate( DWORD usz, DWORD csz );
	void Unshrink( DWORD usz, DWORD csz );

	// unreduce
	void Unreduce( DWORD usz, DWORD csz, int factor );
	void LoadFollowers( BYTE* Slen, BYTE followers[][64] );

	// explode
	void Explode( DWORD usz, DWORD csz, bool _8k,bool littree );
	void LoadTree( sf_tree* tree, int treesize );
	int  ReadTree( sf_tree* tree );
	void ReadLengths( sf_tree* tree );

// ヘッダ処理
// 面倒なのでCentralDirectory系は一切無視！

	// 現在のファイル位置からヘッダとして読む
	// bufには、十分な容量のある作業メモリを与えること。
	bool read_header( ZipLocalHeader* hdr, unsigned char* buf=NULL );
	// ヘッダ位置を探し出して読む
	bool doHeader( ZipLocalHeader* hdr,unsigned char* buf=NULL );
};

#endif
