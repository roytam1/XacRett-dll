#ifndef AFX_ZACTOOL_H__DC72C0F0_7D3A_4DFB_91E7_7C54A63A7FA1__INCLUDED_
#define AFX_ZACTOOL_H__DC72C0F0_7D3A_4DFB_91E7_7C54A63A7FA1__INCLUDED_

#include "Tool.h"

class CZacTool : public CTool
{
public:
	CZacTool() : CTool( "Zac “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"zac");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
