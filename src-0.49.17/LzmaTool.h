#ifndef AFX_LZMATOOL_H__7A3B9C1D_4E22_41F0_9D8A_2B7C4E6F8A01__INCLUDED_
#define AFX_LZMATOOL_H__7A3B9C1D_4E22_41F0_9D8A_2B7C4E6F8A01__INCLUDED_

#include "Tool.h"

class CLzmaTool : public CTool
{
public:
	CLzmaTool() : CTool( "LZMA “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"lzma") || 0==stricmp(ext,"tlz");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

// Whole-file LZMA-Alone decode, shared with .tar.lzma (TarOpen).
// Returns true on a clean decode (exact size when known).
bool LzmaDecodeFile( const char* src, const char* dst );

#endif
