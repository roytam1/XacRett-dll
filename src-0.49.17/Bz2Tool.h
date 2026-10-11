#ifndef AFX_BZ2TOOL_H__5015A720_3A09_11D4_8D96_9C430C9F5A32__INCLUDED_
#define AFX_BZ2TOOL_H__5015A720_3A09_11D4_8D96_9C430C9F5A32__INCLUDED_

#include "Tool.h"

class CBz2Tool : public CTool
{
//
// using
//
// libbzip2 1.0.1
//   http://sources.redhat.com/bzip2/
// Copyright (C) 1996-2000 Julian R Seward.  All rights reserved.
//
// see "libbzip2/LICENSE"
//

public:
	CBz2Tool() : CTool( "Bzip2 “WŠJ" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
