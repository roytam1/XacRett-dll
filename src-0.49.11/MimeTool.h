#ifndef AFX_MIMETOOL_H__7D026A64_3EB5_11D4_8D96_A8F0CDA4C435__INCLUDED_
#define AFX_MIMETOOL_H__7D026A64_3EB5_11D4_8D96_A8F0CDA4C435__INCLUDED_

#include "Tool.h"

class CMimeTool : public CTool
{
public:
	CMimeTool() : CTool( "Base64 Decode" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool DoHeader( FILE* in,char* name,int count=1000000000 );
	void DoData( FILE* in,FILE* out );
	void skip(FILE* fp);//í«â¡Å@By Bonty
};

#endif
