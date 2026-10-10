#ifndef AFX_QPAKTOOL_H__4AF7C920_AE95_11D4_8D97_00909909C163__INCLUDED_
#define AFX_QPAKTOOL_H__4AF7C920_AE95_11D4_8D97_00909909C163__INCLUDED_

#include "Tool.h"

struct PakIndex
{
	char  fname[0x78];
	DWORD offset;
	DWORD size;
};

class CQPakTool : public CTool
{
//
// original source is
//
// QuakePAK v1.5 plugin for FAR
//   http://esk.nm.ru/far/
// Copyright(c) 1999-2000 Eugene Kotlyarov 
//

public:
	CQPakTool()	: CTool( "QuakePak/WAD/Pff “WŠJ" )
		{pak=NULL,entry=NULL;}

	~CQPakTool()
		{Close();}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"pak")
			 || 0==stricmp(ext,"wad") || 0==stricmp(ext,"pff");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool Open( const char* fname );
	void Close();

	FILE* pak;
	PakIndex *entry;
	DWORD count;
};

#endif
