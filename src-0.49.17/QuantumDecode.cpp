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

// A model is rescaled when its total is above this.
#define QUANTUM_RESCALE	0xED8
// The number of the padding bytes a valid archive may need at its end.
#define QUANTUM_MAXPAD	64

//==========================================================================//
// The tables

// The number of the raw bits of the length class (27 classes).
static const unsigned char CTX_BITS[27] = {
	0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,
	4,4,4,4,5,5,5,5,0
};

// The number of the raw bits of the distance class (43 classes).
static const unsigned char DIST_BITS[43] = {
	0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,
	8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,
	16,16,17,17,18,18,19
};

// The bases are the running sums of 1 << bits.
static unsigned CTX_BASE[27];
static unsigned DIST_BASE[43];

static void quantum_make_bases( void )
{
	int i;
	unsigned acc = 0;

	for( i=0; i<27; i++ )
	{
		CTX_BASE[i] = acc;
		acc += 1u << CTX_BITS[i];
	}
	acc = 0;
	for( i=0; i<43; i++ )
	{
		DIST_BASE[i] = acc;
		acc += 1u << DIST_BITS[i];
	}
}

// The number of the distance classes that are valid below a limit.
static int quantum_nclasses( unsigned limit )
{
	int n = 0;

	while( n < 43 && DIST_BASE[n] < limit ) n++;
	return n;
}

//==========================================================================//
// The models

static void quantum_model_init( QuantumModel* m, int n, int total, int base )
{
	int i;

	m->n = n;
	for( i=0; i<=n; i++ )
		m->cum[i] = (unsigned short)((total - i) & 0xFFFF);
	for( i=0; i<256; i++ )
		m->inv[i] = 0;
	for( i=0; i<n; i++ )
	{
		m->sym[i] = (unsigned char)(base + i);
		m->inv[m->sym[i]] = (unsigned char)i;
	}
	m->counter = 4;
}

static void quantum_model_update( QuantumModel* m, int rank )
{
	int i, j;
	unsigned short t;
	unsigned char s;

	for( i=0; i<=rank; i++ )
		m->cum[i] = (unsigned short)(m->cum[i] + 8);
	if( m->cum[0] <= QUANTUM_RESCALE )
		return;
	m->counter--;
	if( m->counter )
	{
		// the light rescale: halve the counts and keep them strictly
		// decreasing
		for( i=m->n-1; i>=0; i-- )
		{
			m->cum[i] = (unsigned short)(m->cum[i] >> 1);
			if( m->cum[i] <= m->cum[i+1] )
				m->cum[i] = (unsigned short)(m->cum[i+1] + 1);
		}
	}
	else
	{
		// the full rescale: the counts become frequencies, the entries
		// are sorted by their frequency (the symbol moves with its
		// count) and the frequencies become cumulative counts again
		m->counter = 50;
		for( i=0; i<m->n; i++ )
			m->cum[i] = (unsigned short)((((m->cum[i] - m->cum[i+1]) & 0xFFFF) + 1) >> 1);
		for( i=0; i<m->n; i++ )
		{
			for( j=i+1; j<m->n; j++ )
			{
				if( m->cum[i] < m->cum[j] )
				{
					t = m->cum[i]; m->cum[i] = m->cum[j]; m->cum[j] = t;
					s = m->sym[i];  m->sym[i]  = m->sym[j];  m->sym[j]  = s;
				}
			}
		}
		for( i=m->n-1; i>=0; i-- )
			m->cum[i] = (unsigned short)(m->cum[i] + m->cum[i+1]);
		for( i=0; i<m->n; i++ )
			m->inv[m->sym[i]] = (unsigned char)i;
	}
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
// The arithmetic decoder

static void quantum_arith_init( QuantumDec* d )
{
	int i;

	d->low = 0;
	d->high = 0xFFFF;
	d->code = 0;
	for( i=0; i<16; i++ )
		d->code = ((d->code << 1) | (unsigned)quantum_bit(&d->in)) & 0xFFFF;
}

static void quantum_arith_update( QuantumDec* d, unsigned base, unsigned cum, unsigned total )
{
	unsigned rng;

	rng = d->high - d->low + 1;
	d->high = (d->low + (unsigned)(((unsigned long)rng * cum) / total) - 1) & 0xFFFF;
	d->low  = (d->low + (unsigned)(((unsigned long)rng * base) / total)) & 0xFFFF;
	for(;;)
	{
		if( (d->high & 0x8000) == (d->low & 0x8000) )
		{
			// the bit is decided
		}
		else if( (d->low & 0x4000) && !(d->high & 0x4000) )
		{
			// the underflow
			d->code ^= 0x4000;
			d->low  &= 0x3FFF;
			d->high |= 0x4000;
		}
		else
		{
			break;
		}
		d->low  = (d->low  << 1) & 0xFFFF;
		d->high = ((d->high << 1) | 1) & 0xFFFF;
		d->code = (d->code << 1) & 0xFFFF;
		d->code = (d->code + (unsigned)quantum_bit(&d->in)) & 0xFFFF;
	}
}

// Decode one symbol of a model.
static int quantum_model_decode( QuantumModel* m, QuantumDec* d )
{
	unsigned total, val;
	int i, sym;

	total = m->cum[0];
	val = ((d->code - d->low + 1) * total - 1) / (d->high - d->low + 1);
	i = 0;
	while( i < m->n && val < m->cum[i+1] )
		i++;
	if( i >= m->n )
		i = m->n - 1;
	sym = m->sym[i];
	quantum_arith_update( d, m->cum[i+1], m->cum[i], total );
	quantum_model_update( m, i );
	return sym;
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

	quantum_make_bases();
	quantum_model_init( &arc->dec->token, 7, 7, 0 );
	for( i=0; i<4; i++ )
		quantum_model_init( &arc->dec->literal[i], 64, 64, 64*i );
	quantum_model_init( &arc->dec->ctx, 27, 27, 0 );
	quantum_model_init( &arc->dec->dist_full, 43,
						quantum_nclasses((unsigned)arc->window), 0 );
	quantum_model_init( &arc->dec->dist_4k, 43,
						quantum_nclasses((unsigned)((arc->window < 0x1000UL) ? arc->window : 0x1000UL)), 0 );
	quantum_model_init( &arc->dec->dist_256k, 43,
						quantum_nclasses((unsigned)((arc->window < 0x40000UL) ? arc->window : 0x40000UL)), 0 );

	// the coder is initialised once for the whole archive
	if( fseek( fp, (long)arc->dataoff, SEEK_SET ) != 0 )
		goto fail;
	quantum_arith_init( arc->dec );

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
		int tok, k;

		if( d->in.overrun > QUANTUM_MAXPAD )
		{
			// the compressed stream is exhausted: the archive is
			// truncated or corrupt
			ok = false;
			break;
		}
		tok = quantum_model_decode( &d->token, d );
		if( tok < 4 )
		{
			// a literal: the token is the group of the byte
			unsigned char b = (unsigned char)quantum_model_decode( &d->literal[tok], d );
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
			unsigned cls = (unsigned)quantum_model_decode( &d->dist_4k, d );
			unsigned raw = quantum_bits( &d->in, DIST_BITS[cls] );
			unsigned long dist = DIST_BASE[cls] + raw + 1;
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
			unsigned cls = (unsigned)quantum_model_decode( &d->dist_256k, d );
			unsigned raw = quantum_bits( &d->in, DIST_BITS[cls] );
			unsigned long dist = DIST_BASE[cls] + raw + 1;
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
			int ctxcls = quantum_model_decode( &d->ctx, d );
			unsigned raw = quantum_bits( &d->in, CTX_BITS[ctxcls] );
			unsigned long len = CTX_BASE[ctxcls] + raw + 5;
			unsigned cls = (unsigned)quantum_model_decode( &d->dist_full, d );
			unsigned long dist;
			raw = quantum_bits( &d->in, DIST_BITS[cls] );
			dist = DIST_BASE[cls] + raw + 1;
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
