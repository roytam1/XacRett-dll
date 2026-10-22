#include "stdafx.h"
#include "DgdTool.h"
#include "DgdDecode.h"
#include "DgcCrypt.h"
#include "kiutil.h"
#include "XacRett.h"

// The container of a .dgd file starts with the "DGCD" chunk, whose
// header is 32 bytes.
#define DGD_CHUNK_HDR	32

// The password of an encrypted file, as the command line gives it:
// -p<password> is the password itself and -p#<file> is a key file,
// whose content is hashed with SHA-512 and whose hex digest is the
// password (the same thing the DGC Archiver's -k switch does).
static std::string DgdPasswordFromArg( const char* arg )
{
	std::string s;
	if( !arg || !*arg ) return s;
	if( *arg=='#' ) {
		if( arg[1]=='#' ) return std::string(arg+1);
		{
			FILE* fp = fopen( arg+1, "rb" );
			std::string data;
			char buf[4096];
			size_t n;
			char hex[129];
			if( !fp ) return s;
			while( (n = fread(buf,1,sizeof(buf),fp)) > 0 )
				data.append(buf,n);
			fclose(fp);
			DgcSha512Hex( (const unsigned char*)data.data(),
						   (unsigned long)data.size(), hex );
			return std::string(hex);
		}
	}
	return std::string(arg);
}

bool CDgdTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	(void)fname;
//--------------------------------------------------------------------//

	if( fsize<DGD_CHUNK_HDR )
		return false;
	if( hdr[0]!='D' || hdr[1]!='G' || hdr[2]!='C' || hdr[3]!='D' )
		return false;
	// the header size of a chunk is always 0x20
	if( hdr[4]!=0x20 || hdr[5] || hdr[6] || hdr[7] )
		return false;
	return true;
}

bool CDgdTool::Extract( const char* fname,const char* ddir )
{
	DgdFile f;
	std::string pw;
	char outname[MAX_PATH];
	char* name;
	FILE* out;
//--------------------------------------------------------------------//

	// The password of the command line is used straight away, so that an
	// encrypted file is recognised without a dialog.
	pw = DgdPasswordFromArg( CTool::m_pPassArg );
	if( !DgdOpen( fname, pw.empty() ? 0 : pw.c_str(), &f ) )
	{
		if( !f.encrypted )
			return false;
		// An encrypted file: the password is asked for, but only now,
		// so that a file that is not encrypted never opens the dialog.
		pw = DgdPasswordFromArg( CTool::GetPassword() );
		if( pw.empty() )
			return false;
		if( !DgdOpen( fname, pw.c_str(), &f ) )
			return false;
	}
	if( m_Endflg ) { DgdClose( &f ); return true; }

	// The name of the whole file is in the INFO chunk of the parts.
	if( !f.name[0] ) { DgdClose( &f ); return false; }
	lstrcpyn(outname,f.name,(int)sizeof(outname));
	if( !CheckStr(outname) ) { DgdClose( &f ); return true; }

	// the 8.3 fallback: the name can be too long for a file system
	// without long names
	kiutil::pathMake83(outname);

	name = kiutil::pathMake( outname );
	if( !name ) { DgdClose( &f ); return false; }

	out = fopen( name, "wb" );
	if( !out ) { DgdClose( &f ); return false; }
	if( f.datalen ) {
		if( f.datalen!=fwrite( f.data, 1, f.datalen, out ) ) {
			fclose(out);
			DeleteFile( name );
			DgdClose( &f );
			return false;
		}
	}
	fclose(out);

	// the time stamp of the file
	if( f.mtime ) {
		FILETIME ft;
		LARGE_INTEGER li;
		li.QuadPart = (LONGLONG)f.mtime;
		ft.dwLowDateTime = li.LowPart;
		ft.dwHighDateTime = li.HighPart;
		kiutil::timeSet( name, &ft );
	}

	DgdClose( &f );
	return true;
}
