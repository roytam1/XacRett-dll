#ifndef AFX_EGGTOOL_H__EGG_TOOL_INCLUDED__
#define AFX_EGGTOOL_H__EGG_TOOL_INCLUDED__

#include "Tool.h"
#include "EggDecode.h"


// EGG (ALZip) extraction.  The container and the five block methods
// (store, deflate, bzip2, AZO, LZMA) are in EggDecode/EggAzo; this class
// only maps the members to files and drives the sink.
class CEggTool : public CTool
{
public:
	CEggTool() : CTool( "Egg \x93W\x8aJ" )
		{}

	bool IsType( const char* ext )
		{ return 0==stricmp(ext,"egg"); }
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
