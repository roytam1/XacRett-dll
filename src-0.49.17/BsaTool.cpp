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
	int pending = -1;	// the first entry skipped but not decoded yet
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

		// A directory entry only makes its directory: never with -j
		// (the name would be flattened, and zip makes none either), and
		// only when the entry itself was asked for -- otherwise an
		// extraction of one file would still leave the archive's empty
		// directories behind.
		if( e->isdir )
		{
			if( !kiutil::m_nodir && CheckStr(outname) )
			{
				name = kiutil::pathMake( outname );
				if( name )
					CreateDirectory( name, NULL );
			}
			continue;
		}

		// The LZ windows are shared by the entries of one archive, so a
		// member that is not wanted still has to be decoded before the
		// next one is, or a later member that refers back into it fails
		// its checksum.  Deferred, so that a selection that matches
		// nothing decodes nothing.
		if( !CheckStr(outname) )
		{
			if( pending < 0 )
				pending = i;
			continue;
		}

		// The name is flattened BEFORE the overwrite test, so that with
		// -j an existing output file is found under the name it is
		// really written to.  (Zip flattens first as well, through
		// CTool::fopen.)
		name = kiutil::pathMake( outname );
		if( !name )
		{
			if( pending < 0 )
				pending = i;
			continue;
		}

		if( arcproc )//SetOwnerWindow
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,name);
			if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
			{
				// the file is kept, but the member still has to be
				// decoded for the members that come after it
				if( pending < 0 )
					pending = i;
				continue;
			}
		}

		// catch up with the entries that were skipped
		if( pending >= 0 )
		{
			int j;
			for( j=pending; j<i; j++ )
				BsaDecodeDiscard( &arc, j );
			pending = -1;
		}

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
