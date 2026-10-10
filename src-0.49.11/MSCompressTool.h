#ifndef AFX_MSCOMPRESSTOOL_H__A8752F3C_9D85_4B6C_AC24_92526C141DC0__INCLUDED_
#define AFX_MSCOMPRESSTOOL_H__A8752F3C_9D85_4B6C_AC24_92526C141DC0__INCLUDED_

#include "Tool.h"

class CMSCompressTool : public CTool
{
//
// using Win32 API
//

public:
	CMSCompressTool() : CTool( "MS-Compress “WŠJ" ){}

	bool IsType( const char* ext )
		{return ext[strlen(ext)-1]=='_';}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
