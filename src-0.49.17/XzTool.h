#ifndef AFX_XZTOOL_H__8B4C1D2E_5F33_42A1_9E9B_3C8D5F7A0022__INCLUDED_
#define AFX_XZTOOL_H__8B4C1D2E_5F33_42A1_9E9B_3C8D5F7A0022__INCLUDED_

#include "Tool.h"

class CXzTool : public CTool
{
public:
	CXzTool() : CTool( "XZ “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"xz") || 0==stricmp(ext,"txz");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

// Whole-file XZ decode, shared with .tar.xz (TarOpen). One complete
// XZ stream; size comes from the stream itself. True on full validation.
bool XzDecodeFile( const char* src, const char* dst );

#endif
