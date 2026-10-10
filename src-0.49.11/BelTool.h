#ifndef AFX_BELTOOL_H__F143A063_398D_11D4_8D96_87647745DB39__INCLUDED_
#define AFX_BELTOOL_H__F143A063_398D_11D4_8D96_87647745DB39__INCLUDED_

#include "Tool.h"

class CBelTool : public CTool
{
//
// original source is
//
// Belon
//   http://www.vector.co.jp/soft/win95/util/se079561.html
// Copyright (c) 1998 N.I
//

public:
	CBelTool() : CTool( "Belon “WŠJ" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
