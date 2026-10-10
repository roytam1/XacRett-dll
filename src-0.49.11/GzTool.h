#ifndef AFX_GZTOOL_H__14B1B7E0_3A46_11D4_8D96_BA2DDC056038__INCLUDED_
#define AFX_GZTOOL_H__14B1B7E0_3A46_11D4_8D96_BA2DDC056038__INCLUDED_

#include "Tool.h"

class CGzTool : public CTool
{
//
// using
//
// zlib 1.1.4
//   http://www.info-zip.org/pub/infozip/zlib/
// Copyright (C) 1995-2002 Jean-loup Gailly and Mark Adler
//
// see "zlib/zlib.h"
//

public:
	CGzTool() : CTool( "Gzip “WŠJ" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
