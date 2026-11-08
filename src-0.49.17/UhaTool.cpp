// The UHARC provider. See uharc/UhaDecode.h for the format.

#include "stdafx.h"
#include "UhaTool.h"
#include "uharc/UhaDecode.h"
#include "uharc/UhaCrypt.h"
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

	// The solid stream lives in the archive image; decrypt it when
	// the archive is -pw protected (fresh S-box over the whole packed
	// buffer, FORMAT 6.6), decode once, then split at the cumulative
	// file ends. Cores 0/2/3 are ported; 1/4/5 refuse loud for now.
	{
		unsigned char* packed = 0;
		unsigned char* decoded = 0;
		unsigned long dlen = 0;
		int drc;
		if( !arc.img || arc.packed_len == 0 )
		{
			UhaClose( &arc );
			return false;
		}
		packed = (unsigned char*)malloc(arc.packed_len);
		if( !packed )
		{
			UhaClose( &arc );
			return false;
		}
		memcpy(packed, arc.img + arc.hdr_size, arc.packed_len);
		if( arc.pw_stream )
		{
			const char* pw = CTool::GetPasswordArg();
			if( !pw || !*pw )
			{
				// The open path already asked; nothing more to try.
				free(packed);
				UhaClose( &arc );
				return false;
			}
			{
				UhaSBox sb;
				UhaSBoxInit(&sb, pw, arc.ver == 2 ? 1 : 0);
				UhaSBoxUpdate(&sb, packed, arc.packed_len);
			}
		}
		drc = UhaDecodeSolid( &arc, packed, &decoded, &dlen );
		free(packed);
		if( drc != 0 || !decoded )
		{
			if( decoded ) free(decoded);
			UhaClose( &arc );
			return false;
		}

		for( i=0; i<(int)arc.nfiles; i++ )
		{
			UhaEntry* e = &arc.entries[i];
			char outname[UHA_MAX_NAME];
			char* name;
			FILE* out;

			if( m_Endflg ) break;

			// 0.6 skip markers (method 0xFF, size 0) have no data of
			// their own; the splitter view already removed them,
			// so they just materialise empty below.
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
				if( arcproc )
				{
					static EXTRACTINGINFO	info;
					::memset(&info,0,sizeof(EXTRACTINGINFO));
					lstrcpy(info.szSourceFileName,name);
					if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
						continue;
				}
				out = fopen( name, "wb" );
				if( !out )
				{
					free(decoded);
					UhaClose( &arc );
					return false;
				}
				{
					unsigned char* filedata = decoded + e->offset;
					// The EXE post-filter runs over the whole file
					// before the CRC check when the archive gates
					// it on and the file is big enough.
					if( arc.exe_filter && e->size >= 0x400 )
						UhaExeFilter( &arc, filedata, e->size );
					if( fwrite( filedata, 1, e->size, out ) != e->size )
					{
						fclose( out );
						DeleteFile( name );
						free(decoded);
						UhaClose( &arc );
						return false;
					}
					fclose( out );
					if( UhaFileCrc( filedata, e->size ) != e->crc )
					{
						DeleteFile( name );
						free(decoded);
						UhaClose( &arc );
						return false;
					}
				}
				kiutil::timeSet( name, (WORD)e->date, (WORD)e->time );
			}
		}
		free(decoded);
	}
	UhaClose( &arc );
	return true;
}
