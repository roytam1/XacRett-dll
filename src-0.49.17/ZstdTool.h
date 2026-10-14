#ifndef AFX_ZSTDTOOL_H__7A1F3C9E_2B48_4D5A_8E1D_5F6A7B8C9D0E1__INCLUDED_
#define AFX_ZSTDTOOL_H__7A1F3C9E_2B48_4D5A_8E1D_5F6A7B8C9D0E1__INCLUDED_

#include "Tool.h"

class CZstdTool : public CTool
{
public:
	CZstdTool() : CTool( "ZSTD “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"zst") || 0==stricmp(ext,"tzst");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

// Whole-file ZSTD decode, shared with .tar.zst (TarOpen). Streaming,
// multi-frame tolerant, strict tail (trailing garbage or a truncated
// tail fails). True on full validation, like XzDecodeFile.
bool ZstdDecodeFile( const char* src, const char* dst );

#endif
