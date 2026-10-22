#ifndef AFX_DGCTOOL_H__DGC_TOOL_INCLUDED__
#define AFX_DGC_TOOL_H__DGC_TOOL_INCLUDED__

#include "Tool.h"

class CDgcTool : public CTool
{
//
// using
//
// DGCA (the "DGC Archiver") v1.10
//   Copyright (C) 2000,2005 SYN (Shin-ichi TSURUTA)
//
// The format is described in dgca/NOTES-dgca-format.md, which was
// reverse engineered from dgcac.exe.
//

public:
	CDgcTool() : CTool( "Dgc “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"dgc");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
