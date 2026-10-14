#ifndef AFX_7ZTOOL_H__3A9D5E71_4C82_4F16_B0D7_5E28A1C93F44__INCLUDED_
#define AFX_7ZTOOL_H__3A9D5E71_4C82_4F16_B0D7_5E28A1C93F44__INCLUDED_

#include "Tool.h"

class C7zTool : public CTool
{
public:
	C7zTool() : CTool( "7z “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"7z");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif