// A reader for the Quantum archives of Cinematronics / Dynamix.
// See QuantumDecode.h for the format.
//
// The compressed data is an LZ77 with an adaptive binary arithmetic coder,
// the carry less ("Subbotin") variant with the 16 bit low/high/code
// registers.  The arithmetic is done in 32 bit unsigned arithmetic, and the
// cumulative counts of the models are 16 bit: the models rely on the
// truncation of the counts, so the widths must not be changed.

#include "stdafx.h"
#include "QuantumDecode.h"

#include <stdlib.h>
#include <string.h>

// The number of the padding bytes a valid archive may need at its end.
#define QUANTUM_MAXPAD	64

//==========================================================================//
// The tables
//
// The length and position tables are the same as in the Cabinet Quantum
// method and live in QuantumCore.cpp.  This format declares two more
// distance slots than the Cabinet one, both with zero extra bits, so the
// base of those two is worked out here.

static unsigned quantum_pos_base( int n )
{
	if( n < 42 ) return QtmPositionBase[n];
	return QtmPositionBase[41] + (unsigned)(n - 41);
}

static int quantum_pos_bits( int n )
{
	return (n < 42) ? (int)QtmPositionExtra[n] : 0;
}

// The number of the distance classes that are valid below a limit.
static int quantum_nclasses( unsigned limit )
{
	int n = 0;

	while( n < 43 && quantum_pos_base(n) < limit ) n++;
	return n;
}

//==========================================================================//
// The bit reader

static int quantum_bit( QuantumBitIn* b )
{
	int bit;

	if( b->left == 0 )
	{
		b->idx++;
		if( b->idx >= b->len )
		{
			b->len = (int)fread( b->buf, 1, sizeof(b->buf), b->fp );
			b->idx = 0;
			if( b->len <= 0 )
			{
				b->len = 0;
				b->idx = 0;
				b->left = 8;
				b->overrun++;		// one padding byte
				return 0;
			}
		}
		b->left = 8;
	}
	bit = (b->buf[b->idx] >> 7) & 1;
	b->buf[b->idx] = (unsigned char)(b->buf[b->idx] << 1);
	b->left--;
	b->nbits++;
	return bit;
}

static unsigned quantum_bits( QuantumBitIn* b, int n )
{
	unsigned v = 0;
	int i;

	for( i=0; i<n; i++ )
		v = (v << 1) | (unsigned)quantum_bit(b);
	return v;
}

static int quantum_byte( QuantumBitIn* b )
{
	return (int)quantum_bits( b, 8 );
}

//==========================================================================//
// The bit source of the shared arithmetic decoder

static int quantum_getbit( void* ctx )
{
	return quantum_bit( (QuantumBitIn*)ctx );
}

//==========================================================================//
// The CRC of a member
//
// It is not a CRC-32: the byte is XORed into the 16 bit register and the
// register is shifted left, the bit that falls out is fed back in.

static unsigned quantum_crc16( unsigned crc, unsigned char b )
{
	crc ^= b;
	crc = (crc & 0x8000) ? (((crc << 1) | 1) & 0xFFFF) : ((crc << 1) & 0xFFFF);
	return crc;
}

//==========================================================================//
// The window

static void quantum_win_put( QuantumDec* d, unsigned char b )
{
	d->win[ d->winpos & (d->winsize-1) ] = b;
	d->winpos++;
}

//==========================================================================//
// The container

// Normalize a stored name to a relative path with backslashes: absolute
// paths, drive letters and every ".." component are dropped, so that a
// hostile member cannot escape the output directory.  Returns false when
// nothing is left.
static bool quantum_clean_name( char* dst, int dstsz, const char* src )
{
	int o = 0;
	const char* p = src;

	if( !src || !*src ) return false;
	while( *p )
	{
		const char* q = p;
		int len;
		while( *q && *q != '\\' && *q != '/' && *q != ':' ) q++;
		len = (int)(q - p);
		if( len == 0 )
		{
			// an empty component
		}
		else if( len == 1 && p[0]=='.' )
		{
			// the current directory
		}
		else if( len == 2 && p[0]=='.' && p[1]=='.' )
		{
			// a parent directory: dropped
		}
		else
		{
			if( o > 0 )
			{
				if( o >= dstsz-1 ) return false;
				dst[o++] = '\\';
			}
			if( o + len > dstsz-1 ) len = dstsz-1-o;
			if( len <= 0 ) return false;
			memcpy( dst+o, p, len );
			o += len;
		}
		p = q;
		while( *p=='\\' || *p=='/' || *p==':' ) p++;
	}
	if( o <= 0 ) return false;
	dst[o] = '\0';
	return true;
}

// Read one byte of the file; -1 at the end of it.
static int quantum_fgetc( FILE* fp )
{
	int c = fgetc( fp );

	if( c == EOF ) return -1;
	return c & 0xFF;
}

// Read a length prefixed string.  Returns false at the end of the file.
static bool quantum_read_string( FILE* fp, char* dst, int dstsz )
{
	int n, i, c;

	n = quantum_fgetc( fp );
	if( n < 0 ) return false;
	if( n >= 0x80 )
	{
		int n2 = quantum_fgetc( fp );
		if( n2 < 0 ) return false;
		n = ((n & 0x7F) << 8) | n2;
	}
	if( n > dstsz-1 ) return false;
	for( i=0; i<n; i++ )
	{
		c = quantum_fgetc( fp );
		if( c < 0 ) return false;
		dst[i] = (char)c;
	}
	dst[n] = '\0';
	return true;
}

bool QuantumOpen( const char* fname, QuantumArchive* arc )
{
	FILE* fp;
	unsigned char hdr[8];
	unsigned long fsize;
	int i;
	int nfiles;
	int windowbits;
	int level;

	memset( arc, 0, sizeof(*arc) );

	fp = fopen( fname, "rb" );
	if( !fp ) return false;
	fseek( fp, 0, SEEK_END );
	fsize = (unsigned long)ftell( fp );
	fseek( fp, 0, SEEK_SET );

	if( fsize < 8 ) { fclose(fp); return false; }
	if( fread( hdr, 1, 8, fp ) != 8 ) { fclose(fp); return false; }
	if( hdr[0] != 'D' || hdr[1] != 'S' ) { fclose(fp); return false; }

	nfiles     = (int)hdr[4] | ((int)hdr[5] << 8);
	windowbits = (int)hdr[6];
	level      = (int)hdr[7] & 7;
	if( nfiles <= 0 || nfiles > QUANTUM_MAX_FILES ) { fclose(fp); return false; }
	if( windowbits < 10 || windowbits > 21 ) { fclose(fp); return false; }

	arc->files = (QuantumFile*)calloc( nfiles, sizeof(QuantumFile) );
	if( !arc->files ) { fclose(fp); return false; }

	for( i=0; i<nfiles; i++ )
	{
		unsigned long size, tm, dt;

		if( !quantum_read_string( fp, arc->files[i].name, QUANTUM_MAX_NAME ) )
			goto fail;
		if( !quantum_read_string( fp, arc->files[i].comment, QUANTUM_MAX_NAME ) )
			goto fail;
		if( fread( hdr, 1, 8, fp ) != 8 ) goto fail;
		size = (unsigned long)hdr[0] | ((unsigned long)hdr[1]<<8) |
			   ((unsigned long)hdr[2]<<16) | ((unsigned long)hdr[3]<<24);
		tm = (unsigned long)hdr[4] | ((unsigned long)hdr[5]<<8);
		dt = (unsigned long)hdr[6] | ((unsigned long)hdr[7]<<8);
		arc->files[i].size = (DWORD)size;
		arc->files[i].time = (WORD)tm;
		arc->files[i].date = (WORD)dt;
		// the stored name is turned into a relative path here, so that
		// the listing and the extraction cannot disagree about it
		{
			char raw[QUANTUM_MAX_NAME];
			lstrcpyn( raw, arc->files[i].name, (int)sizeof(raw) );
			if( !quantum_clean_name( arc->files[i].name, QUANTUM_MAX_NAME, raw ) )
			{
				arc->files[i].name[0] = '\0';
				arc->files[i].badname = 1;
			}
		}
	}

	arc->nfile		= nfiles;
	arc->level		= level;
	arc->windowbits	= windowbits;
	arc->window		= 1UL << windowbits;
	arc->dataoff	= (unsigned long)ftell( fp );
	arc->nextmember	= 0;

	// the decoder
	arc->dec = (QuantumDec*)malloc( sizeof(QuantumDec) );
	if( !arc->dec ) goto fail;
	memset( arc->dec, 0, sizeof(QuantumDec) );

	arc->dec->in.fp = fp;
	arc->dec->in.idx = -1;
	arc->dec->winsize = arc->window;
	arc->dec->winpos = 0;
	arc->dec->win = (unsigned char*)malloc( arc->window );
	if( !arc->dec->win ) goto fail;
	memset( arc->dec->win, 0, arc->window );

	QtmInitModel( &arc->dec->token, arc->dec->tokenSyms, 0, 7, 7 );
	for( i=0; i<4; i++ )
		QtmInitModel( &arc->dec->literal[i], arc->dec->litSyms[i], 64*i, 64, 64 );
	QtmInitModel( &arc->dec->ctx, arc->dec->ctxSyms, 0, 27, 27 );
	QtmInitModel( &arc->dec->dist_full, arc->dec->distSyms[0], 0, QTM_ENTRIES_MAX,
				  quantum_nclasses((unsigned)arc->window) );
	QtmInitModel( &arc->dec->dist_4k, arc->dec->distSyms[1], 0, QTM_ENTRIES_MAX,
				  quantum_nclasses((unsigned)((arc->window < 0x1000UL) ? arc->window : 0x1000UL)) );
	QtmInitModel( &arc->dec->dist_256k, arc->dec->distSyms[2], 0, QTM_ENTRIES_MAX,
				  quantum_nclasses((unsigned)((arc->window < 0x40000UL) ? arc->window : 0x40000UL)) );

	// the coder is initialised once for the whole archive
	if( fseek( fp, (long)arc->dataoff, SEEK_SET ) != 0 )
		goto fail;
	if( !QtmArithInit( &arc->dec->arith, quantum_getbit, &arc->dec->in ) )
		goto fail;

	return true;

fail:
	{
		bool had_dec = (arc->dec != NULL);
		QuantumClose( arc );
		if( !had_dec ) fclose( fp );
	}
	return false;
}

void QuantumClose( QuantumArchive* arc )
{
	if( !arc ) return;
	if( arc->dec )
	{
		if( arc->dec->in.fp ) fclose( arc->dec->in.fp );
		if( arc->dec->win ) free( arc->dec->win );
		free( arc->dec );
		arc->dec = NULL;
	}
	if( arc->files ) { free( arc->files ); arc->files = NULL; }
	arc->nfile = 0;
}

bool QuantumDecodeMember( QuantumArchive* arc, int index, const char* outpath )
{
	QuantumDec* d;
	QuantumFile* f;
	FILE* out = NULL;
	unsigned long remaining;
	unsigned long produced = 0;
	unsigned crc = 0;
	unsigned stored;
	bool ok = true;

	if( !arc || !arc->dec ) return false;
	if( index < 0 || index >= arc->nfile ) return false;
	if( index != arc->nextmember ) return false;	// the order matters

	d = arc->dec;
	f = &arc->files[index];
	remaining = f->size;

	if( outpath )
	{
		out = fopen( outpath, "wb" );
		if( !out ) return false;
	}

	while( remaining > 0 )
	{
		int tok, k, sym;

		if( d->in.overrun > QUANTUM_MAXPAD )
		{
			// the compressed stream is exhausted: the archive is
			// truncated or corrupt
			ok = false;
			break;
		}
		if( !QtmDecodeSymbol( &d->token, &d->arith, quantum_getbit, &d->in, &tok ) )
		{
			ok = false;
			break;
		}
		if( tok < 4 )
		{
			// a literal: the token is the group of the byte
			unsigned char b;
			if( !QtmDecodeSymbol( &d->literal[tok], &d->arith,
								  quantum_getbit, &d->in, &sym ) )
			{
				ok = false;
				break;
			}
			b = (unsigned char)sym;
			quantum_win_put( d, b );
			produced++;
			remaining--;
			if( out ) fputc( b, out );
			crc = quantum_crc16( crc, b );
		}
		else if( tok == 4 )
		{
			// a match of 3 bytes, its distance is coded with the model
			// of the distances below 4 K
			unsigned cls, raw;
			unsigned long dist;
			if( !QtmDecodeSymbol( &d->dist_4k, &d->arith,
								  quantum_getbit, &d->in, &sym ) )
			{
				ok = false;
				break;
			}
			cls = (unsigned)sym;
			raw = quantum_bits( &d->in, quantum_pos_bits((int)cls) );
			dist = quantum_pos_base((int)cls) + raw + 1;
			for( k=0; k<3; k++ )
			{
				unsigned char b = d->win[ (d->winpos - dist) & (d->winsize-1) ];
				quantum_win_put( d, b );
				produced++;
				if( out ) fputc( b, out );
				crc = quantum_crc16( crc, b );
			}
			remaining -= 3;
		}
		else if( tok == 5 )
		{
			// a match of 4 bytes, its distance is coded with the model
			// of the distances below 256 K
			unsigned cls, raw;
			unsigned long dist;
			if( !QtmDecodeSymbol( &d->dist_256k, &d->arith,
								  quantum_getbit, &d->in, &sym ) )
			{
				ok = false;
				break;
			}
			cls = (unsigned)sym;
			raw = quantum_bits( &d->in, quantum_pos_bits((int)cls) );
			dist = quantum_pos_base((int)cls) + raw + 1;
			for( k=0; k<4; k++ )
			{
				unsigned char b = d->win[ (d->winpos - dist) & (d->winsize-1) ];
				quantum_win_put( d, b );
				produced++;
				if( out ) fputc( b, out );
				crc = quantum_crc16( crc, b );
			}
			remaining -= 4;
		}
		else
		{
			// a match of 5 to 259 bytes
			int ctxcls;
			unsigned raw, cls;
			unsigned long len, dist;
			if( !QtmDecodeSymbol( &d->ctx, &d->arith,
								  quantum_getbit, &d->in, &ctxcls ) )
			{
				ok = false;
				break;
			}
			// the raw length bits come out before the distance symbol
			raw = quantum_bits( &d->in, QtmLengthExtra[ctxcls] );
			len = QtmLengthBase[ctxcls] + raw + 5;
			if( !QtmDecodeSymbol( &d->dist_full, &d->arith,
								  quantum_getbit, &d->in, &sym ) )
			{
				ok = false;
				break;
			}
			cls = (unsigned)sym;
			raw = quantum_bits( &d->in, quantum_pos_bits((int)cls) );
			dist = quantum_pos_base((int)cls) + raw + 1;
			for( k=0; k<(int)len; k++ )
			{
				unsigned char b = d->win[ (d->winpos - dist) & (d->winsize-1) ];
				quantum_win_put( d, b );
				produced++;
				if( out ) fputc( b, out );
				crc = quantum_crc16( crc, b );
			}
			remaining -= len;
		}
		if( remaining > f->size )
		{
			// the member is corrupt
			ok = false;
			break;
		}
	}

	stored = quantum_bits( &d->in, 16 );
	if( out )
	{
		if( fclose( out ) != 0 ) ok = false;
	}
	arc->nextmember = index + 1;

	if( !ok ) return false;
	if( produced != f->size ) return false;
	if( stored != crc ) return false;
	return true;
}
