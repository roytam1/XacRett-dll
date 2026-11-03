// A reader for the BSA and BSN archives.  See BsaDecode.h for the
// format; BsaCodec.cpp holds the decompressors.

#include "stdafx.h"
#include "BsaDecode.h"
#include "BsaCodec.h"
#include "zlib/zlib.h"

// Whole files larger than this are refused rather than read into memory.
#define BSA_MAX_IMAGE (512UL*1024*1024)

// How much of the archive after a packed member is handed to the bit
// readers as "trail" (they over-read past the packed end, like the
// original does).
#define BSA_TRAIL_MAX 65536UL

//==========================================================================//
// little helpers

static unsigned long bsa_be16( const unsigned char* p )
{
	return ((unsigned long)p[0] << 8) | p[1];
}

static unsigned long bsa_be32( const unsigned char* p )
{
	return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
		   ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

static unsigned long bsa_le16( const unsigned char* p )
{
	return ((unsigned long)p[0]) | ((unsigned long)p[1] << 8);
}

static unsigned long bsa_le32( const unsigned char* p )
{
	return ((unsigned long)p[0]) | ((unsigned long)p[1] << 8) |
		   ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

// The byte-sum checksum of a BSA entry, modulo 2^16.
static unsigned long bsa_bytesum( const unsigned char* buf, unsigned long len )
{
	unsigned long s = 0;
	unsigned long i;
	for( i=0; i<len; i++ )
		s += buf[i];
	return s & 0xFFFFUL;
}

// Normalize a member name to a relative path with backslashes: absolute
// paths, drive letters and every ".." component are dropped (a hostile
// member cannot escape the output directory), empty and "." components
// are skipped.  Returns false when nothing is left.
static bool BsaCleanName( char* dst, int dstsz, const char* src )
{
	char clean[BSA_MAX_NAME];
	int o = 0, s = 0, i;

	for( s=0; src[s] && o < (int)sizeof(clean)-1; s++ )
	{
		if( src[s]=='/' || src[s]=='\\' )
		{
			if( o==0 || clean[o-1]=='\\' ) continue;	// collapse, drop leading
			clean[o++] = '\\';
			continue;
		}
		clean[o++] = src[s];
	}
	while( o>0 && clean[o-1]=='\\' ) o--;
	clean[o] = 0;
	if( o==0 )
		return false;

	// strip a leading "C:" style drive specifier
	if( o>1 && clean[1]==':' )
	{
		memmove( clean, clean + 2, (size_t)o - 1 );
		o -= 2;
		while( o>0 && clean[0]=='\\' )
		{
			memmove( clean, clean + 1, (size_t)o );
			o--;
		}
	}

	// drop "." and ".." components
	o = 0;
	{
		int s2 = 0, n = (int)strlen(clean);
		char tmp[BSA_MAX_NAME];
		memcpy( tmp, clean, (size_t)n + 1 );
		while( s2 < n )
		{
			int e = s2;
			while( e < n && tmp[e] != '\\' ) e++;
			if( !(e - s2 == 1 && tmp[s2] == '.') &&
				!(e - s2 == 2 && tmp[s2] == '.' && tmp[s2+1] == '.') &&
				e > s2 )
			{
				if( o>0 ) clean[o++] = '\\';
				for( i=s2; i<e && o < (int)sizeof(clean)-1; i++ )
					clean[o++] = tmp[i];
			}
			s2 = (e < n) ? e + 1 : e;
		}
	}
	clean[o] = 0;
	if( o==0 )
		return false;

	if( o >= dstsz )
		return false;
	memcpy( dst, clean, (size_t)o + 1 );
	return true;
}

const char* BsaMethodName( int method )
{
	switch( method )
	{
	case BSA_METHOD_STORED:	return "stored";
	case BSA_METHOD_M2:		return "m2";
	case BSA_METHOD_M3:		return "m3";
	case BSA_METHOD_M5:		return "m5";
	case BSA_METHOD_M6:		return "m6";
	case BSA_METHOD_M7:		return "m7";
	case BSA_METHOD_M9:		return "m9";
	}
	return "m10?";
}

//==========================================================================//
// the directory

struct BsaBuilder
{
	BsaEntry*		e;
	int				n;
	int				cap;
};

static BsaEntry* bsa_new_entry( BsaBuilder* b )
{
	if( b->n == b->cap )
	{
		int nc = b->cap ? b->cap * 2 : 64;
		BsaEntry* ne = new BsaEntry[nc];
		if( b->n )
			memcpy( ne, b->e, sizeof(BsaEntry) * (size_t)b->n );
		if( b->e )
			delete[] b->e;
		b->e = ne;
		b->cap = nc;
	}
	memset( &b->e[b->n], 0, sizeof(BsaEntry) );
	return &b->e[b->n++];
}

// BSN: the global header and the entries.
static bool BsaParseBsn( BsaArchive* arc )
{
	unsigned char* img = arc->img;
	unsigned long size = arc->imgsize;
	unsigned long pos;
	unsigned long gflags;
	BsaBuilder b;

	b.e = 0; b.n = 0; b.cap = 0;

	if( size < 6 )
		return false;
	if( img[0]!=0xFF || img[1]!='B' || img[2]!='S' || img[3]!='G' )
		return false;
	gflags = bsa_be16( img + 4 );
	pos = 6;
	if( gflags & 0x0100 )
	{
		// a NUL terminated archive comment
		while( pos < size && img[pos] != 0 )
			pos++;
		if( pos >= size )
			goto fail;
		pos++;
	}

	for(;;)
	{
		unsigned long L, csum_stored, csum_calc;
		unsigned long eflags, ts, orig, packed, filecrc;
		unsigned long p, end;
		unsigned long dataoff;
		char raw[BSA_MAX_NAME];
		BsaEntry* e;
		int fi;

		if( pos >= size )
			break;
		if( size - pos < 4 )
			break;
		if( img[pos]!=0xFF || img[pos+1]!='B' || img[pos+2]!='S' || img[pos+3]!='A' )
			break;			// trailing data: stop cleanly
		pos += 4;
		if( size - pos < 2 )
			goto fail;
		L = bsa_be16( img + pos );
		pos += 2;
		if( L==0 || L>65535 )
			goto fail;
		if( size - pos < L + 4 )
			goto fail;
		csum_stored = bsa_be32( img + pos + L );
		csum_calc = (unsigned long)crc32( 0, img + pos, (uInt)L );
		if( csum_stored != csum_calc && csum_stored != (~csum_calc & 0xFFFFFFFFUL) )
			goto fail;		// a damaged metadata block

		// parse the metadata block
		p = pos;
		end = pos + L;
		eflags = bsa_be16( img + p ); p += 2;
		p += 2;					// method/format byte, not validated
		ts = bsa_be32( img + p ); p += 4;
		fi = 0;
		while( p < end && img[p] != 0 && fi < (int)sizeof(raw)-1 )
			raw[fi++] = (char)img[p++];
		raw[fi] = 0;
		if( p < end && img[p]==0 ) p++;
		if( eflags & 0x0008 )
		{
			if( p + 4 > end ) goto fail;
			p += 4;				// the "extra" size
		}
		if( p + 4 > end ) goto fail;
		orig = bsa_be32( img + p ); p += 4;
		if( p + 4 > end ) goto fail;
		packed = bsa_be32( img + p ); p += 4;
		if( p + 4 > end ) goto fail;
		filecrc = bsa_be32( img + p ); p += 4;
		if( eflags & 0x0020 )
		{
			unsigned long clen;
			if( p + 2 > end ) goto fail;
			clen = bsa_be16( img + p ); p += 2;
			if( p + clen > end ) goto fail;
			p += clen;
		}

		pos = end + 4;
		dataoff = pos;
		if( packed > size || dataoff > size - packed )
			goto fail;
		pos = dataoff + packed;

		e = bsa_new_entry( &b );
		e->orig = orig;
		e->packed = packed;
		e->offset = dataoff;
		e->crc = filecrc;
		e->ts = ts;
		e->isdir = (packed==0 && orig==0 && (eflags & 0x0A00)==0x0A00);
		e->flag = (unsigned int)eflags;
		e->textmode = 0;
		if( e->isdir )
			e->method = BSA_METHOD_STORED;
		else if( packed == orig )
			e->method = BSA_METHOD_STORED;
		else if( eflags & 0x8000 )
			e->method = BSA_METHOD_UNKNOWN;
		else if( eflags & 0x4000 )
		{
			e->method = BSA_METHOD_M9;
			e->textmode = (eflags & 1);
		}
		else
			e->method = BSA_METHOD_M7;

		if( !BsaCleanName( e->name, (int)sizeof(e->name), raw ) )
		{
			// an unusable name: the entry is dropped (the member count
			// drops with it, so the data pointer is still right)
			b.n--;
			continue;
		}
	}

	arc->entries = b.e;
	arc->nentry = b.n;
	arc->isbsn = 1;
	return true;

fail:
	if( b.e ) delete[] b.e;
	return false;
}

// BSA: no global header, one entry after the other.
static bool BsaParseBsa( BsaArchive* arc )
{
	unsigned char* img = arc->img;
	unsigned long size = arc->imgsize;
	unsigned long pos = 0;
	int parsed = 0;
	BsaBuilder b;

	b.e = 0; b.n = 0; b.cap = 0;

	for(;;)
	{
		unsigned long b0, packed, orig, ts, w1;
		unsigned long nl, dataoff;
		unsigned long flag;
		char raw[BSA_MAX_NAME];
		BsaEntry* e;
		int i;

		if( pos >= size )
			break;
		b0 = img[pos];
		if( pos + 1 >= size )
		{
			// a lone trailing 00 ends the older archives
			if( b0 == 0 )
				break;
			goto fail;
		}
		if( b0 == 0 && img[pos+1] == 0 )
			break;				// the terminator
		pos += 2;
		if( pos >= size )
			goto fail;
		if( img[pos] != 0xAE )
		{
			// Some archives carry trailing data after the last entry; stop
			// like the tar reader does, but only once at least one entry has
			// parsed (a broken FIRST entry is fatal).
			if( parsed )
				break;
			goto fail;			// a bad entry magic
		}
		pos++;
		if( pos >= size )
			goto fail;
		flag = img[pos];
		pos++;
		if( size - pos < 12 + 6 + 1 )
			goto fail;
		packed = bsa_le32( img + pos ); pos += 4;
		orig = bsa_le32( img + pos ); pos += 4;
		ts = bsa_le32( img + pos ); pos += 4;
		w1 = bsa_le16( img + pos ); pos += 2;	// the byte-sum checksum
		pos += 2;								// w2
		pos += 2;								// w3
		nl = img[pos];
		pos++;
		if( size - pos < nl + 1 )
			goto fail;
		{
			unsigned long cpy = nl >= sizeof(raw)-1 ? sizeof(raw)-1 : nl;
			for( i=0; i<(int)cpy; i++ )
				raw[i] = (char)img[pos + (unsigned long)i];
			raw[cpy] = 0;
		}
		pos += nl;				// the name bytes
		pos += 1;				// the NUL terminator

		dataoff = pos;
		if( packed > size || dataoff > size - packed )
			goto fail;
		pos = dataoff + packed;

		parsed++;
		e = bsa_new_entry( &b );
		e->orig = orig;
		e->packed = packed;
		e->offset = dataoff;
		e->crc = w1;
		e->ts = ts;
		e->isdir = 0;
		e->flag = (unsigned int)flag;
		e->textmode = 0;
		if( flag == 0 || packed == orig )
			e->method = BSA_METHOD_STORED;
		else if( flag==2 || flag==3 || flag==5 || flag==6 )
			e->method = (int)flag;
		else if( flag == 7 )
			e->method = BSA_METHOD_M7;
		else
			e->method = BSA_METHOD_UNKNOWN;

		if( !BsaCleanName( e->name, (int)sizeof(e->name), raw ) )
		{
			b.n--;
			continue;
		}
	}

	arc->entries = b.e;
	arc->nentry = b.n;
	arc->isbsn = 0;
	return true;

fail:
	if( b.e ) delete[] b.e;
	return false;
}

//==========================================================================//

bool BsaOpen( const char* path, BsaArchive* arc )
{
	FILE* fp;
	unsigned long size;
	unsigned char* img;

	memset( arc, 0, sizeof(*arc) );

	fp = fopen( path, "rb" );
	if( !fp )
		return false;
	if( fseek( fp, 0, SEEK_END ) != 0 )
	{
		fclose(fp);
		return false;
	}
	size = (unsigned long)ftell( fp );
	if( ftell(fp) < 0 || size > BSA_MAX_IMAGE )
	{
		fclose(fp);
		return false;
	}
	rewind( fp );
	img = (unsigned char*)malloc( size ? size : 1 );
	if( !img )
	{
		fclose(fp);
		return false;
	}
	if( size && fread( img, 1, size, fp ) != size )
	{
		free( img );
		fclose(fp);
		return false;
	}
	fclose( fp );

	arc->img = img;
	arc->imgsize = size;

	// Both windows of the LZ decoders are persistent across the entries
	// of one archive, so a fresh archive must start from a clean window.
	BsaCodecReset();

	if( size >= 4 && img[0]==0xFF && img[1]=='B' && img[2]=='S' && img[3]=='G' )
	{
		if( BsaParseBsn( arc ) )
			return true;
	}
	else if( BsaParseBsa( arc ) )
	{
		return true;
	}

	BsaClose( arc );
	return false;
}

void BsaClose( BsaArchive* arc )
{
	if( arc->entries )
		delete[] arc->entries;
	if( arc->img )
		free( arc->img );
	memset( arc, 0, sizeof(*arc) );
}

// Decode one compressed member.  *used_ppm is set when a "method 7" BSA
// member turned out to be a PPM stream.
static unsigned char* BsaDecodeMember( BsaArchive* arc, BsaEntry* e,
									   unsigned long* produced, int* used_ppm )
{
	const unsigned char* packed = arc->img + e->offset;
	unsigned long trailmax;
	const unsigned char* trail;
	unsigned long tsize;
	unsigned char* out;

	*produced = 0;
	*used_ppm = 0;

	trailmax = arc->imgsize - (e->offset + e->packed);
	tsize = trailmax > BSA_TRAIL_MAX ? BSA_TRAIL_MAX : trailmax;
	trail = packed + e->packed;

	switch( e->method )
	{
	case BSA_METHOD_M2:
	case BSA_METHOD_M3:
	case BSA_METHOD_M5:
	case BSA_METHOD_M6:
		return BsaDecodeMethod23( packed, e->packed, trail, tsize,
								  e->orig, produced, e->method );

	case BSA_METHOD_M7:
		out = BsaDecodeMethod7( packed, e->packed, e->orig, produced );
		if( out && *produced == e->orig )
			return out;
		if( out )
		{
			free( out );
			*produced = 0;
		}
		// the BSA writer stores 7 for every compressed flavor, so a PPM
		// stream arrives marked 7; the original cannot read those at all
		out = BsaDecodeMethod9( packed, e->packed, trail, tsize,
								e->orig, produced, 0 );
		if( out && *produced == e->orig )
		{
			*used_ppm = 1;
			return out;
		}
		if( out )
		{
			free( out );
			*produced = 0;
		}
		return 0;

	case BSA_METHOD_M9:
		return BsaDecodeMethod9( packed, e->packed, trail, tsize,
								 e->orig, produced, e->textmode );
	}
	return 0;
}

bool BsaExtractFile( BsaArchive* arc, int index, const char* outpath )
{
	BsaEntry* e;
	FILE* out;
	bool ok = true;

	if( index < 0 || index >= arc->nentry )
		return false;
	e = &arc->entries[index];

	if( e->isdir )
	{
		// the directories are made by pathMake at the call site
		return true;
	}

	if( e->orig > BSA_MAX_MEMBER )
		return false;

	out = fopen( outpath, "wb" );
	if( !out )
		return false;

	if( e->orig == 0 )
	{
		fclose( out );
		return true;
	}

	if( e->method == BSA_METHOD_STORED )
	{
		const unsigned char* src = arc->img + e->offset;
		if( e->packed != e->orig || fwrite( src, 1, e->orig, out ) != e->orig )
			ok = false;
		else if( arc->isbsn )
		{
			// the CRC of the stored member is in the block
			if( e->crc && (unsigned long)crc32( 0, src, (uInt)e->orig ) != e->crc )
				ok = false;
		}
		else if( arc->isbsn == 0 )
		{
			// the BSA entry carries a byte sum, not a CRC; the old tools
			// verify it, and a member that does not match is corrupt
			if( e->crc && bsa_bytesum( src, e->orig ) != e->crc )
				ok = false;
		}
	}
	else if( e->method == BSA_METHOD_UNKNOWN )
	{
		ok = false;
	}
	else
	{
		unsigned long produced = 0;
		int used_ppm = 0;
		unsigned char* buf = BsaDecodeMember( arc, e, &produced, &used_ppm );

		if( !buf || produced != e->orig )
		{
			if( buf ) free( buf );
			ok = false;
		}
		else
		{
			if( arc->isbsn )
			{
				if( e->crc && (unsigned long)crc32( 0, buf, (uInt)produced ) != e->crc )
					ok = false;
			}
			else
			{
				if( e->crc && bsa_bytesum( buf, produced ) != e->crc )
					ok = false;
			}
			if( ok && fwrite( buf, 1, produced, out ) != produced )
				ok = false;
			free( buf );
		}
	}

	if( fclose( out ) != 0 )
		ok = false;
	return ok;
}

// The members of one archive share the LZ windows of the coders, so a
// member whose output is not wanted still has to be run through its
// decoder before the members that follow it: a stream may refer back
// into the bytes an earlier member produced (PTSDOS.BSN is such an
// archive).  Nothing is written; only the window state matters.
bool BsaDecodeDiscard( BsaArchive* arc, int index )
{
	BsaEntry* e;
	unsigned long produced = 0;
	int used_ppm = 0;
	unsigned char* buf;

	if( index < 0 || index >= arc->nentry )
		return false;
	e = &arc->entries[index];

	if( e->isdir || e->method == BSA_METHOD_STORED || e->orig == 0 )
		return true;
	if( e->method == BSA_METHOD_UNKNOWN || e->orig > BSA_MAX_MEMBER )
		return false;

	buf = BsaDecodeMember( arc, e, &produced, &used_ppm );
	if( !buf )
		return false;
	free( buf );
	return produced == e->orig;
}
