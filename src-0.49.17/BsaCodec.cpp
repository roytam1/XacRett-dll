// Decompressors for the BSA/BSN archives.  See BsaCodec.h.
//
// The three decoders below are a straight port of the proven decoding
// paths of the recon tool in bsabsn/ (bsn_lz.c, bsn_m23.c, bsn_ppm.c),
// which were written from the disassembly of bsa32.exe.  Only the entry
// points, the names of the internal helpers and the state reset are
// different; the bit level behaviour must not drift, because the model
// state of the PPM coder and the sliding windows of the LZ coders are
// part of the format.

#include "stdafx.h"
#include "BsaCodec.h"

//==========================================================================//
// method 7: LZ77 + dynamic canonical Huffman
//
// sub_413940 (LZ77 + dynamic Huffman), sub_413260 / sub_413750 /
// sub_413490 (Huffman header + canonical table builder), sub_404DA0
// (MSB-first bit reader).
//
//   bitstream is MSB-first over bytes.
//   block := count:16
//            meta:  count19:5, then (count19) code lengths coded as
//                   "3-bit value, if 7 then unary ones terminated by 0",
//                   with a 2-bit zero-run inserted after each 3rd length.
//            litlen: count510:9, then (count510) code lengths coded with the
//                   meta Huffman alphabet: 0..2 are zero runs, 3.. are len-2.
//            dist:  count17:5, then (count17) code lengths (same coding).
//            count tokens.
//   token := symbol (<256 literal, else length=symbol-253)
//            if match: dist symbol d; d<=1 -> distance d+1,
//                      else extra=d-1 bits, distance=(1<<extra)+val+1.
//==========================================================================//

#define LZ_MAXBITS 20

typedef struct
{
	const unsigned char*	data;
	unsigned long			size;
	unsigned long			bitpos;
} LZBR;

static unsigned long lz_getbit( LZBR* br )
{
	unsigned long byi = br->bitpos >> 3;
	unsigned int bit;
	if( byi >= br->size ) { br->bitpos++; return 0; }
	bit = (br->data[byi] >> (7 - (br->bitpos & 7))) & 1;
	br->bitpos++;
	return bit;
}

static unsigned long lz_read( LZBR* br, int n )
{
	unsigned long v = 0;
	int i;
	for( i=0; i<n; i++ )
		v = (v << 1) | lz_getbit(br);
	return v;
}

static unsigned long lz_peek( LZBR* br, int n )
{
	unsigned long p = br->bitpos;
	unsigned long v = lz_read(br, n);
	br->bitpos = p;
	return v;
}

// Canonical Huffman, MSB-first.  A "const" table decodes a fixed symbol
// consuming zero bits (the original fills the fast table with a constant
// when the code count is 0).
typedef struct
{
	int	maxlen;
	int	count[LZ_MAXBITS+1];
	int	first[LZ_MAXBITS+1];
	int	off[LZ_MAXBITS+1];
	int	nsyms;
	int	sym_len[LZ_MAXBITS*520];
	int	sym_val[LZ_MAXBITS*520];
	int	is_const;
	int	const_sym;
} LZHT;

static int lz_ht_build( LZHT* t, const int* lens, int n )
{
	int l, s, k, code, total;
	memset( t, 0, sizeof(*t) );
	t->maxlen = 0;
	for( s=0; s<n; s++ )
	{
		l = lens[s];
		if( l<0 || l>LZ_MAXBITS ) return -1;
		if( l > t->maxlen ) t->maxlen = l;
		if( l ) t->count[l]++;
	}
	code = 0; total = 0;
	for( l=1; l<=t->maxlen; l++ )
	{
		t->first[l] = code;
		t->off[l] = total;
		code = (code + t->count[l]) << 1;
		total += t->count[l];
	}
	k = 0;
	for( l=1; l<=t->maxlen; l++ )
	{
		for( s=0; s<n; s++ )
		{
			if( lens[s] == l )
			{
				t->sym_len[k] = l;
				t->sym_val[k] = s;
				k++;
			}
		}
	}
	t->nsyms = k;
	t->is_const = 0;
	t->const_sym = 0;
	return k ? 0 : -1;
}

static void lz_ht_build_const( LZHT* t, int sym )
{
	memset( t, 0, sizeof(*t) );
	t->is_const = 1;
	t->const_sym = sym;
}

static int lz_ht_decode( LZBR* br, const LZHT* t )
{
	int l, code = 0, d;
	if( t->is_const ) return t->const_sym;
	for( l=1; l<=t->maxlen; l++ )
	{
		code = (code << 1) | (int)lz_getbit(br);
		d = code - t->first[l];
		if( t->count[l] && d >= 0 && d < t->count[l] )
			return t->sym_val[t->off[l] + d];
	}
	return -1;
}

// Read one "3-bit value, 7 -> unary extension" code.
static int lz_read_codevalue( LZBR* br )
{
	int top = (int)lz_peek( br, 3 );
	int extra;
	if( top < 7 )
	{
		lz_read( br, 3 );
		return top;
	}
	extra = 0;
	lz_read( br, 3 );
	while( lz_getbit(br) ) extra++;
	return 7 + extra;
}

// sub_413260: read code lengths for the meta/distance alphabet.
// cnt==0 means a constant table whose special value is stored in
// *const_sym (>=0); *const_sym is -1 otherwise.
static int lz_read_codelens( LZBR* br, int* lens, int n, int cntbits, int gap,
							 int* const_sym )
{
	int cnt = (int)lz_read( br, cntbits );
	int i = 0;
	memset( lens, 0, sizeof(int) * n );
	*const_sym = -1;
	if( cnt == 0 )
	{
		*const_sym = (int)lz_read( br, cntbits );
		return 0;
	}
	while( i < cnt )
	{
		int v = lz_read_codevalue( br );
		if( i < n ) lens[i] = v;
		i++;
		if( gap && i == gap )
		{
			int z = (int)lz_read( br, 2 );
			while( z-- > 0 && i < n ) { lens[i] = 0; i++; }
		}
	}
	return 0;
}

// sub_413750: read the 510 literal/length code lengths via the meta tree.
static int lz_read_litlens( LZBR* br, const LZHT* meta, int* lens, int* const_sym )
{
	int cnt = (int)lz_read( br, 9 );
	int i = 0;
	memset( lens, 0, sizeof(int) * 510 );
	*const_sym = -1;
	if( cnt == 0 )
	{
		*const_sym = (int)lz_read( br, 9 );
		return 0;
	}
	while( i < cnt )
	{
		int v = lz_ht_decode( br, meta );
		if( v < 0 ) return -1;
		if( v > 2 )
		{
			if( i < 510 ) lens[i] = v - 2;
			i++;
			continue;
		}
		if( v == 0 )
		{
			if( i < 510 ) lens[i] = 0;
			i++;
		}
		else if( v == 1 )
		{
			int run = (int)lz_read( br, 4 ) + 3;
			while( run-- > 0 && i < 510 ) { lens[i] = 0; i++; }
		}
		else
		{
			int run = (int)lz_read( br, 9 ) + 20;
			while( run-- > 0 && i < 510 ) { lens[i] = 0; i++; }
		}
	}
	return 0;
}

// 128 KiB sliding window, persistent across the entries of one archive
// (the original's static window buffer is never cleared).
#define LZ_RING_MASK 0x1FFFFUL
static unsigned char lz_ring[131072];
static unsigned long lz_wpos = 0;

unsigned char* BsaDecodeMethod7( const unsigned char* packed, unsigned long psize,
								 unsigned long outsize, unsigned long* produced )
{
	LZBR br;
	unsigned char* out;
	unsigned long opos = 0;
	int metalens[19], litlens[510], dlens[17];
	LZHT meta, lit, dist;
	int n, i, s;
	int meta_const, lit_const, dist_const;

	*produced = 0;
	br.data = packed; br.size = psize; br.bitpos = 0;
	out = (unsigned char*)malloc( outsize ? outsize : 1 );
	if( !out ) return 0;

	n = (int)lz_read( &br, 16 );

	for(;;)
	{
		// ---- block header ----
		lz_read_codelens( &br, metalens, 19, 5, 3, &meta_const );
		if( meta_const >= 0 ) lz_ht_build_const( &meta, meta_const );
		else if( lz_ht_build( &meta, metalens, 19 ) != 0 ) { free(out); return 0; }
		if( lz_read_litlens( &br, &meta, litlens, &lit_const ) != 0 ) { free(out); return 0; }
		if( lit_const >= 0 ) lz_ht_build_const( &lit, lit_const );
		else if( lz_ht_build( &lit, litlens, 510 ) != 0 ) { free(out); return 0; }
		lz_read_codelens( &br, dlens, 17, 5, 0, &dist_const );
		if( dist_const >= 0 ) lz_ht_build_const( &dist, dist_const );
		else if( lz_ht_build( &dist, dlens, 17 ) != 0 ) { free(out); return 0; }

		// ---- tokens ----
		for( i=0; i<n; i++ )
		{
			if( opos >= outsize ) goto done;
			s = lz_ht_decode( &br, &lit );
			if( s < 0 ) { free(out); return 0; }
			if( s < 256 )
			{
				lz_ring[lz_wpos] = (unsigned char)s;
				lz_wpos = (lz_wpos + 1) & LZ_RING_MASK;
				out[opos++] = (unsigned char)s;
			}
			else
			{
				int len = s - 253;
				int d = lz_ht_decode( &br, &dist );
				unsigned long distance;
				int j;
				if( d < 0 ) { free(out); return 0; }
				if( d <= 1 ) distance = (unsigned long)d + 1;
				else
				{
					int eb = d - 1;
					unsigned long e = lz_read( &br, eb );
					distance = (1UL << eb) + e + 1;
				}
				for( j=0; j<len; j++ )
				{
					unsigned char c;
					if( opos >= outsize ) goto done;
					c = lz_ring[(lz_wpos - distance) & LZ_RING_MASK];
					lz_ring[lz_wpos] = c;
					lz_wpos = (lz_wpos + 1) & LZ_RING_MASK;
					out[opos++] = c;
				}
			}
		}
		if( opos >= outsize ) break;
		n = (int)lz_read( &br, 16 );
		if( n == 0 && (br.bitpos >> 3) >= psize ) break;
	}
done:
	*produced = opos;
	return out;
}

//==========================================================================//
// methods 2/3/5/6: LZSS + dynamic Huffman (BSA200)
//
// sub_208C4 (main loop), seg004:052E (block header), sub_1F6E7 (pre-table
// RLE), sub_1F7F6 (lit/len table), sub_1F946 (distance), wrappers
// seg018:031F (method 2) / seg018:038F (method 3), sub_20808 (length
// extension), sub_20A91 (canonical table builder), sub_20631 (MSB-first
// bit reader).
//
// Per block: u16 token count; 19 code lengths (5-bit count, with a 2-bit
// zero-run inserted after every 3rd length); 510 literal/length code
// lengths coded with the 19-symbol alphabet (0/1/2 are zero runs of 1,
// 3+extra4, 0x14+extra9; else len-2); 14 distance code lengths (4-bit
// count, no zero gap); then tokens.  Token < 0x100 is a literal, else
// length = symbol-0xFD.  Distance base from the distance code:
// 0 -> 0, else (1<<(c-1))|extra(c-1) bits; method 2 adds 0x2000 for
// symbols >= 0x180 (symbol -= 0x80); method 3 has three tiers
// (0x140/0x180/0x1C0 -> +0x2000/0x4000/0x6000, symbol -= 0x40/0x80/0xC0)
// and extends length when the symbol is 0x13D.  Method 5 uses the method 2
// wrapper plus the length extension when the symbol is 0x17D; method 6
// uses the method 3 wrapper.  Distance = base+1, 64 KiB ring.  Termination
// is output-driven (orig byte count).
//==========================================================================//

#define M23_MAXBITS 16
#define M23_MAXSYM 510

typedef struct
{
	const unsigned char*	data;
	unsigned long			size;
	unsigned long			bitpos;
	int						eof;	// sticky: set once input is over-read
} M23BR;

static int m23_getbit( M23BR* br )
{
	unsigned long byi = br->bitpos >> 3;
	int bit;
	if( byi >= br->size ) { br->eof = 1; br->bitpos++; return 1; }
	bit = (br->data[byi] >> (7 - (br->bitpos & 7))) & 1;
	br->bitpos++;
	return bit;
}

static unsigned long m23_read( M23BR* br, int n )
{
	unsigned long v = 0;
	int i;
	for( i=0; i<n; i++ )
		v = (v << 1) | (unsigned long)m23_getbit(br);
	return v;
}

typedef struct
{
	int	maxlen;
	int	count[M23_MAXBITS+1];
	int	first[M23_MAXBITS+1];
	int	off[M23_MAXBITS+1];
	int	nsyms;
	int	is_const;
	int	const_sym;
	int	sym_len[M23_MAXSYM];
	int	sym_val[M23_MAXSYM];
} M23HT;

static void m23_ht_build_const( M23HT* t, int sym )
{
	memset( t, 0, sizeof(*t) );
	t->is_const = 1;
	t->const_sym = sym;
}

static int m23_ht_build( M23HT* t, const int* lens, int n )
{
	int l, s, k, code, total;
	long kraft = 0;
	memset( t, 0, sizeof(*t) );
	t->maxlen = 0;
	for( s=0; s<n; s++ )
	{
		l = lens[s];
		if( l<0 || l>M23_MAXBITS ) return -1;
		if( l ) t->count[l]++;
		if( l > t->maxlen ) t->maxlen = l;
	}
	// Kraft sum over the 16-bit code space must be exact
	for( l=1; l<=t->maxlen; l++ )
		kraft += (long)t->count[l] << (16 - l);
	if( kraft != 65536L ) return -1;
	code = 0; total = 0;
	for( l=1; l<=t->maxlen; l++ )
	{
		t->first[l] = code;
		t->off[l] = total;
		code = (code + t->count[l]) << 1;
		total += t->count[l];
	}
	k = 0;
	for( l=1; l<=t->maxlen; l++ )
	{
		for( s=0; s<n; s++ )
		{
			if( lens[s] == l )
			{
				t->sym_len[k] = l;
				t->sym_val[k] = s;
				k++;
			}
		}
	}
	t->nsyms = k;
	t->is_const = 0;
	t->const_sym = 0;
	return 0;
}

static int m23_ht_decode( M23BR* br, const M23HT* t )
{
	int l, code = 0, d;
	if( t->is_const ) return t->const_sym;
	for( l=1; l<=t->maxlen; l++ )
	{
		code = (code << 1) | m23_getbit(br);
		if( br->eof ) return -1;
		d = code - t->first[l];
		if( t->count[l] && d >= 0 && d < t->count[l] )
			return t->sym_val[t->off[l] + d];
	}
	return -1;
}

static int m23_read_codevalue( M23BR* br )
{
	int top = (int)m23_read( br, 3 );
	int extra;
	if( top < 7 || br->eof ) return top;
	extra = 0;
	while( m23_getbit(br) ) extra++;
	return 7 + extra;
}

typedef struct
{
	M23BR	br;
	M23HT	pre, lit, dist;
	int		prelens[64];
	int		litlens[M23_MAXSYM + 2];
	int		distlens[64];
	int		blkcount;
	int		method;
} M23;

// sub_1F6E7(special, kbit, count): read a pre/distance alphabet.
static int m23_read_pre( M23* m, int* lens, M23HT* t, int count, int kbit, int special )
{
	int n, i, c, v, mval;
	n = (int)m23_read( &m->br, kbit );
	if( m->br.eof ) return -1;
	memset( lens, 0, sizeof(int) * count );
	if( n == 0 )
	{
		mval = (int)m23_read( &m->br, kbit );
		if( m->br.eof ) return -1;
		m23_ht_build_const( t, mval );
		return 0;
	}
	i = 0;
	while( i < n )
	{
		c = m23_read_codevalue( &m->br );
		if( m->br.eof ) return -1;
		if( i < count ) lens[i] = c;
		i++;
		if( i == special )
		{
			v = (int)m23_read( &m->br, 2 );
			if( m->br.eof ) return -1;
			while( v-- > 0 ) { if( i < count ) lens[i] = 0; i++; }
		}
	}
	while( i < count ) { lens[i] = 0; i++; }
	if( m23_ht_build( t, lens, count ) != 0 ) return -1;
	return 0;
}

// sub_1F7F6: read the 510 literal/length code lengths via the pre tree.
static int m23_read_lit( M23* m )
{
	int n, i, c, v, mval;
	n = (int)m23_read( &m->br, 9 );
	if( m->br.eof ) return -1;
	if( n == 0 )
	{
		mval = (int)m23_read( &m->br, 9 );
		if( m->br.eof ) return -1;
		memset( m->litlens, 0, sizeof(m->litlens) );
		m23_ht_build_const( &m->lit, mval );
		return 0;
	}
	memset( m->litlens, 0, sizeof(m->litlens) );
	i = 0;
	while( i < n )
	{
		c = m23_ht_decode( &m->br, &m->pre );
		if( c < 0 ) return -1;
		if( c > 2 )
		{
			if( i < 510 ) m->litlens[i] = c - 2;
			i++;
			continue;
		}
		if( c == 0 ) v = 1;
		else if( c == 1 ) v = 3 + (int)m23_read( &m->br, 4 );
		else v = 0x14 + (int)m23_read( &m->br, 9 );
		if( m->br.eof ) return -1;
		while( v-- > 0 ) { if( i < 510 ) m->litlens[i] = 0; i++; }
	}
	while( i < 510 ) { m->litlens[i] = 0; i++; }
	if( m23_ht_build( &m->lit, m->litlens, 510 ) != 0 ) return -1;
	return 0;
}

// seg004:052E block header shared by methods 2/3/5/6.
static int m23_read_header( M23* m )
{
	m->blkcount = (int)m23_read( &m->br, 16 );
	if( m->br.eof ) return -1;
	if( m23_read_pre( m, m->prelens, &m->pre, 19, 5, 3 ) != 0 ) return -1;
	if( m23_read_lit( m ) != 0 ) return -1;
	if( m23_read_pre( m, m->distlens, &m->dist, 14, 4, -1 ) != 0 ) return -1;
	return 0;
}

// sub_20808: length extension (methods 3/5/6).
static int m23_decode_ext( M23* m )
{
	if( --m->blkcount == 0 )
	{
		if( m23_read_header( m ) != 0 ) return -1;
	}
	return m23_ht_decode( &m->br, &m->lit );
}

// sub_1F946 + method wrapper.  Returns the distance base, or -1.
static int m23_decode_dist( M23* m, int* sym )
{
	int c, base, e, x;
	c = m23_ht_decode( &m->br, &m->dist );
	if( c < 0 ) return -1;
	if( c == 0 ) base = 0;
	else
	{
		e = (int)m23_read( &m->br, c - 1 );
		if( m->br.eof ) return -1;
		base = (1 << (c - 1)) | e;
	}
	if( m->method == 2 )
	{
		if( *sym >= 0x180 ) { base += 0x2000; *sym -= 0x80; }
	}
	else if( m->method == 5 )
	{
		if( *sym >= 0x180 ) { base += 0x2000; *sym -= 0x80; }
		if( *sym == 0x17D )
		{
			x = m23_decode_ext( m );
			if( x < 0 ) return -1;
			*sym += x;
		}
	}
	else
	{
		// methods 3 and 6 (wrappers seg018:038F and seg018:033A)
		if( *sym >= 0x1C0 ) { base += 0x6000; *sym -= 0xC0; }
		else if( *sym >= 0x180 ) { base += 0x4000; *sym -= 0x80; }
		else if( *sym >= 0x140 ) { base += 0x2000; *sym -= 0x40; }
		if( *sym == 0x13D )
		{
			x = m23_decode_ext( m );
			if( x < 0 ) return -1;
			*sym += x;
		}
	}
	return base;
}

// 64 KiB sliding window, persistent across the entries of one archive.
static unsigned char m23_ring[65536];
static unsigned long m23_wpos = 0;

unsigned char* BsaDecodeMethod23( const unsigned char* packed, unsigned long psize,
								  const unsigned char* trail, unsigned long tsize,
								  unsigned long outsize, unsigned long* produced,
								  int method )
{
	M23 m;
	unsigned char* inbuf;
	unsigned char* out;
	unsigned long opos = 0;
	unsigned long rhi;		// remaining-high + 1 (word_27E3C)
	unsigned int rlo;		// remaining-low (bp)
	int s, sym, len, base;

	*produced = 0;
	if( method != 2 && method != 3 && method != 5 && method != 6 ) return 0;

	out = (unsigned char*)malloc( outsize ? outsize : 1 );
	if( !out ) return 0;

	// input = packed stream + trailing archive bytes, like the original
	// sees them (its bit reader runs past the packed end into file data)
	inbuf = (unsigned char*)malloc( (psize + tsize) ? (psize + tsize) : 1 );
	if( !inbuf ) { free(out); return 0; }
	if( psize ) memcpy( inbuf, packed, psize );
	if( tsize ) memcpy( inbuf + psize, trail, tsize );

	memset( &m, 0, sizeof(m) );
	m.br.data = inbuf;
	m.br.size = psize + tsize;
	m.br.bitpos = 0;
	m.br.eof = 0;
	m.method = method;
	m.blkcount = 1;

	if( outsize == 0 ) { free(inbuf); return out; }

	rhi = (outsize >> 16) + 1;
	rlo = (unsigned int)(outsize & 0xFFFF);

	// no prime read needed: the original primes a zeroed bit buffer,
	// which loads (not discards) the first 16 bits; the block count
	// read below consumes exactly those bits.

	// loop-top entry decrement (loc_2092E first pass)
	if( rlo == 0 ) { rlo = 0xFFFF; if( --rhi == 0 ) goto done; }
	else rlo--;

	for(;;)
	{
		if( --m.blkcount == 0 )
		{
			if( m23_read_header( &m ) != 0 ) goto done;
		}
		if( m.br.eof ) goto done;
		s = m23_ht_decode( &m.br, &m.lit );
		if( s < 0 ) goto done;
		if( s < 0x100 )
		{
			m23_ring[m23_wpos] = (unsigned char)s;
			out[opos++] = (unsigned char)s;
			m23_wpos = (m23_wpos + 1) & 0xFFFF;
			// exactly one countdown step per literal; on a ring wrap
			// this substitutes for the step skipped by the wrap branch
			if( rlo == 0 ) { rlo = 0xFFFF; if( --rhi == 0 ) goto done; }
			else rlo--;
		}
		else
		{
			unsigned int src, mx;
			int k;
			sym = s;
			base = m23_decode_dist( &m, &sym );
			if( base < 0 ) goto done;
			len = sym - 0xFD;	// distance is base+1
			if( len < 3 || opos + (unsigned long)len > outsize ) goto done;
			// source starts (base+1) bytes back: si = di-1-base
			src = (unsigned int)((m23_wpos - 1 - (unsigned long)base) & 0xFFFF);
			// fast path iff neither source nor dest range wraps 64K
			mx = src > (unsigned int)m23_wpos ? src : (unsigned int)m23_wpos;
			if( ((unsigned long)mx + (unsigned long)len) & 0x10000UL )
			{
				for( k=0; k<len; k++ )
				{
					unsigned char b = m23_ring[src];
					m23_ring[m23_wpos] = b;
					out[opos++] = b;
					src = (src + 1) & 0xFFFF;
					m23_wpos = (m23_wpos + 1) & 0xFFFF;
				}
			}
			else
			{
				for( k=0; k<len; k++ )
				{
					unsigned char b = m23_ring[(src + k) & 0xFFFF];
					m23_ring[(m23_wpos + (unsigned long)k) & 0xFFFF] = b;
					out[opos++] = b;
				}
				m23_wpos = (m23_wpos + (unsigned long)len) & 0xFFFF;
			}
			// 32-bit remaining -= len; done iff exactly zero
			{
				unsigned long r = ((rhi << 16) | rlo) - (unsigned long)len;
				rhi = r >> 16;
				rlo = (unsigned int)(r & 0xFFFF);
				if( r == 0 ) goto done;
			}
		}
		if( opos >= outsize ) goto done;
	}

done:
	*produced = opos;
	free(inbuf);
	return out;
}

//==========================================================================//
// method 9: PPM range coder
//
// sub_404A90 (case 9) -> sub_406920 -> sub_405160 (main decode loop)
// sub_4054C0 (context setup) + sub_4057B0 (context search)
// sub_405D60 (range-decode one symbol or escape)
// sub_405840 (model update after a literal)
// sub_405A30 (allocate a new context node, LRU)
// sub_4060D0 (range-decoder rescaling) + sub_404710/sub_404DA0 (bit input)
// sub_406500 (model init, LABEL_9 path) + sub_40A9F0 (output flush)
//
// The trie init loops write every 4th int with stride 8/16, leaving zero
// gaps; that is replicated bit-exactly (both sides need the same).  The
// range coder keeps (lo,hi,val) 16-bit plus a 32-bit code buffer refilled
// 16 bits at a time, big-endian, 0xFF past end.  The hash function is
// h = b0; h = b1 ^ 4*h; ... (< 65536 always, no masking).  The exclusion
// flags live in ONE 258-byte array (661[v] == 660[v+1]).
//==========================================================================//

#define PPM_NCTX   11264
#define PPM_NPOOL  22528
#define PPM_NHASH  65536

// ---- model state (mirrors the original BSS layout) ----
static int			 hHead[PPM_NHASH];		// hash bucket heads
static int			 hNext[PPM_NCTX];		// hash chain links
static int			 nFreq[PPM_NPOOL];		// node frequencies
static int			 nChild[PPM_NPOOL];		// first child / free link
static unsigned char nSym[PPM_NPOOL];		// node symbol
static unsigned char nOrd[PPM_NPOOL];		// node order
static unsigned char nCtx[PPM_NCTX * 5];	// context bytes
static int			 mtfP[PPM_NCTX];		// LRU prev
static int			 mtfN[PPM_NCTX];		// LRU next
static unsigned char xFlag[258];			// exclusion flags
static unsigned char xList[260];			// excluded symbols
static int			 xCount;
static unsigned char hist[8];				// history bytes
static int			 oh[6];					// order hashes
static int			 oC4;
static int			 ctxN[10];				// nodes per level
static int			 ctxF[9];				// level flags
static int			 lvlT[8];				// level totals
static int			 escAcc;
static int			 lvlIdx;
static int			 freeHead;
static int			 clockHand;
static int			 maxOrd;
static int			 ord5cnt;
static int			 mtfHead, mtfTail;

// ---- range coder state ----
static unsigned int	 rc_code;
static int			 rc_bits;
static unsigned short rc_lo, rc_hi, rc_val;

// ---- input ----
static const unsigned char* ppm_in_base;
static unsigned long ppm_in_size, ppm_in_pos;
static const unsigned char* ppm_in_trail;
static unsigned long ppm_in_tsize, ppm_in_tpos;

static int ppm_in_byte( void )
{
	if( ppm_in_pos < ppm_in_size ) return ppm_in_base[ppm_in_pos++];
	if( ppm_in_tpos < ppm_in_tsize ) return ppm_in_trail[ppm_in_tpos++];
	return -1;
}

static unsigned ppm_in_word( void )
{
	int b0 = ppm_in_byte();
	int b1;
	if( b0 < 0 ) return 0xFFFFFFFFu;
	b1 = ppm_in_byte();
	return ((unsigned)b0 << 8) | (unsigned)(b1 & 0xFFFF);
}

// ---- output ----
static unsigned char* ppm_o_base;
static unsigned long  ppm_o_cap, ppm_o_len;
static int ppm_o_text;
static int ppm_o_fail;

static void ppm_o_put( unsigned char c )
{
	if( ppm_o_len >= ppm_o_cap ) { ppm_o_fail = 1; return; }
	ppm_o_base[ppm_o_len++] = c;
}

static unsigned char ppm_stageB[1024];
static int ppm_stageP;

static void ppm_o_flush( const unsigned char* src, int n )
{
	int i;
	if( ppm_o_text )
	{
		// a "text" member stores LF and the original writes CR LF
		for( i=0; i<n; i++ )
		{
			if( src[i] == 10 ) ppm_o_put( 13 );
			ppm_o_put( src[i] );
		}
	}
	else
	{
		for( i=0; i<n; i++ ) ppm_o_put( src[i] );
	}
}

// ---- bit input (sub_404710 / sub_404DA0) ----
static void ppm_rc_init( void )
{
	rc_code = 0;
	rc_bits = 0;
}

static void ppm_rc_refill( int n )
{
	unsigned w;
	int k;
	rc_code <<= n;
	rc_bits -= n;
	if( rc_bits >= 0 ) return;
	k = -rc_bits;
	if( ppm_in_pos + 2 <= ppm_in_size )
	{
		w = ((unsigned)ppm_in_base[ppm_in_pos] << 8) | ppm_in_base[ppm_in_pos + 1];
		ppm_in_pos += 2;
	}
	else
	{
		w = ppm_in_word();
	}
	rc_code |= w << k;
	rc_bits += 16;
}

// range-decoder rescale (sub_4060D0)
static void ppm_rc_decode( unsigned a1, unsigned a2, unsigned total )
{
	unsigned range = (unsigned)rc_hi - (unsigned)rc_lo + 1u;
	unsigned hi = (unsigned)(unsigned short)((unsigned)(rc_lo - 1) + a2 * range / total);
	unsigned lo = rc_lo + (unsigned)(unsigned short)(a1 * range / total);
	unsigned val = rc_val;
	for(;;)
	{
		if( ((hi ^ lo) & 0x8000u) == 0 ) goto norm;
		if( (lo & 0x4000u) == 0 || (hi & 0x4000u) != 0 ) break;
		lo &= 0x3FFFu;
		val ^= 0x4000u;
		hi |= 0x4000u;
norm:
		{
			unsigned carry = rc_code >> 31;
			hi = 2u * hi + 1u;
			lo *= 2u;
			val = (unsigned short)(2u * val);
			rc_val = (unsigned short)val;
			rc_code *= 2u;
			--rc_bits;
			if( rc_bits < 0 )
			{
				unsigned w;
				int k = -rc_bits;
				if( ppm_in_pos + 2 <= ppm_in_size )
				{
					w = ((unsigned)ppm_in_base[ppm_in_pos] << 8) | ppm_in_base[ppm_in_pos + 1];
					ppm_in_pos += 2;
				}
				else
				{
					w = ppm_in_word();
				}
				rc_code |= w << k;
				rc_bits += 16;
				val = rc_val;
			}
			val = (unsigned short)(val + carry);
			rc_val = (unsigned short)val;
		}
	}
	rc_hi = (unsigned short)hi;
	rc_lo = (unsigned short)lo;
}

// ---- model init (sub_406500, LABEL_9 path: always full init) ----
//
// The original builds these tables with SSE stores to overlapping views, so
// the net effect is DENSE tables (no gaps): consecutive MTF ramps and a
// consecutive free chain.  Treating the views as separate arrays leaves
// zero gaps, breaks the LRU victim order and desynchronises the model.
static void ppm_init( void )
{
	int v2, k;
	for( v2=0; v2<PPM_NHASH; v2++ ) hHead[v2] = -1;
	for( v2=0; v2<PPM_NCTX; v2++ ) hNext[v2] = -1;
	// trie nodes: dense ramp init (overlapping SSE stores combined)
	for( v2=0; v2<PPM_NCTX; v2+=8 )
	{
		int n;
		if( v2 + 8 > PPM_NCTX ) n = PPM_NCTX - v2; else n = 8;
		memset( &nOrd[v2], 0xFF, (size_t)n );
		for( k=0; k<8 && v2 + k < PPM_NCTX; k++ )
		{
			mtfN[v2 + k] = v2 + 1 + k;
			mtfP[v2 + k] = v2 - 1 + k;
			nChild[v2 + k] = -1;
		}
	}
	// free chain [NCTX, NPOOL): consecutive, last links to -1
	for( v2=PPM_NCTX; v2<PPM_NPOOL; v2++ )
		nChild[v2] = v2 + 1;
	nChild[PPM_NPOOL - 1] = -1;
	freeHead = PPM_NCTX;
	clockHand = 0;
	mtfHead = PPM_NCTX - 1;
	mtfTail = 0;
	memset( hist, 0, sizeof(hist) );
	memset( xFlag, 0, sizeof(xFlag) );
	xCount = 0;
	maxOrd = 5;
	ord5cnt = PPM_NCTX >> 2;
	escAcc = 0;
	oh[0] = 0;
}

// context search (sub_4057B0)
static int ppm_ctx_find( void )
{
	int v0 = oC4 - 1;
	while( v0 >= 0 )
	{
		int v1 = hHead[oh[v0]];
		if( v1 != -1 )
		{
			for(;;)
			{
				if( (int)nOrd[v1] != v0 ) goto nxt;
				if( v0 == 0 ) { oC4 = 0; return v1; }
				{
					int k;
					for( k=0; k<v0; k++ )
						if( hist[k] != nCtx[5 * v1 + k] ) break;
					if( k == v0 ) { oC4 = v0; return v1; }
				}
nxt:
				v1 = hNext[v1];
				if( v1 == -1 ) break;
			}
		}
		v0--;
	}
	return -1;
}

// context setup (sub_4054C0)
static int ppm_ctx_setup( void )
{
	int v0;
	oh[1] = hist[0];
	lvlIdx = 0;
	oh[2] = hist[1] ^ (4 * oh[1]);
	oh[3] = hist[2] ^ (4 * oh[2]);
	oh[4] = hist[3] ^ (4 * oh[3]);
	oh[5] = hist[4] ^ (4 * oh[4]);
	for( v0=xCount-1; v0>=0; v0-- ) xFlag[xList[v0]] = 0;
	xCount = 0;
	oC4 = 6;
	return ppm_ctx_find();
}

// range-decode one symbol in a context (sub_405D60).  256 = escape.
static int ppm_decode_sym( int a1 )
{
	int v2 = 0, v29 = 0, v3 = 0, v4 = 0;
	int v31 = a1, v5 = a1;
	int v7, v27;
	unsigned tot, total30;
	if( a1 != -1 )
	{
		do
		{
			unsigned f;
			++v2;
			f = (unsigned)nFreq[v5];
			if( xFlag[nSym[v5]] ) v29 += (int)f;
			else { v3 += (int)f; if( f < 4 ) ++v4; }
			v5 = nChild[v5];
		} while( v5 != -1 );
		a1 = v31;
	}
	v7 = a1;
	if( v3 <= 0x1000 ) v7 = -1;
	tot = (unsigned)v29 + (unsigned)v3;
	lvlT[lvlIdx] = (int)tot;
	v27 = v7;
	if( tot == 1 )
	{
		v4 = (int)tot;
	}
	else if( v2 > 1 )
	{
		if( v2 == 256 )
		{
			v4 = 1;
		}
		else if( 2 * v2 >= (int)tot )
		{
			v4 = (int)((unsigned)(2 * v2) * (unsigned)v4 / tot);
			if( (int)tot == v2 ) v4 += v2 >> 2;
			if( !v4 ) v4 = 1;
		}
		else if( !v4 )
		{
			v4 = 1;
		}
	}
	else if( !v4 )
	{
		v4 = 1;
	}
	if( (unsigned)escAcc < 4u )
	{
		// unscaled branch
		unsigned thr;
		unsigned short t16;
		int v9, v13, v14;
		total30 = (unsigned)v4 + (unsigned)v3;
		thr = (unsigned)(((total30 * ((unsigned)rc_val - (unsigned)rc_lo + 1u)) - 1u)
						/ ((unsigned)rc_hi - (unsigned)rc_lo + 1u));
		t16 = (unsigned short)thr;
		v9 = v31;
		v13 = 0;
		if( v31 != -1 )
		{
			for(;;)
			{
				if( !xFlag[nSym[v9]] )
				{
					if( (unsigned)(v13 + nFreq[v9]) > (unsigned)t16 ) break;
					v13 += nFreq[v9];
				}
				v9 = nChild[v9];
				if( v9 == -1 ) break;
			}
		}
		v14 = nFreq[v9];
		if( v9 == -1 || !v14 ) goto escape;
		ppm_rc_decode( (unsigned)v13, (unsigned)(v13 + v14), total30 );
		{
			int v16 = lvlIdx;
			int v17 = escAcc + 1;
			ctxN[v16 + 1] = v9;
			ctxF[v16] = v31;
			lvlIdx = v16 + 1;
			{
				int v24 = v27, v25;
				escAcc = v17;
				while( v24 != -1 )
				{
					v25 = nFreq[v24];
					if( v25 > 1 ) nFreq[v24] = v25 >> 1;
					v24 = nChild[v24];
				}
				return (int)nSym[v9];
			}
		}
	}
	else
	{
		// scaled branch
		int v28;
		unsigned thr;
		unsigned short t16;
		int v9, v12, v13, v14;
		v28 = (v3 > 6) ? 1 : 2;
		total30 = (unsigned)v4 + ((unsigned)v3 << v28);
		thr = (unsigned)(((total30 * ((unsigned)rc_val - (unsigned)rc_lo + 1u)) - 1u)
						/ ((unsigned)rc_hi - (unsigned)rc_lo + 1u));
		t16 = (unsigned short)thr;
		v9 = v31;
		v12 = 0;
		if( v31 != -1 )
		{
			for(;;)
			{
				if( !xFlag[nSym[v9]] )
				{
					if( (((unsigned)(v12 + nFreq[v9])) << v28) > (unsigned)t16 ) break;
					v12 += nFreq[v9];
				}
				v9 = nChild[v9];
				if( v9 == -1 ) break;
			}
		}
		v13 = v12 << v28;
		v14 = (int)((unsigned)nFreq[v9] << v28);
		if( v9 == -1 || !v14 ) goto escape;
		ppm_rc_decode( (unsigned)v13, (unsigned)(v13 + v14), total30 );
		{
			int v16 = lvlIdx;
			int v17 = escAcc + 1;
			ctxN[v16 + 1] = v9;
			ctxF[v16] = v31;
			lvlIdx = v16 + 1;
			{
				int v24 = v27, v25;
				escAcc = v17;
				while( v24 != -1 )
				{
					v25 = nFreq[v24];
					if( v25 > 1 ) nFreq[v24] = v25 >> 1;
					v24 = nChild[v24];
				}
				return (int)nSym[v9];
			}
		}
	}
escape:
	{
		int v20 = v31, v21, v22, v23 = lvlIdx;
		unsigned e2 = total30 - (unsigned)v4;
		ppm_rc_decode( e2, total30, total30 );
		v21 = xCount;
		for(;;)
		{
			v22 = (int)nSym[v20];
			if( !xFlag[v22] )
			{
				xList[v21++] = (unsigned char)v22;
				xFlag[v22] = 1;
			}
			if( nChild[v20] == -1 ) break;
			v20 = nChild[v20];
		}
		xCount = v21;
		ctxN[v23 + 1] = v20;
		ctxF[v23] = ~v31;
		lvlIdx = v23 + 1;
		escAcc = 0;
		{
			int v24 = v27, v25;
			while( v24 != -1 )
			{
				v25 = nFreq[v24];
				if( v25 > 1 ) nFreq[v24] = v25 >> 1;
				v24 = nChild[v24];
			}
			return 256;
		}
	}
}

// model update after a literal (sub_405840)
static void ppm_model_update( int lit )
{
	int v1 = lvlIdx;
	if( v1 )
	{
		int v2 = freeHead, v3 = clockHand, v21 = v2;
		do
		{
			int v4 = ctxN[v1--];
			int v20 = v4;
			lvlIdx = v1;
			if( ctxF[v1] < 0 )
			{
				if( v2 == -1 )
				{
					// free list empty: clock sweep + rescale
					for(;;)
					{
						int v5 = 0, v6, v7;
						do
						{
							++v3;
							if( v3 == PPM_NCTX ) v3 = 0;
							v6 = nChild[v3];
						} while( v6 == -1 );
						for(;;)
						{
							v7 = ctxF[v5];
							if( v7 < 0 ) v7 = ~v7;
							if( v7 == v3 ) break;
							if( ++v5 > v1 ) goto rescale;
						}
					}
rescale:
					{
						int v8, v9 = 0, v10 = v3, v11, v12, v13, v14, v15, i, v6;
						clockHand = v3;
						v8 = nFreq[v3];
						v6 = nChild[v3];
						while( v6 != -1 )
						{
							int f9 = nFreq[v6];
							v6 = nChild[v6];
							if( v8 > f9 ) f9 = v8;
							v8 = f9;
						}
						v9 = v8;
						v10 = v3;
						if( v3 != -1 )
						{
							v11 = v9 + 1;
							do
							{
								int v19 = v10;
								if( nChild[v3] == -1 ) break;
								v12 = v10;
								v13 = nFreq[v10];
								if( v13 >= v11 )
								{
									v10 = nChild[v10];
									nFreq[v12] = (int)((unsigned)v13 / (unsigned)v11);
								}
								else if( v10 == v3 )
								{
									v14 = nChild[v10];
									nFreq[v10] = nFreq[v14];
									nSym[v10] = nSym[v14];
									nChild[v10] = nChild[v14];
									nChild[v14] = v21;
									v21 = v14;
								}
								else
								{
									v15 = v3;
									for( i = nChild[v3]; i != v10; i = nChild[i] )
										v15 = i;
									v10 = nChild[v10];
									nChild[v15] = v10;
									nChild[v12] = v21;
									v21 = v19;
								}
							} while( v10 != -1 );
							v1 = lvlIdx;
						}
						v4 = v20;
						v2 = v21;
					}
				}
				nChild[v4] = v2;
				{
					int v17 = v2;
					v4 = v2;
					nFreq[v17] = 0;
					v2 = nChild[v2];
					nChild[v17] = -1;
					v21 = v2;
					freeHead = v2;
					nSym[v4] = (unsigned char)lit;
				}
			}
			++nFreq[v4];
		} while( v1 );
	}
}

// create a new context node, LRU slot reuse (sub_405A30)
static void ppm_node_create( int order, int sym )
{
	int v2 = mtfHead;
	int v3 = order;
	unsigned char v4 = nOrd[mtfHead];
	int v6;
	mtfHead = mtfP[mtfHead];
	{
		int v5 = mtfTail;
		mtfN[v2] = mtfTail;
		mtfTail = v2;
		mtfP[v5] = v2;
	}
	if( v4 == 0xFF ) goto fill;
	if( v4 == 5 )
	{
		if( !--ord5cnt ) maxOrd = 4;
	}
	if( v4 )
	{
		v6 = nCtx[5 * v2];
		if( v4 > 1u )
		{
			v6 = nCtx[5 * v2 + 1] ^ (4 * v6);
			if( v4 > 2u )
			{
				v6 = nCtx[5 * v2 + 2] ^ (4 * v6);
				if( v4 > 3u )
				{
					v6 = nCtx[5 * v2 + 3] ^ (4 * v6);
					if( v4 > 4u )
						v6 = nCtx[5 * v2 + 4] ^ (4 * v6);
				}
			}
		}
	}
	else
	{
		v6 = 0;
	}
	{
		int v7 = hHead[v6];
		if( v7 == v2 )
		{
			hHead[v6] = hNext[v2];
		}
		else
		{
			int i;
			for( i = hNext[v7]; i != v2; i = hNext[i] )
				v7 = i;
			hNext[v7] = hNext[v2];
		}
	}
	{
		int v9 = nChild[v2];
		if( v9 != -1 )
		{
			int v10 = freeHead, v11;
			do
			{
				v11 = nChild[v9];
				nChild[v9] = v10;
				v10 = v9;
				v9 = v11;
			} while( v11 != -1 );
			freeHead = v10;
		}
	}
fill:
	nSym[v2] = (unsigned char)sym;
	nChild[v2] = -1;
	nFreq[v2] = 1;
	nOrd[v2] = (unsigned char)v3;
	nCtx[5 * v2] = hist[0];
	nCtx[5 * v2 + 1] = hist[1];
	nCtx[5 * v2 + 2] = hist[2];
	nCtx[5 * v2 + 3] = hist[3];
	nCtx[5 * v2 + 4] = hist[4];
	if( v3 )
	{
		int v18 = hist[0], v17;
		if( v3 > 1 )
		{
			v18 = hist[1] ^ (4 * v18);
			if( v3 > 2 )
			{
				v18 = hist[2] ^ (4 * v18);
				if( v3 > 3 )
				{
					v18 = hist[3] ^ (4 * v18);
					if( v3 > 4 )
						v18 = hist[4] ^ (4 * v18);
				}
			}
		}
		v17 = v18;
		{
			int v19 = hHead[v17];
			hHead[v17] = v2;
			hNext[v2] = v19;
		}
	}
	else
	{
		int v19 = hHead[0];
		hHead[0] = v2;
		hNext[v2] = v19;
	}
}

// main decode loop (sub_405160)
unsigned char* BsaDecodeMethod9( const unsigned char* packed, unsigned long psize,
								 const unsigned char* trail, unsigned long tsize,
								 unsigned long outsize, unsigned long* produced,
								 int textmode )
{
	unsigned char* out;
	int i;

	*produced = 0;
	ppm_in_base = packed; ppm_in_size = psize; ppm_in_pos = 0;
	ppm_in_trail = trail ? trail : (const unsigned char*)"";
	ppm_in_tsize = trail ? tsize : 0; ppm_in_tpos = 0;
	(void)ppm_in_byte();	// first stream byte: version flag, ignored

	out = (unsigned char*)malloc( outsize ? outsize : 1 );
	if( !out ) return 0;
	ppm_o_base = out;
	ppm_o_cap = outsize; ppm_o_len = 0; ppm_o_text = textmode; ppm_o_fail = 0;
	ppm_stageP = 0;

	ppm_init();
	ppm_rc_init();
	ppm_rc_refill( 16 );
	rc_lo = 0;
	rc_hi = 0xFFFF;
	rc_val = 0;
	for( i=0; i<16; i++ )
	{
		unsigned b = rc_code >> 31;
		rc_val = (unsigned short)(rc_val * 2u);
		ppm_rc_refill( 1 );
		rc_val = (unsigned short)(rc_val + b);
	}

	for(;;)
	{
		int v6 = ppm_ctx_setup();
		int v17, v18, sym;
		if( v6 == -1 )
		{
			v17 = 0;
			v18 = maxOrd + 1;
			goto order1;
		}
		v17 = (int)nOrd[v6] + 1;
		v18 = maxOrd + 1;
		for(;;)
		{
			sym = ppm_decode_sym( v6 );
			if( sym != 256 ) break;
			v6 = ppm_ctx_find();
			if( v6 == -1 ) goto order1;
		}
		// move to front of the LRU
		{
			int v10 = mtfTail;
			if( v6 != v10 )
			{
				if( v6 == mtfHead )
				{
					mtfHead = mtfP[v6];
				}
				else
				{
					int v11 = mtfN[v6];
					mtfP[v11] = mtfP[v6];
					mtfN[mtfP[v6]] = v11;
				}
				mtfP[v10] = v6;
				mtfN[v6] = v10;
				mtfTail = v6;
			}
		}
		goto emit;
order1:
		{
			// order -1 literal (LABEL_12): unrolled 4-wide exclusion scan
			unsigned total = (unsigned)(0x101 - xCount);
			unsigned thr = (unsigned)(((total * ((unsigned)rc_val - (unsigned)rc_lo + 1u)) - 1u)
									/ ((unsigned)rc_hi - (unsigned)rc_lo + 1u));
			unsigned short t16 = (unsigned short)thr;
			unsigned v8 = 0;
			int v7 = 0;
			for(;;)
			{
				if( !xFlag[v7] )
				{
					if( v8 >= t16 ) break;
					++v8;
				}
				if( !xFlag[v7 + 1] )
				{
					if( v8 >= t16 ) { v7 += 1; break; }
					++v8;
				}
				if( !xFlag[v7 + 2] )
				{
					if( v8 >= t16 ) { v7 += 2; break; }
					++v8;
				}
				if( !xFlag[v7 + 3] )
				{
					if( v8 >= t16 ) { v7 += 3; break; }
					++v8;
				}
				v7 += 4;
				if( v7 >= 0x100 ) break;
			}
			ppm_rc_decode( v8, v8 + 1, total );
			sym = v7;
		}
emit:
		if( sym == 256 ) break;
		ppm_model_update( sym );
		if( v18 > v17 )
		{
			int v12 = v18, v13 = v18 - v17;
			do
			{
				ppm_node_create( --v12, sym );
			} while( --v13 );
		}
		ppm_stageB[ppm_stageP++] = (unsigned char)sym;
		if( ppm_stageP >= 1024 )
		{
			ppm_o_flush( ppm_stageB, ppm_stageP );
			if( ppm_o_fail ) { free(out); return 0; }
			ppm_stageP = 0;
		}
		// history shift
		hist[4] = hist[3]; hist[3] = hist[2]; hist[2] = hist[1];
		hist[1] = hist[0]; hist[0] = (unsigned char)sym;
	}
	if( ppm_stageP )
	{
		ppm_o_flush( ppm_stageB, ppm_stageP );
		if( ppm_o_fail ) { free(out); return 0; }
		ppm_stageP = 0;
	}
	*produced = ppm_o_len;
	return out;
}

//==========================================================================//

void BsaCodecReset( void )
{
	// The two sliding windows are persistent across the entries of one
	// archive (the original never clears them), so a second archive
	// decoded in the same process must start from a clean window or it
	// would match against the previous archive's bytes.
	memset( lz_ring, 0, sizeof(lz_ring) );
	lz_wpos = 0;
	memset( m23_ring, 0, sizeof(m23_ring) );
	m23_wpos = 0;
}
