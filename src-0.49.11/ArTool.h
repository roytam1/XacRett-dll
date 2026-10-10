#ifndef AFX_ARTOOL_H__F5993B2C_314D_4A56_BCAB_9A4B93E0702C__INCLUDED_
#define AFX_ARTOOL_H__F5993B2C_314D_4A56_BCAB_9A4B93E0702C__INCLUDED_

#include "Tool.h"

class CArTool : public CTool
{
public:
	CArTool() : CTool( "Deb/Ar “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"deb");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
