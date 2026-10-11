#ifndef AFX_ATOBTOOL_H__F0336381_3E5C_11D4_8D96_EC1A93AF0635__INCLUDED_
#define AFX_ATOBTOOL_H__F0336381_3E5C_11D4_8D96_EC1A93AF0635__INCLUDED_

#include "Tool.h"

class CAtoBTool : public CTool
{
public:
	CAtoBTool()	: CTool( "Ascii to Binary •ÏŠ·" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
