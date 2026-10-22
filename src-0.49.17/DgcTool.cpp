
#include "stdafx.h"
#include "DgcTool.h"
#include "DgcDecode.h"
#include "kiutil.h"
#include "XacRett.h"

// The container of a DGCA archive starts with the "DGCA" chunk, whose
// header is 32 bytes.
#define DGC_CHUNK_HDR	32

bool CDgcTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<DGC_CHUNK_HDR )
		return false;
	if( hdr[0]!='D' || hdr[1]!='G' || hdr[2]!='C' || hdr[3]!='A' )
		return false;
	// the header size of a chunk is always 0x20
	if( hdr[4]!=0x20 || hdr[5]!=0 || hdr[6]!=0 || hdr[7]!=0 )
		return false;
	return true;
}

bool CDgcTool::Extract( const char* fname,const char* ddir )
{
	DgcArchive arc;
	int i;
//--------------------------------------------------------------------//

	if( !DgcOpen( fname, &arc, 0 ) )
	{
		if( !arc.encrypted )
			return false;
		// An encrypted archive: the password is asked for, but only
		// now, so that an archive that is not encrypted never opens
		// the dialog.
		{
			std::string pw = DgcPasswordFromArg( CTool::GetPassword() );
			if( pw.empty() )
				return false;
			if( !DgcOpen( fname, &arc, pw.c_str() ) )
				return false;
		}
	}

	for( i=0; i<arc.nentry; i++ )
	{
		DgcEntry* e = &arc.entries[i];
		char outname[MAX_PATH];
		char tmp[MAX_PATH];
		char* name;

		if( m_Endflg ) break;

		// The names are UTF-8 and the paths use backslashes.
		if( !kiutil::utf8toAnsi(tmp,(int)sizeof(tmp),e->name) )
			lstrcpyn(tmp,e->name,(int)sizeof(tmp));
		lstrcpyn(outname,tmp,(int)sizeof(outname));

		// a directory entry has no data of its own, but its
		// directories are still made
		if( e->attr & 0x10 )
		{
			name = kiutil::pathMake( outname );
			if( name ) {
				// pathMake made the directories
			}
			continue;
		}

		//--------------------------------------------------------------------//
		if (!CheckStr(outname))
		{
			continue;
		}
		if(arcproc)//SetOwnerWindow
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,outname);
			if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
				continue;
		}
		//--------------------------------------------------------------------//
		name = kiutil::pathMake( outname );
		if( !name )
			continue;
		if( !DgcExtractFile( &arc, i, name ) )
		{
			// the contents did not match the index, so the file is
			// not left behind
			DeleteFile( name );
			DgcClose( &arc );
			return false;
		}
		// the time stamp of the entry
		{
			FILETIME ft;
			LARGE_INTEGER li;
			li.QuadPart = (LONGLONG)e->mtime;
			ft.dwLowDateTime = li.LowPart;
			ft.dwHighDateTime = li.HighPart;
			kiutil::timeSet( name, &ft );
		}
	}
	DgcClose( &arc );
	return true;
}
