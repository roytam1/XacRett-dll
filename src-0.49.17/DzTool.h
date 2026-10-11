#ifndef AFX_DZTOOL_H__010F029E_EFA6_4EC4_BA1A_6E7C0614AB67__INCLUDED_
#define AFX_DZTOOL_H__010F029E_EFA6_4EC4_BA1A_6E7C0614AB67__INCLUDED_

#include "Tool.h"
#include "zlib/zlib.h"

enum
{
	TYPE_Normal,
	TYPE_Demv1,
	TYPE_Txt,
	TYPE_Pak,
	TYPE_Dz,
	TYPE_Dem,
	TYPE_Nehahra,
	TYPE_Dir,
	TYPE_Store,
	TYPE_Last
};

class CDzTool : public CTool  
{
//
// original source is
//
// Dzip v2.5 (23rd January 2001)
//   http://planetquake.com/sda/dzip/
// (C) 2000 Stefan Schwoon, Nolan Pflug
//
/*
Copyright notice:

 (C) 2000 Stefan Schwoon, Nolan Pflug

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Stefan Schwoon	Nolan Pflug
  schwoon@in.tum.de	radix@planetquake.com
*/

//
// using
//
// zlib 1.1.3
//   http://www.info-zip.org/pub/infozip/zlib/
// Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler
//
// see "zlib/zlib.h"
//

public:
	CDzTool() : CTool( "Dzip 展開" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"dz");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	struct fileInf
	{
		unsigned int ptr;
		unsigned int csz, usz, isz;
		char         nam[MAX_PATH];

		unsigned short pak;
		unsigned int type;
		unsigned int date;
	};

	bool decodeAll( fileInf* fi, unsigned int fn, bool ver1, unsigned long datend );

	// 標準的な入出力
	void writeOut( void* p, unsigned int s );
	FILE *wp, *dz;

	// Pak入出力
	struct pakentry
	{
		char name[56];
		int ptr;
		int len;
	};
	FILE*     pak;
	fileInf*  pakinf;
	pakentry* pakEnt;
	FILE* createPak( fileInf& fi );
	FILE* addPak( fileInf& fi );
	void  closePak();

	// zlib入出力
	z_stream_s zs;
	unsigned int ztotal;
	void init_zlib( unsigned long flen );
	void end_zlib();
	void fp_to_zlbuf();
	int read_from_zlib( int siz, unsigned char* ob=NULL, unsigned int obs=0 );

	// demo_uncompress
	void demDecode( unsigned int siz, int type );
	int  decBlock();
};

#endif
