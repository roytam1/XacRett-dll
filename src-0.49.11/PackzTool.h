#ifndef AFX_PACKZTOOL_H__C32553E0_3E56_11D4_8D96_B9FCCFB20032__INCLUDED_
#define AFX_PACKZTOOL_H__C32553E0_3E56_11D4_8D96_B9FCCFB20032__INCLUDED_

#include "Tool.h"

class CPackzTool : public CTool
{
// see complib.h

public:
	CPackzTool() : CTool( "Pack “WŠJ" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
