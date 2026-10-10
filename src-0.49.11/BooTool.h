#ifndef AFX_BOOTOOL_H__F0336383_3E5C_11D4_8D96_EC1A93AF0635__INCLUDED_
#define AFX_BOOTOOL_H__F0336383_3E5C_11D4_8D96_EC1A93AF0635__INCLUDED_

#include "Tool.h"

class CBooTool : public CTool
{
public:
	CBooTool() : CTool( "Boo Decode" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
