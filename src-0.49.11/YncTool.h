#ifndef AFX_YNCTOOL_H__699E3660_0DBE_4E43_A4B0_874B9C18F1C2__INCLUDED_
#define AFX_YNCTOOL_H__699E3660_0DBE_4E43_A4B0_874B9C18F1C2__INCLUDED_

#include "Tool.h"

class CYncTool : public CTool  
{
//
// reference
//
// draft yEnc 1.0 ( 31-July-2001 )
//   http://www.winews.net/yenc/yEnc-draft-1.txt
//   http://www.yenc.org/
// by Juergen Helbing
//

public:
	CYncTool() : CTool( "yDecode" ){}
	bool IsType( const char* ext ) {return 0==stricmp(ext,"ntx");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
