#ifndef AFX_REZTOOL_H__1FC840CD_1915_402E_8C02_EEFF48ABE1D4__INCLUDED_
#define AFX_REZTOOL_H__1FC840CD_1915_402E_8C02_EEFF48ABE1D4__INCLUDED_

#include "Tool.h"

class CRezTool : public CTool
{
public:
	CRezTool() : CTool( "Rez åãçá" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"rez");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
