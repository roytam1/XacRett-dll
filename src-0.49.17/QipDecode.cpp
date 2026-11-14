// A reader for the QIP archives of the QEMM386 / QPACK installers.
// See QipDecode.h for the format.
//
// The static tables below are the constants of the format: the length and
// the distance group codes and the literal codes of QUNPCK32.DLL, which
// were extracted from that library by the reverse engineering in the
// qunpack/ project of the workarea root.  They are the equivalent of a CRC
// table - the format cannot be decoded without them.

#include "stdafx.h"
#include "QipDecode.h"
#include "zlib/zlib.h"

#include <stdlib.h>
#include <string.h>

//==========================================================================//
// The static tables

// The bit length of the 16 length codes.
static const unsigned char QIP_LENBITS[16] = {
	3,2,3,3,4,4,4,5,5,5,5,6,6,6,7,7,
};

// The code value of the 16 length codes.  The codes are stored LSB first,
// so the value is the bit reversed code.
static const unsigned char QIP_LENBASE[16] = {
	5,3,1,6,10,2,12,20,4,24,8,48,16,32,64,0,
};

// The number of the extra bits of the 16 length codes.
static const unsigned char QIP_LENEXTRA[16] = {
	0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,
};

// The base value of the 16 length codes.
static const unsigned short QIP_LENVALUE[16] = {
	0,1,2,3,4,5,6,7,
	8,10,14,22,38,70,134,262,
};

// The bit length of the 64 distance groups.
static const unsigned char QIP_GRPBITS[64] = {
	2,4,4,5,5,5,5,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};

// The code value of the 64 distance groups.
static const unsigned char QIP_GRPBASE[64] = {
	3,13,5,25,9,17,1,62,30,46,14,54,22,38,6,58,
	26,42,10,50,18,34,66,2,124,60,92,28,108,44,76,12,
	116,52,84,20,100,36,68,4,120,56,88,24,104,40,72,8,
	240,112,176,48,208,80,144,16,224,96,160,32,192,64,128,0,
};

// The code length of the 256 literal symbols (mode 1 only).
static const unsigned char QIP_LITLEN[256] = {
	11,12,12,12,12,12,12,12,12,8,7,12,12,7,12,12,
	12,12,12,12,12,12,12,12,12,12,13,12,12,12,12,12,
	4,10,8,12,10,12,10,8,7,7,8,9,7,6,7,8,
	7,6,7,7,7,7,8,7,7,8,8,12,11,7,9,11,
	12,6,7,6,6,5,7,8,8,6,11,9,6,7,6,6,
	7,11,6,6,6,7,9,8,9,9,11,8,11,9,12,8,
	12,5,6,6,6,5,6,6,6,5,11,7,5,6,5,5,
	6,10,5,5,5,5,8,7,8,8,10,11,11,12,12,12,
	13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
	13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
	13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
	13,12,13,13,13,12,13,13,13,12,13,13,13,13,12,13,
	13,13,12,12,12,13,13,13,13,13,13,13,13,13,13,13,
};

// The code word of the 256 literal symbols (mode 1 only).
static const unsigned short QIP_LITCODE[256] = {
	1168,4064,2016,3040,992,3552,1504,2528,480,184,
	98,3808,1760,34,2784,736,3296,1248,2272,224,
	3936,1888,2912,864,3424,1376,4672,2400,352,3680,
	1632,2656,15,592,56,608,80,3168,912,216,
	66,2,88,432,124,41,60,152,92,9,
	28,108,44,76,24,12,116,232,104,1120,
	144,52,176,1808,2144,49,84,17,33,23,
	20,168,40,1,784,304,62,100,30,46,
	36,1296,14,54,22,68,48,200,464,208,
	272,72,1552,336,96,136,4000,7,38,6,
	58,27,26,42,10,11,528,4,19,50,
	3,29,18,400,13,21,5,25,8,120,
	240,112,656,1040,16,1952,2976,928,576,7232,
	3136,5184,1088,6208,2112,4160,64,8064,3968,6016,
	1920,7040,2944,4992,896,7552,3456,5504,1408,6528,
	2432,4480,384,7808,3712,5760,1664,6784,2688,4736,
	640,7296,3200,5248,1152,6272,2176,4224,128,7936,
	3840,5888,1792,6912,2816,4864,3488,1440,2464,416,
	3744,1696,2720,672,3232,1184,2208,160,3872,1824,
	2848,800,3360,1312,2336,288,3616,1568,2592,544,
	3104,1056,2080,32,4032,1984,3008,960,3520,1472,
	2496,448,3776,1728,2752,704,3264,1216,2240,192,
	3904,1856,2880,832,768,3392,7424,3328,5376,1344,
	1280,6400,2304,2368,4352,256,7680,3584,320,5632,
	1536,6656,3648,1600,2624,2560,4608,512,7168,3072,
	5120,1024,6144,2048,4096,0,
};

//==========================================================================//
// The bit reader
//
// The stream is read with a 32 bit accumulator whose low byte always holds
// the next eight bits of the stream.  The third byte of the block is
// pre-loaded and the counter of the available bits starts at zero, so the
// first read triggers the refill: the accumulator is shifted right, the
// next byte is ORed in at bit 8 and the accumulator is shifted right again.

struct QipBits
{
	const unsigned char*	data;
	DWORD					size;
	DWORD					pos;		// the next byte to load
	unsigned long			acc;		// the bit accumulator
	int						avail;		// the counter of the available bits
};

static void qip_bit_init( QipBits* b, const unsigned char* data, DWORD size )
{
	b->data		= data;
	b->size		= size;
	b->pos		= 3;					// the byte 2 is pre-loaded
	b->acc		= (size > 2) ? data[2] : 0;
	b->avail	= 0;
}

static unsigned long qip_bit_peek( QipBits* b, int n )
{
	if( n <= 0 ) return 0;
	if( n >= 32 ) return b->acc;
	return b->acc & ((1UL << n) - 1UL);
}

// Consume n bits.  Returns 1 when the stream ran out.
static int qip_bit_skip( QipBits* b, int n )
{
	unsigned long edx;
	int cl;

	if( b->avail >= n )
	{
		b->avail -= n;
		b->acc >>= n;
		return 0;
	}
	cl = b->avail;
	b->acc >>= cl;
	if( b->pos >= b->size ) return 1;
	edx = ((unsigned long)b->data[b->pos++] << 8) | (b->acc & 0xFFFFUL);
	cl = n - b->avail;
	b->acc		= edx >> cl;
	b->avail	= b->avail - n + 8;
	return 0;
}

//==========================================================================//
// The decoder

// The window of the LZ77 is 4096 bytes.
#define QIP_WINDOW	4096

// The symbols that are not a literal and not a match.
#define QIP_SYM_EOB		773
#define QIP_SYM_ERROR	774

struct QipDec
{
	QipBits			b;
	int				mode;		// 0 = raw literals, 1 = Huffman literals
	int				codebits;	// 4..6, the width of the low distance bits
	unsigned long	mask;
	unsigned char	len_tab[256];	// the next 8 bits -> the length code
	unsigned char	grp_tab[256];	// the next 8 bits -> the distance group
	// the four look ahead tables of the mode 1 literal code
	unsigned char	lit_len[256];
	unsigned char	lit_c[256];
	unsigned char	lit_d[256];
	unsigned char	lit_e[256];
	unsigned char	lit_b[256];
	unsigned char	ring[QIP_WINDOW];	// the sliding window
	DWORD			pos;		// the number of the bytes produced
};

// Build an eight bit look ahead table for a code whose values are stored
// LSB first: for every symbol, starting at its code value and stepping by
// 2^code_length, the table entry becomes the symbol.
static void qip_build_tab( const unsigned char* bits, const unsigned char* bases,
						   int n, unsigned char* tab )
{
	int i;

	memset( tab, 0, 256 );
	for( i = n-1; i >= 0; i-- )
	{
		unsigned v = bases[i];
		unsigned step = 1u << bits[i];
		while( v < 256 )
		{
			tab[v] = (unsigned char)i;
			v += step;
		}
	}
}

// Build the four look ahead tables of the mode 1 literal code.
//
// The 16 bit QUNPACK.DLL expands them the self consistent way: the symbol
// n uses the word n, the marker goes into the table 1 at (word & 0xFF) and
// the fill base is word>>split.  The 32 bit QUNPCK32.DLL walks the word
// table backwards (the symbol n uses word[255-n]) and stores the marker at
// the whole word, which runs off the end of the table; that is why its
// mode 1 decodes only 69 of the 256 symbols.  The self consistent rule is
// used here.
static void qip_build_lit_tables( QipDec* d )
{
	int sym, v;

	memcpy( d->lit_len, QIP_LITLEN, 256 );
	memset( d->lit_c, 0, 256 );
	memset( d->lit_d, 0, 256 );
	memset( d->lit_e, 0, 256 );
	memset( d->lit_b, 0, 256 );
	for( sym = 255; sym >= 0; sym-- )
	{
		unsigned short w = QIP_LITCODE[sym];
		unsigned char cl = d->lit_len[sym];
		unsigned step;

		if( cl <= 8 )
		{
			v = w; step = 1u << cl;
			while( v < 256 ) { d->lit_c[v] = (unsigned char)sym; v += step; }
		}
		else if( (w & 0xFF) == 0 )			// the 8 bit split
		{
			cl -= 8; d->lit_len[sym] = cl;
			v = (w >> 8) & 0xFFFF; step = 1u << cl;
			while( v < 256 ) { d->lit_b[v] = (unsigned char)sym; v += step; }
		}
		else
		{
			d->lit_c[w & 0xFF] = 0xFF;
			if( (w & 0x3F) != 0 )				// the 4 bit split
			{
				cl -= 4; d->lit_len[sym] = cl;
				v = (w >> 4) & 0xFFFF; step = 1u << cl;
				while( v < 256 ) { d->lit_d[v] = (unsigned char)sym; v += step; }
			}
			else								// the 6 bit split
			{
				cl -= 6; d->lit_len[sym] = cl;
				v = (w >> 6) & 0xFFFF; step = 1u << cl;
				while( v < 128 ) { d->lit_e[v] = (unsigned char)sym; v += step; }
			}
		}
	}
}

// Decode one symbol.
//   0..255   a literal
//   256..772 a match, its length is the value - 254
//   773      the end of the block
//   774      an error
static int qip_decode_symbol( QipDec* d )
{
	QipBits* b = &d->b;
	int i, extra, code, sym;
	unsigned long v;

	if( qip_bit_peek(b,1) & 1 )
	{
		// a match
		if( qip_bit_skip(b,1) ) return QIP_SYM_ERROR;
		i = d->len_tab[ qip_bit_peek(b,8) ];
		if( qip_bit_skip(b, QIP_LENBITS[i]) ) return QIP_SYM_ERROR;
		extra = QIP_LENEXTRA[i];
		code = i;
		if( extra )
		{
			v = qip_bit_peek(b, extra);
			// The end of block marker may end exactly at the end of the
			// data, so the read of its eight extra bits may fail then;
			// that is the only failure that is accepted.
			if( qip_bit_skip(b, extra) && (i + (int)v) != 270 ) return QIP_SYM_ERROR;
			code = QIP_LENVALUE[i] + (int)v;
		}
		return code + 256;
	}

	if( qip_bit_skip(b,1) ) return QIP_SYM_ERROR;	// a literal
	if( d->mode == 0 )								// the raw byte
	{
		v = qip_bit_peek(b,8);
		if( qip_bit_skip(b,8) ) return QIP_SYM_ERROR;
		return (int)v;
	}
	{
		unsigned long lo = qip_bit_peek(b,8);

		if( lo != 0 )
		{
			sym = d->lit_c[lo];
			if( sym == 0xFF )
			{
				if( qip_bit_peek(b,8) & 0x3F )
				{
					if( qip_bit_skip(b,4) ) return QIP_SYM_ERROR;
					sym = d->lit_d[ qip_bit_peek(b,8) ];
				}
				else
				{
					if( qip_bit_skip(b,6) ) return QIP_SYM_ERROR;
					sym = d->lit_e[ qip_bit_peek(b,8) & 0x7F ];
				}
			}
		}
		else
		{
			if( qip_bit_skip(b,8) ) return QIP_SYM_ERROR;
			sym = d->lit_b[ qip_bit_peek(b,8) ];
		}
		if( qip_bit_skip(b, d->lit_len[sym]) ) return QIP_SYM_ERROR;
		return sym;
	}
}

// Decode the distance of a match.  A match of length 2 uses a different
// distance encoding - only two extra bits - so its distance is limited to
// 256.  Returns 0 on an error.
static int qip_decode_dist( QipDec* d, int len2 )
{
	QipBits* b = &d->b;
	int grp;
	unsigned long v;

	grp = d->grp_tab[ qip_bit_peek(b,8) ];
	if( qip_bit_skip(b, QIP_GRPBITS[grp]) ) return 0;
	if( len2 )
	{
		v = (qip_bit_peek(b,2) & 3) | ((unsigned long)grp << 2);
		if( qip_bit_skip(b,2) ) return 0;
	}
	else
	{
		v = (qip_bit_peek(b, d->codebits) & d->mask) |
			((unsigned long)grp << d->codebits);
		if( qip_bit_skip(b, d->codebits) ) return 0;
	}
	return (int)v + 1;
}

// Decode one block.  Returns the buffer (the caller frees it) or NULL.
// The output may not be longer than cap bytes.
static unsigned char* qip_decode_block( const unsigned char* comp, DWORD csize,
										DWORD cap, DWORD* outsize, DWORD* outcrc )
{
	QipDec* d;
	unsigned char* out;
	DWORD len = 0;
	int sym;

	if( csize < 3 ) return NULL;

	d = (QipDec*)malloc( sizeof(QipDec) );
	if( !d ) return NULL;

	d->mode		= comp[0];
	d->codebits	= comp[1];
	if( d->mode != 0 && d->mode != 1 ) { free(d); return NULL; }
	if( d->codebits < 4 || d->codebits > 6 ) { free(d); return NULL; }
	d->mask = 0xFFFFUL >> (16 - d->codebits);
	d->pos = 0;
	memset( d->ring, 0, QIP_WINDOW );

	qip_build_tab( QIP_LENBITS, QIP_LENBASE, 16, d->len_tab );
	qip_build_tab( QIP_GRPBITS, QIP_GRPBASE, 64, d->grp_tab );
	if( d->mode == 1 ) qip_build_lit_tables( d );

	out = (unsigned char*)malloc( cap ? cap : 1 );
	if( !out ) { free(d); return NULL; }

	qip_bit_init( &d->b, comp, csize );

	for(;;)
	{
		sym = qip_decode_symbol( d );
		if( sym >= QIP_SYM_EOB ) break;

		if( sym >= 256 )
		{
			int mlen = sym - 254;			// 2 .. 518
			int dist = qip_decode_dist( d, mlen == 2 );
			int i;

			if( dist <= 0 ) { free(out); free(d); return NULL; }
			if( len + (DWORD)mlen > cap ) { free(out); free(d); return NULL; }
			for( i=0; i<mlen; i++ )
			{
				unsigned char v = d->ring[ (d->pos - (DWORD)dist) & (QIP_WINDOW-1) ];
				d->ring[ d->pos & (QIP_WINDOW-1) ] = v;
				d->pos++;
				out[len++] = v;
			}
		}
		else
		{
			if( len + 1 > cap ) { free(out); free(d); return NULL; }
			d->ring[ d->pos & (QIP_WINDOW-1) ] = (unsigned char)sym;
			d->pos++;
			out[len++] = (unsigned char)sym;
		}
	}
	if( sym != QIP_SYM_EOB ) { free(out); free(d); return NULL; }

	*outsize = len;
	*outcrc  = (DWORD)crc32( 0, out, (uInt)len );
	free( d );
	return out;
}

//==========================================================================//
// The container

static unsigned long qip_le16( const unsigned char* p )
{
	return (unsigned long)p[0] | ((unsigned long)p[1] << 8);
}

static unsigned long qip_le32( const unsigned char* p )
{
	return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
		   ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

// Copy a name of at most len bytes out of the archive.  The name is NUL
// terminated in every archive that was seen, but a name that fills the
// whole field is accepted as well (it is simply cut there).
static void qip_get_name( const unsigned char* src, int len, char* dst, int dstsz )
{
	int i;

	for( i=0; i<len; i++ )
	{
		if( src[i] == 0 ) break;
		dst[i] = (char)src[i];
	}
	if( i >= dstsz ) i = dstsz-1;
	dst[i] = '\0';
}

// The name of a member has to be a plain file name; the archive may not
// send the extraction outside of the output directory.
static bool qip_clean_name( char* dst, int dstsz, const char* src )
{
	int o = 0;
	const char* p = src;

	if( !src || !*src ) return false;
	while( *p )
	{
		const char* q = p;
		int len;
		while( *q && *q != '\\' && *q != '/' ) q++;
		len = (int)(q - p);
		if( len == 0 )
		{
			// an empty component
		}
		else if( len == 1 && p[0] == '.' )
		{
			// the current directory
		}
		else if( len == 2 && p[0] == '.' && p[1] == '.' )
		{
			// a parent directory: dropped, so that a hostile member
			// cannot escape the output directory
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
		if( *p == '\\' || *p == '/' ) p++;
	}
	if( o <= 0 ) return false;
	dst[o] = '\0';
	return true;
}

// Add one member to the list.
static bool qip_add_file( QipArchive* arc, const QipFile* f )
{
	QipFile* n;

	if( arc->nfile >= QIP_MAX_FILES ) return false;
	n = (QipFile*)realloc( arc->files, sizeof(QipFile) * (arc->nfile + 1) );
	if( !n ) return false;
	arc->files = n;
	arc->files[arc->nfile] = *f;
	arc->nfile++;
	return true;
}

bool QipOpen( const char* fname, QipArchive* arc )
{
	FILE* fp;
	unsigned char hdr[32];
	unsigned char rec[32];
	unsigned long fsize;
	unsigned long pos;
	unsigned long hdrlen;
	unsigned long count = 0;
	unsigned long idxsize;
	int version = 0;
	char curdir[MAX_PATH];
	char name[MAX_PATH];

	memset( arc, 0, sizeof(*arc) );
	curdir[0] = '\0';

	fp = fopen( fname, "rb" );
	if( !fp ) return false;
	fseek( fp, 0, SEEK_END );
	fsize = (unsigned long)ftell( fp );
	fseek( fp, 0, SEEK_SET );

	if( fsize < 16 ) { fclose(fp); return false; }
	if( fread( hdr, 1, 16, fp ) != 16 ) { fclose(fp); return false; }

	if( hdr[0]=='Q' && hdr[1]=='P' )
	{
		version = (int)qip_le16( hdr + 8 );
		count	= qip_le16( hdr + 2 );
		idxsize	= qip_le32( hdr + 4 );
		if( version != 1 && version != 2 ) { fclose(fp); return false; }
		if( count == 0 || count > QIP_MAX_FILES ) { fclose(fp); return false; }
		if( idxsize != count * 16UL ) { fclose(fp); return false; }
		// the chain of the blocks starts right behind the index
		pos = 16 * (count + 1);
	}
	else if( hdr[0]=='Q' && hdr[1]=='D' )
	{
		// a raw (version 1) archive: the chain starts at the beginning
		version = 1;
		pos = 0;
	}
	else
	{
		fclose(fp);
		return false;
	}

	arc->fp = fp;
	arc->version = version;
	arc->count = (int)count;
	hdrlen = (version == 1) ? 10UL : 14UL;

	while( pos + hdrlen <= fsize )
	{
		unsigned long type, csize, flags, crc = 0;

		if( fseek( fp, (long)pos, SEEK_SET ) != 0 ) break;
		if( fread( hdr, 1, hdrlen, fp ) != hdrlen ) break;
		if( hdr[0] != 'Q' || hdr[1] != 'D' ) break;

		type  = qip_le16( hdr + 2 );
		csize = qip_le32( hdr + 4 );
		flags = qip_le16( hdr + 8 );
		if( version == 2 ) crc = qip_le32( hdr + 10 );

		if( type == 1 )
		{
			// a directory: its name follows the header
			if( csize > fsize - pos - hdrlen ) break;
			if( csize == 0 || csize >= sizeof(name) ) break;
			if( fread( name, 1, csize, fp ) != csize ) break;
			name[csize] = '\0';
			if( !qip_clean_name( curdir, sizeof(curdir), name ) )
				curdir[0] = '\0';
			pos += hdrlen + csize;
			continue;
		}
		if( type != 0 ) break;

		// a file: the 22 byte record and then the compressed data
		if( fsize - pos - hdrlen < 22 ) break;
		if( csize > fsize - pos - hdrlen - 22 ) break;
		if( fread( rec, 1, 22, fp ) != 22 ) break;

		{
			QipFile f;

			memset( &f, 0, sizeof(f) );
			qip_get_name( rec + 9, 13, name, sizeof(name) );
			if( !qip_clean_name( f.path, sizeof(f.path), name ) )
			{
				// a member without a usable name is skipped, the chain
				// of the blocks is still walked
				pos += hdrlen + 22 + csize;
				continue;
			}
			if( curdir[0] )
			{
				int dl = lstrlen(curdir);
				int nl = lstrlen(f.path);
				if( dl + 1 + nl >= (int)sizeof(f.path) )
				{
					pos += hdrlen + 22 + csize;
					continue;
				}
				memmove( f.path+dl+1, f.path, nl+1 );
				memcpy( f.path, curdir, dl );
				f.path[dl] = '\\';
			}
			f.blockpos	= (DWORD)pos;
			f.csize		= (DWORD)csize;
			f.orig		= (DWORD)qip_le32( rec + 5 );
			f.crc		= (DWORD)crc;
			f.flags		= (WORD)flags;
			if( !qip_add_file( arc, &f ) ) break;
		}
		pos += hdrlen + 22 + csize;
	}

	return true;
}

void QipClose( QipArchive* arc )
{
	if( arc->fp ) { fclose( arc->fp ); arc->fp = NULL; }
	if( arc->files ) { free( arc->files ); arc->files = NULL; }
	arc->nfile = 0;
}

bool QipExtract( QipArchive* arc, int index, const char* outpath )
{
	QipFile* f;
	unsigned char* comp;
	unsigned char* buf;
	DWORD outsz = 0, crc = 0;
	unsigned long dstart;
	unsigned long hdrlen;
	FILE* out;
	bool ok = true;

	if( !arc || !arc->fp ) return false;
	if( index < 0 || index >= arc->nfile ) return false;
	f = &arc->files[index];

	if( f->orig > QIP_MAX_MEMBER ) return false;

	hdrlen = (arc->version == 1) ? 10UL : 14UL;
	dstart = (unsigned long)f->blockpos + hdrlen + 22;

	comp = (unsigned char*)malloc( f->csize ? f->csize : 1 );
	if( !comp ) return false;
	if( fseek( arc->fp, (long)dstart, SEEK_SET ) != 0 ) { free(comp); return false; }
	if( fread( comp, 1, f->csize, arc->fp ) != f->csize ) { free(comp); return false; }

	// the archive knows the size of the member, so the decoder is bounded
	// by it: a corrupt archive cannot make it write forever
	buf = qip_decode_block( comp, f->csize, f->orig, &outsz, &crc );
	free( comp );
	if( !buf ) return false;

	// the size and, in a version 2 archive, the checksum have to match
	if( outsz != f->orig ) { free(buf); return false; }
	if( arc->version == 2 && crc != f->crc ) { free(buf); return false; }

	out = fopen( outpath, "wb" );
	if( !out ) { free(buf); return false; }
	if( outsz && fwrite( buf, 1, outsz, out ) != outsz ) ok = false;
	if( fclose( out ) != 0 ) ok = false;
	free( buf );
	return ok;
}
