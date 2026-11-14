// The QIP provider.  See QipDecode.h for the format.

#include "stdafx.h"
#include "QipTool.h"
#include "QipDecode.h"
#include "kiutil.h"
#include "XacRett.h"

bool CQipTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	(void)fname;
//--------------------------------------------------------------------//

	if( siz < 16 )
		return false;

	if( hdr[0]=='Q' && hdr[1]=='P' )
	{
		// the "QP" container: a version, a number of index entries and
		// the size of the index
		unsigned long count = (unsigned long)hdr[2] | ((unsigned long)hdr[3]<<8);
		unsigned long ver   = (unsigned long)hdr[8] | ((unsigned long)hdr[9]<<8);
		unsigned long isz   = (unsigned long)hdr[4] | ((unsigned long)hdr[5]<<8) |
							  ((unsigned long)hdr[6]<<16) | ((unsigned long)hdr[7]<<24);
		if( ver != 1 && ver != 2 ) return false;
		if( count == 0 || count > QIP_MAX_FILES ) return false;
		if( isz != count * 16UL ) return false;
		if( siz < 16 * (count + 1) ) return false;
		return true;
	}

	if( hdr[0]=='Q' && hdr[1]=='D' )
	{
		// a raw (version 1) archive: the first block has to be a "QD"
		// header whose sizes fit into the file
		unsigned long type  = (unsigned long)hdr[2] | ((unsigned long)hdr[3]<<8);
		unsigned long csize = (unsigned long)hdr[4] | ((unsigned long)hdr[5]<<8) |
							  ((unsigned long)hdr[6]<<16) | ((unsigned long)hdr[7]<<24);
		if( type > 1 ) return false;
		if( type == 1 )
		{
			// a directory: the name follows the header
			if( csize == 0 || csize >= MAX_PATH ) return false;
			if( csize > siz - 10 ) return false;
			return true;
		}
		// a file: the 22 byte record and then the compressed data
		if( siz < 10 + 22 ) return false;
		if( csize > siz - 10 - 22 ) return false;
		{
			// the name of the record has to be a plain, NUL terminated
			// string, otherwise this is not a QIP archive
			const unsigned char* rec = hdr + 10;
			int i;
			for( i=0; i<13; i++ )
			{
				if( rec[9+i] == 0 ) break;
				if( rec[9+i] < 0x20 || rec[9+i] > 0x7E ) return false;
			}
			if( i >= 13 ) return false;
		}
		return true;
	}

	return false;
}

bool CQipTool::Extract( const char* fname, const char* ddir )
{
	QipArchive arc;
	int i;
	(void)ddir;
//--------------------------------------------------------------------//

	if( !QipOpen( fname, &arc ) )
		return false;

	for( i=0; i<arc.nfile; i++ )
	{
		QipFile* f = &arc.files[i];
		char outname[MAX_PATH];
		char* name;

		if( m_Endflg ) break;

		lstrcpyn( outname, f->path, (int)sizeof(outname) );

		if( !CheckStr(outname) )
			continue;

		name = kiutil::pathMake( outname );
		if( !name )
			continue;

		if( arcproc )//SetOwnerWindow
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,name);
			if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
				continue;	// the file is kept
		}

		if( !QipExtract( &arc, i, name ) )
		{
			// a decode failure, a size mismatch or a checksum
			// mismatch: the partial file is not left behind
			DeleteFile( name );
			QipClose( &arc );
			return false;
		}
	}
	QipClose( &arc );
	return true;
}
