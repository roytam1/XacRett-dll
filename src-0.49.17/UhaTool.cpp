// The UHARC provider. See uharc/UhaDecode.h for the format.

#include "stdafx.h"
#include "UhaTool.h"
#include "uharc/UhaDecode.h"
#include "kiutil.h"
#include "XacRett.h"

bool CUhaTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz < 39 )
		return false;
	if( hdr[0]!=0x55 || hdr[1]!=0x48 || hdr[2]!=0x41 )
		return false;
	if( hdr[3]!=2 && hdr[3]!=3 && hdr[3]!=4 && hdr[3]!=6 )
		return false;
	return true;
}

bool CUhaTool::Extract( const char* fname,const char* ddir )
{
	UhaArchive arc;
	int i;
//--------------------------------------------------------------------//

	// Try without a password first, so a plain archive never opens the
	// dialog; ask only when the archive says it is encrypted.
	{
		const char* pw = CTool::GetPasswordArg();
		int rc = UhaOpen( fname, pw, &arc );
		if( rc != 0 )
		{
			if( rc == 8 )
			{
				// password required: one dialog attempt (the tool's
				// own GetPassword honours -p, so headless runs never
				// hang when a password was given)
				std::string s = GetPassword();
				if( s.empty() || 0 != UhaOpen( fname, s.c_str(), &arc ) )
					return false;
			}
			else
				return false;
		}
	}

	for( i=0; i<(int)arc.nfiles; i++ )
	{
		UhaEntry* e = &arc.entries[i];
		char outname[UHA_MAX_NAME];
		char* name;

		if( m_Endflg ) break;

		// 0.6 skip markers (method 0xFF) and empty files come from the
		// splitter, not from here; the engine archive holds the true
		// directory, including empty files.
		lstrcpyn( outname, e->path, (int)sizeof(outname) );

		if( e->size == 0 )
		{
			if( !CheckStr(outname) )
				continue;
			name = kiutil::pathMake( outname );
			if( name )
			{
				FILE* f = fopen( name, "wb" );
				if( f ) fclose( f );
			}
			continue;
		}

		if( !CheckStr(outname) )
			continue;

		// pathMake runs before the overwrite test, so -j flattens
		// first (the name the callback tests is the name written).
		// kiutil::pathMake rewrites in place; each member needs its
		// own copy because the engine owns e->path.
		{
			char tmp[UHA_MAX_NAME];
			lstrcpyn( tmp, outname, (int)sizeof(tmp) );
			name = kiutil::pathMake( tmp );
			if( !name )
				continue;
			// UhaExtractAll writes the whole solid stream; per-member
			// extraction is added once the solid split is final.
			// Phase 0 exercises the plumbing through the empty-file
			// path above only.
			(void)name;
		}
	}

	UhaClose( &arc );
	return true;
}
