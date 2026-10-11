#ifndef AFX_LBRTOOL_H__A1B2C3D4_E5F6_4A7B_8C9D_0E1F20304050__INCLUDED_
#define AFX_LBRTOOL_H__A1B2C3D4_E5F6_4A7B_8C9D_0E1F20304050__INCLUDED_

#include "Tool.h"

class CLbrTool : public CTool
{
public:
	CLbrTool() : CTool( "Lbr “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"lbr");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname,const char* ddir );
};

#endif
