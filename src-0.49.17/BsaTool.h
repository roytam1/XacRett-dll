#ifndef AFX_BSATOOL_H__BSA_TOOL_INCLUDED__
#define AFX_BSATOOL_H__BSA_TOOL_INCLUDED__

#include "Tool.h"

class CBsaTool : public CTool
{
//
// The game data archives that bsa32.exe handles: ".bsn" (a global
// "FF 42 53 47" header, big-endian fields, per entry CRC-32) and ".bsa"
// (no global header, little-endian entry headers, per entry byte-sum
// checksum).  The formats were reverse engineered in the bsabsn/
// project; see BsaDecode.h.
//

public:
	CBsaTool() : CTool( "BSA/BSN Extract" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"bsa") || 0==stricmp(ext,"bsn");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
