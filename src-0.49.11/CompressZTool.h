#ifndef AFX_COMPRESSZTOOL_H__7D026A62_3EB5_11D4_8D96_A8F0CDA4C435__INCLUDED_
#define AFX_COMPRESSZTOOL_H__7D026A62_3EB5_11D4_8D96_A8F0CDA4C435__INCLUDED_

#include "Tool.h"

class CCompressZTool : public CTool
{
// see complib.h

public:
	CCompressZTool() : CTool( "Compress “WŠJ" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
