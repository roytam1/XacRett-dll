#include "stdafx.h"
#include "BrTool.h"
#include "BrotliDecode.h"

bool CBrTool::Check( const char* fname, unsigned long fsize )
{
	if( fsize==0 ) return false;
	return BrotliCheckFile( fname, fsize );
}

bool CBrTool::Extract( const char* fname,const char* ddir )
{
	char outname[MAX_PATH];
	(void)ddir;
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"br") ? NULL : "unbr" );
	if (m_Endflg) return true;
	if (!CheckStr(outname))return true;
	char *name = kiutil::pathMake( outname );
	if( !BrotliDecodeFile( fname, name ) )
	{
		DeleteFile( name );
		return false;
	}
	return true;
}
