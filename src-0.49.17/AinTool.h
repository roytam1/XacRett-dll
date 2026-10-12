#ifndef AFX_AINTOOL_H__B2C3D4E5_F6A7_4B8C_9D0E_1F2A30415261__INCLUDED_
#define AFX_AINTOOL_H__B2C3D4E5_F6A7_4B8C_9D0E_1F2A30415261__INCLUDED_

#include "Tool.h"

class CAinTool : public CTool
{
public:
	CAinTool() : CTool( "Ain “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"ain");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname,const char* ddir );
};

#endif
