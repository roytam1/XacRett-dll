#ifndef AFX_QIPTOOL_H__QIP_TOOL_INCLUDED__
#define AFX_QIPTOOL_H__QIP_TOOL_INCLUDED__

#include "Tool.h"

// The QIP archives of the QEMM386 / QPACK installers: a "QP" container
// (a header, an index and a chain of "QD" blocks) or a raw "QD" archive.
// See QipDecode.h for the format.

class CQipTool : public CTool
{
public:
	CQipTool() : CTool( "Qip Extract" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"qip");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
