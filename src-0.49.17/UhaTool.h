#ifndef AFX_UHATOOL_H__UHA_TOOL_INCLUDED__
#define AFX_UHATOOL_H__UHA_TOOL_INCLUDED__

#include "Tool.h"

class CUhaTool : public CTool
{
//
// UHARC 0.2-0.6 (UHA\x02/\x03/\x04/\x06). The format is described in
// FORMAT.md in the unuharc/ project; the clean-room decoder lives in
// uharc/. One provider covers all four container versions.
//

public:
	CUhaTool() : CTool( "Uha Extract" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"uha");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
