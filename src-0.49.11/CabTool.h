#ifndef AFX_CABTOOL_H__7CA00740_39A0_11D4_8D96_871A6CA2BE31__INCLUDED_
#define AFX_CABTOOL_H__7CA00740_39A0_11D4_8D96_871A6CA2BE31__INCLUDED_

#include "Tool.h"

class CCabTool : public CTool
{
//
// using
//
// cabinet-sdk
//   http://msdn.microsoft.com/workshop/management/cab/cabdl.asp
// by Microsoft
//

public:
	CCabTool() : CTool( "Cab “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"cab");}

	bool Check( const char* fname, unsigned long fsize);
	bool Extract( const char* fname, const char* ddir );

private:
	int FindHeader( const char* fname, const BYTE* hdr, DWORD siz );
};

#endif
