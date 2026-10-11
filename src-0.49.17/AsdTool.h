#ifndef AFX_ASDTOOL_H__1F4CD8DD_6E50_4097_8562_CAD572D1387F__INCLUDED_
#define AFX_ASDTOOL_H__1F4CD8DD_6E50_4097_8562_CAD572D1387F__INCLUDED_

#include "Tool.h"

class CAsdTool : public CTool
{
//
// original source is
//
// UNASD VERSIN 0.1.4 ( Dec 1996 )
//   http://hem1.passagen.se/svto
// (C) 1996 TOPOLOGI SOFTWARE
//

public:
	CAsdTool() : CTool( "Asd “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"asd");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool expand_data( FILE* in, FILE* out, unsigned long len );
	void init_expander( int x );
	int wptr, xptr, verylong;
};

#endif
