// The InstallShield 3 provider.  See Is3Decode.h for the format.

#include "stdafx.h"
#include "Is3Tool.h"
#include "Is3Decode.h"
#include "kiutil.h"
#include "XacRett.h"

bool CIs3Tool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	(void)fname;
//--------------------------------------------------------------------//

	// magic 13 5D 65 8C and a 0x3A header size byte
	if( siz < 64 ) return false;
	if( hdr[0]!=0x13 || hdr[1]!=0x5D || hdr[2]!=0x65 || hdr[3]!=0x8C )
		return false;
	if( hdr[4] != 0x3A ) return false;
	// number of files (offset 5+7=12) must be sane
	{
		unsigned long nf = (unsigned long)hdr[12] | ((unsigned long)hdr[13]<<8);
		if( nf==0 || nf>IS3_MAX_FILES ) return false;
	}
	return true;
}

bool CIs3Tool::Extract( const char* fname, const char* ddir )
{
	Is3Archive arc;
	int i;
	(void)ddir;
//--------------------------------------------------------------------//

	if( !Is3Open( fname, &arc ) )
		return false;

	for( i=0; i<arc.nfile; i++ )
	{
		Is3File* f = &arc.files[i];
		char outname[MAX_PATH];
		char* name;

		if( m_Endflg ) break;

		lstrcpyn( outname, f->path, (int)sizeof(outname) );
		if( !CheckStr(outname) )
			continue;

		name = kiutil::pathMake( outname );
		if( !name )
			continue;

		if( arcproc )
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,name);
			if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
				continue;
		}

		if( !Is3Extract( &arc, i, name ) )
		{
			DeleteFile( name );
			Is3Close( &arc );
			return false;
		}
	}
	Is3Close( &arc );
	return true;
}
