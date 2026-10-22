#ifndef AFX_DGDTOOL_H__DGD_SPANNED_FILE_INCLUDED__
#define AFX_DGDTOOL_H__DGD_SPANNED_FILE_INCLUDED__

#include "Tool.h"

// The ".dgd" and ".dgdr" files of the "DGC Archiver" hold a file that
// is too big for a single archive, split into parts.  The parts are put
// together again and the file is written under the name that the INFO
// chunk of the parts carries.

class CDgdTool : public CTool
{
public:
	CDgdTool() : CTool( "Dgd “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"dgd") || 0==stricmp(ext,"dgdr");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
