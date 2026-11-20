// InstallShield 3 container reader.  See Is3Decode.h for the format.

#include "stdafx.h"
#include "Is3Decode.h"
#include "kiutil.h"
extern "C" {
#include "pkdcl/pkdcl.h"
}
#include <stddef.h>

// ---------- little endian helpers ----------
static WORD le16( const unsigned char* p )
{
	return (WORD)(p[0] | ((WORD)p[1]<<8));
}
static DWORD le32( const unsigned char* p )
{
	return (DWORD)p[0] | ((DWORD)p[1]<<8) | ((DWORD)p[2]<<16) | ((DWORD)p[3]<<24);
}

// ---------- part management ----------
// part 0 is the archive that was passed in; part N>0 is <base>.N
static bool is3_part( Is3Archive* arc, int id )
{
	char path[MAX_PATH*2];
	if( arc->fp )
	{
		fclose( arc->fp );
		arc->fp = NULL;
	}
	if( id == 0 )
		lstrcpyn( path, arc->self_path, (int)sizeof(path) );
	else
		wsprintf( path, "%s.%d", arc->base, id );
	arc->fp = fopen( path, "rb" );
	return arc->fp != NULL;
}

// ---------- header / list parsing ----------
static bool is3_read_list( Is3Archive* arc )
{
	unsigned char hdr[58];
	unsigned char rec[64];
	char name[MAX_PATH];
	Is3Dir* dirs = NULL;
	DWORD dirofs, filofs;
	int nd, nf;
	int i;
	bool ok = false;

	if( !arc->fp ) return false;
	if( fseek( arc->fp, 5, SEEK_SET ) ) return false;
	if( fread( hdr, 1, 58, arc->fp ) != 58 ) return false;

	// header layout verified against ICOMP 3.00.062 archives:
	// haspw@4, flags@5, nfiles@7, taken@21, tparts@25, partid@26,
	// endinteg@28, startinteg@32, dirofs@36, dirsize@40,
	// ndirs@44, filofs@46.
	if( hdr[4] & 0x01 ) arc->encrypted = true;
	arc->flags  = le16( hdr+5 );
	arc->part_id = hdr[26];
	arc->total_parts = hdr[25];
	arc->end_integral_data   = le32( hdr+28 );
	arc->start_integral_data = le32( hdr+32 );
	arc->dirs_list_offset = le32( hdr+36 );
	arc->dirs_list_size   = le32( hdr+40 );
	nd = (int)le16( hdr+44 );
	nf = (int)le16( hdr+7 );
	dirofs = le32( hdr+36 );
	filofs = le32( hdr+46 );
	if( nd < 0 || nd > 4096 || nf < 0 || nf > IS3_MAX_FILES )
		return false;

	if( nd > 0 )
	{
		dirs = (Is3Dir*)calloc( (size_t)nd, sizeof(Is3Dir) );
		if( !dirs ) return false;
	}
	if( nd > 0 && fseek( arc->fp, (long)dirofs, SEEK_SET ) )
		goto done;
	for( i=0; i<nd; i++ )
	{
		WORD ent, ns;
		if( fread( rec, 1, 6, arc->fp ) != 6 ) goto done;
		ent = le16( rec+2 );
		ns  = le16( rec+4 );
		if( ns >= MAX_PATH || ent < 11U || ent > 11U+ns ) goto done;
		if( fread( dirs[i].name, 1, ns+1, arc->fp ) != ns+1 ) goto done;
		dirs[i].name[ns] = 0;
		if( fread( rec, 1, 4, arc->fp ) != 4 ) goto done;
	}
	if( nf > 0 )
	{
		arc->files = (Is3File*)calloc( (size_t)nf, sizeof(Is3File) );
		if( !arc->files ) goto done;
		arc->nfile = nf;
	}
	if( nf > 0 && fseek( arc->fp, (long)filofs, SEEK_SET ) )
		goto done;
	for( i=0; i<nf; i++ )
	{
		Is3File* f = &arc->files[i];
		WORD ent, ns;
		int di;
		// 30-byte fixed record, verified field offsets:
		// endpart@0 dirid@1 osz@3 csz@7 ofs@11 fdate@15 attr@19
		// entsz@23 flags@25 pad@27 startpart@28 ns@29
		if( fread( rec, 1, 30, arc->fp ) != 30 ) goto done;
		f->end_part   = rec[0];
		di            = (int)le16( rec+1 );
		f->orig       = le32( rec+3 );
		f->comp       = le32( rec+7 );
		f->ofs        = le32( rec+11 );
		{
			DWORD fd = le32( rec+15 );
			f->date = (WORD)(fd & 0xFFFFUL);
			f->time = (WORD)(fd >> 16);
		}
		f->attrib     = (BYTE)le32( rec+19 );
		ent           = le16( rec+23 );
		f->flags      = le16( rec+25 );
		f->start_part = rec[28];
		ns            = rec[29];
		if( ns >= MAX_PATH || ent < 43U || ent > 43U+ns ) goto done;
		if( fread( name, 1, ns+1, arc->fp ) != ns+1 ) goto done;
		name[ns] = 0;
		// 12-byte trailer: versionMS, versionLS, unused
		if( fread( rec, 1, 12, arc->fp ) != 12 ) goto done;

		// tolerate a missing directory id (some writers use 0xffff)
		if( di>=0 && di<nd && dirs[di].name[0] )
			wsprintf( f->path, "%s\\%s", dirs[di].name, name );
		else
			lstrcpyn( f->path, name, (int)sizeof(f->path) );
		for( di=0; f->path[di]; di++ )
			if( f->path[di]=='/' ) f->path[di]='\\';
	}
	ok = true;
done:
	if( dirs ) free( dirs );
	if( !ok && arc->files ) { free( arc->files ); arc->files = NULL; arc->nfile = 0; }
	return ok;
}

bool Is3Open( const char* fname, Is3Archive* arc )
{
	unsigned char magic[5];
	size_t n;

	memset( arc, 0, sizeof(*arc) );
	lstrcpyn( arc->self_path, fname, (int)sizeof(arc->self_path) );
	lstrcpyn( arc->base, fname, (int)sizeof(arc->base) );
	n = strlen( arc->base );
	while( n>0 && arc->base[n-1]!='.' && arc->base[n-1]!='\\' && arc->base[n-1]!='/' )
		n--;
	if( n>0 && arc->base[n-1]=='.' ) arc->base[n-1] = 0;

	if( !is3_part( arc, 0 ) ) return false;
	if( fread( magic, 1, 5, arc->fp ) != 5 ) return false;
	if( magic[4] != 0x3A || le32(magic) != 0x8C655D13UL )
		return false;
	return is3_read_list( arc );
}

void Is3Close( Is3Archive* arc )
{
	if( !arc ) return;
	if( arc->fp ) { fclose( arc->fp ); arc->fp = NULL; }
	if( arc->files ) { free( arc->files ); arc->files = NULL; }
	arc->nfile = 0;
}

// ---------- member decode ----------
struct Is3DecIO
{
	const unsigned char* in;
	DWORD inlen, inpos;
	FILE* out;
	DWORD wrote;
	DWORD orig;
	bool over;
};

static unsigned short is3_dcl_read( unsigned char* buf, unsigned short* size, void* opaque )
{
	Is3DecIO* io = (Is3DecIO*)opaque;
	DWORD want = *size;
	if( want > io->inlen - io->inpos ) want = io->inlen - io->inpos;
	if( !want ) { *size = 0; return 0; }
	memcpy( buf, io->in + io->inpos, want );
	io->inpos += want;
	*size = (unsigned short)want;
	return *size;
}

static void is3_dcl_write( unsigned char* buf, unsigned short* size, void* opaque )
{
	Is3DecIO* io = (Is3DecIO*)opaque;
	if( io->wrote + *size > io->orig )
	{
		io->over = true;
		*size = 0;
		return;
	}
	fwrite( buf, 1, *size, io->out );
	io->wrote += *size;
}

bool Is3Extract( Is3Archive* arc, int index, const char* outpath )
{
	Is3File* f;
	unsigned char* packed;
	DWORD total = 0, n;
	FILE* out;
	bool ok = false;
	Is3DecIO io;

	if( !arc || index<0 || index>=arc->nfile ) return false;
	f = &arc->files[index];
	if( f->comp > IS3_MAX_COMP || f->orig > IS3_MAX_MEMBER )
		return false;
	// A stored empty member has comp==orig==0; create an empty file.
	if( f->comp == 0 )
	{
		if( (f->flags & 0x10) && f->orig == 0 )
		{
			FILE* e = fopen( outpath, "wb" );
			if( !e ) return false;
			fclose( e );
			return true;
		}
		return false;
	}

	packed = (unsigned char*)malloc( (size_t)f->comp );
	if( !packed ) return false;

	// A single-part member is contiguous in the archive.  Split members
	// concatenate the slices from each part; part 1+ is named <base>.N.
	if( !(f->flags & 0x100) || !(arc->flags & 0x3) || f->start_part==f->end_part )
	{
		if( !arc->fp ) goto done;
		if( fseek( arc->fp, (long)f->ofs, SEEK_SET ) ) goto done;
		if( fread( packed, 1, (size_t)f->comp, arc->fp ) != f->comp ) goto done;
		total = f->comp;
	}
	else
	{
		int p;
		for( p=f->start_part; p<=f->end_part; p++ )
		{
			FILE* part;
			char pp[MAX_PATH*2];
			DWORD start, len;
			if( p==arc->part_id && p==0 ) part = arc->fp;
			else if( p==0 ) part = arc->fp;
			else
			{
				wsprintf( pp, "%s.%d", arc->base, p );
				part = fopen( pp, "rb" );
			}
			if( !part ) goto done;
			start = (p==f->start_part) ? f->ofs : 255;
			if( p==f->end_part )
				len = arc->start_integral_data - start;
			else
				len = arc->dirs_list_offset - start;
			if( len > f->comp - total ) len = f->comp - total;
			if( fseek( part, (long)start, SEEK_SET ) || fread( packed+total, 1, len, part )!=len )
			{
				if( p!=0 ) fclose( part );
				goto done;
			}
			total += len;
			if( p!=0 ) fclose( part );
		}
		if( total != f->comp ) goto done;
	}

	out = fopen( outpath, "wb" );
	if( !out ) goto done;
	if( f->flags & 0x10 )
	{
		// stored: the compacted bytes are the original data
		n = (f->orig < f->comp) ? f->orig : f->comp;
		fwrite( packed, 1, n, out );
		ok = (n == f->orig);
	}
	else
	{
		memset( &io, 0, sizeof(io) );
		io.in = packed; io.inlen = f->comp; io.out = out;
		io.orig = f->orig;
		pkdcl_explode_ex( is3_dcl_read, is3_dcl_write, &io );
		ok = (!io.over && io.wrote == f->orig);
	}
	fclose( out );
	if( !ok ) DeleteFile( outpath );
done:
	free( packed );
	return ok;
}
