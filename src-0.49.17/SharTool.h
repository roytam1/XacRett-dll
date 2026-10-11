#ifndef AFX_SHARTOOL_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_SHARTOOL_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_

#include "Tool.h"

class CSharTool : public CTool
{
public:
	CSharTool()	: CTool( "Shar “WŠJ" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
