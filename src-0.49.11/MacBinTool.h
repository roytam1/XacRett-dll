#ifndef AFX_MACBINTOOL_H__B79B751D_C16F_4C40_A5E7_03CCCE4F19FC__INCLUDED_
#define AFX_MACBINTOOL_H__B79B751D_C16F_4C40_A5E7_03CCCE4F19FC__INCLUDED_

#include "Tool.h"

class CMacBinTool : public CTool
{
//
// special thanks to
//
// MacBinary Specification
//   http://www.lazerware.com/formats/macbinary.html
// and
//   "MacBinary II+ Preliminary Specification" (Sorry, I forgot where I've got it.)
//

public:
	CMacBinTool() : CTool( "MacBinary decode" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"bin");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool easyCheck( const unsigned char* hdr );
	bool doHeader( FILE* bin, char* name, DWORD* siz, int* offset, int* dirflag, DWORD* time );
	void doContent( FILE* bin, FILE* out, DWORD len );
};

#endif
