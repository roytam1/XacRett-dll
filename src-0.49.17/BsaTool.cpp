// The BSA/BSN provider.  See BsaDecode.h for the format.

#include "stdafx.h"
#include "BsaTool.h"
#include "BsaDecode.h"
#include "kiutil.h"
#include "XacRett.h"

// A BSA entry header is 22 fixed bytes plus the name (and its NUL), and
// the little-endian length field at offset 0 repeats that length.
#define BSA_HDR_FIXED	22

bool CBsaTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	// BSN: the global header
	if( siz>=6 && hdr[0]==0xFF && hdr[1]=='B' && hdr[2]=='S' && hdr[3]=='G' )
		return true;

	// BSA: no global header, so the first entry has to be checked: a
	// little-endian header length, 0xAE, and a length that matches the
	// fixed part plus the name
	if( siz < (unsigned long)BSA_HDR_FIXED + 1 )
		return false;
	if( hdr[2]!=0xAE )
		return false;
	{
		unsigned long hlen = (unsigned long)hdr[0] | ((unsigned long)hdr[1]<<8);
		unsigned long namelen = hdr[22];
		if( hlen != BSA_HDR_FIXED + namelen )
			return false;
		if( siz < BSA_HDR_FIXED + namelen + 1 )
			return false;
	}
	return true;
}

bool CBsaTool::Extract( const char* fname,const char* ddir )
{
	BsaArchive arc;
	int i;
//--------------------------------------------------------------------//

	if( !BsaOpen( fname, &arc ) )
		return false;

	for( i=0; i<arc.nentry; i++ )
	{
		BsaEntry* e = &arc.entries[i];
		char outname[MAX_PATH];
		char* name;

		if( m_Endflg ) break;

		lstrcpyn( outname, e->name, (int)sizeof(outname) );

		// a directory entry only makes its directory
		if( e->isdir )
		{
			name = kiutil::pathMake( outname );
			if( name )
				CreateDirectory( name, NULL );
			continue;
		}

		if( !CheckStr(outname) )
			continue;
		if( arcproc )//SetOwnerWindow
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,outname);
			if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
				continue;
		}
		name = kiutil::pathMake( outname );
		if( !name )
			continue;
		if( !BsaExtractFile( &arc, i, name ) )
		{
			// an unsupported method, a decode failure or a checksum
			// mismatch: the partial file is not left behind
			DeleteFile( name );
			BsaClose( &arc );
			return false;
		}
		// the time stamp of the entry is a DOS date and time
		kiutil::timeSet( name, (WORD)(e->ts >> 16), (WORD)(e->ts & 0xFFFF) );
	}
	BsaClose( &arc );
	return true;
}
