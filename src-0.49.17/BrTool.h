#ifndef AFX_BRRTOOL_H__7A1F3C9E_2B48_4D5A_8E1D_5F6A7B8D1__INCLUDED_
#define AFX_BRRTOOL_H__7A1F3C9E_2B48_4D5A_8E1D_5F6A7B8D1__INCLUDED_

#include "Tool.h"

// Standalone Brotli (.br).  A Brotli stream has no magic and no size
// field, so Check is a bounded decode of the first few KB; registration
// is late enough that the strict-magic providers still win pass 1.
class CBrTool : public CTool
{
public:
	CBrTool() : CTool( "Brotli Extract" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"br") || 0==stricmp(ext,"tbr");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
