#ifndef AFX_UUETOOL_H__839F2EA0_3E45_11D4_8D96_B9FCCFB20032__INCLUDED_
#define AFX_UUETOOL_H__839F2EA0_3E45_11D4_8D96_B9FCCFB20032__INCLUDED_

#include "Tool.h"

class CUnixDecoderTool : public CTool
{
public:
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

protected:
	CUnixDecoderTool( const char* r,char a,char b )
		: CTool(r), x(a), X(b) {}
	virtual void InitTable() = 0;
	char x,X,table[128];
};

class CUueTool : public CUnixDecoderTool
{
public:
	CUueTool() : CUnixDecoderTool( "UUDecode",'u','U' ) {}
protected:
	void InitTable()
	{
		memset( table,0,sizeof(table) );
		for( int n=0; n!=64; n++ )
			table[ ' '+n ] = n;
	}
};

class CXxeTool : public CUnixDecoderTool
{
public:
	CXxeTool() : CUnixDecoderTool( "XXDecode",'x','X' ) {}
protected:
	void InitTable()
	{
		const char set[] =
		"+-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
		memset( table,0,sizeof(table) );
		for( int n=0 ; n!=64; n++ )
			table[ set[n] ] = n;
	}
};

#endif
