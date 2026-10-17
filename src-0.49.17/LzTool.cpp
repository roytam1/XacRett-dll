#include "stdafx.h"
#include "LzTool.h"
#include "LzDecode.h"


bool CLzTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	int base_log;
	(void)fname;
	if( fsize < 6 )
		return false;
	if( hdr[0]!='L' || hdr[1]!='Z' || hdr[2]!='I' || hdr[3]!='P' )
		return false;
	if( hdr[4] != 1 )                              // version 1 for now
		return false;
	// DS: the base-2 log of the dictionary base size must be in 12..29,
	// and a corrupt DS is the cheapest way to spot a bad file up front.
	base_log = hdr[5] & 0x1F;
	return base_log >= 12 && base_log <= 29;
}


bool CLzTool::Extract( const char* fname,const char* ddir )
{
	char outname[MAX_PATH];
	(void)ddir;
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"lz") ? NULL : "unlz" );
	if (m_Endflg) return true;
	if (!CheckStr(outname))return true;
	char *name = kiutil::pathMake( outname );
	if( !LzDecodeFile( fname, name ) )
		return false;
	return true;
}