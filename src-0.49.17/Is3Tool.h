#ifndef AFX_IS3TOOL_H__IS3_TOOL_INCLUDED__
#define AFX_IS3TOOL_H__IS3_TOOL_INCLUDED__

#include "Tool.h"

// InstallShield 3 ".Z" archives.  See Is3Decode.h for the format.
class CIs3Tool : public CTool
{
public:
	CIs3Tool() : CTool( "Is3 Extract" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"Z") || 0==stricmp(ext,"is3");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
