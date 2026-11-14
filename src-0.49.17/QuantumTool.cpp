// The Quantum provider.  See QuantumDecode.h for the format.

#include "stdafx.h"
#include "QuantumTool.h"
#include "QuantumDecode.h"
#include "kiutil.h"
#include "XacRett.h"

// Walk the file table of the archive.  The result is
//   0  the table fits into the buffer
//   1  the table does not fit (the file is not a Quantum archive)
//   2  the buffer was not enough to tell
static int quantum_scan_table( const unsigned char* p, unsigned long siz,
							   int nfiles, unsigned long* used )
{
	unsigned long pos = 8;
	int i;

	for( i=0; i<nfiles; i++ )
	{
		unsigned long n;

		// the name
		if( pos >= siz ) return 2;
		n = p[pos++];
		if( n >= 0x80 )
		{
			if( pos >= siz ) return 2;
			n = ((n & 0x7F) << 8) | p[pos++];
		}
		if( pos + n > siz ) return 1;
		pos += n;

		// the comment
		if( pos >= siz ) return 2;
		n = p[pos++];
		if( n >= 0x80 )
		{
			if( pos >= siz ) return 2;
			n = ((n & 0x7F) << 8) | p[pos++];
		}
		if( pos + n > siz ) return 1;
		pos += n;

		// the size, the time and the date
		if( pos + 8 > siz ) return 1;
		pos += 8;
	}
	*used = pos;
	return 0;
}

bool CQuantumTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	unsigned long used = 0;
	int nfiles, windowbits;
	int r;
	(void)fname;
//--------------------------------------------------------------------//

	if( siz < 8 )
		return false;

	// the magic is "DS" (0x5344)
	if( hdr[0] != 'D' || hdr[1] != 'S' )
		return false;

	nfiles     = (int)hdr[4] | ((int)hdr[5] << 8);
	windowbits = (int)hdr[6];
	if( nfiles <= 0 || nfiles > QUANTUM_MAX_FILES )
		return false;
	if( windowbits < 10 || windowbits > 21 )
		return false;

	r = quantum_scan_table( hdr, siz, nfiles, &used );
	if( r == 1 ) return false;
	// the table could not be walked to its end: that is only a problem
	// when the whole file was in the buffer
	if( r == 2 && siz >= fsize ) return false;
	return true;
}

bool CQuantumTool::Extract( const char* fname, const char* ddir )
{
	QuantumArchive arc;
	int i;
	(void)ddir;
//--------------------------------------------------------------------//

	if( !QuantumOpen( fname, &arc ) )
		return false;

	for( i=0; i<arc.nfile; i++ )
	{
		QuantumFile* f = &arc.files[i];
		char outname[MAX_PATH];
		char* name = NULL;
		bool wanted;

		if( m_Endflg ) break;

		lstrcpyn( outname, f->name, (int)sizeof(outname) );

		// The models and the coder state of the archive carry over from
		// one file to the next, so every file has to be decoded, also
		// the ones that are not wanted.
		wanted = ( !f->badname && CheckStr(outname) );
		if( wanted )
		{
			name = kiutil::pathMake( outname );
			if( !name ) wanted = false;
		}

		if( wanted && arcproc )//SetOwnerWindow
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,name);
			if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
				wanted = false;		// the file is kept
		}

		if( !QuantumDecodeMember( &arc, i, wanted ? name : NULL ) )
		{
			// a decode failure or a checksum mismatch: the partial file
			// is not left behind
			if( wanted && name )
				DeleteFile( name );
			QuantumClose( &arc );
			return false;
		}
	}
	QuantumClose( &arc );
	return true;
}
