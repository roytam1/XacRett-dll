#ifndef AFX_LZTOOL_H__1D7C4E88_6B32_4A19_9E5F_0C8A2D4B7F31__INCLUDED__
#define AFX_LZTOOL_H__1D7C4E88_6B32_4A19_9E5F_0C8A2D4B7F31__INCLUDED__


#include "Tool.h"

class CLzTool : public CTool
{
public:
	CLzTool() : CTool( "LZIP “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"lz") || 0==stricmp(ext,"tlz");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};


#endif