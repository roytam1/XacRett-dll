#include "stdafx.h"
#include "EggAzo.h"
#include <stdlib.h>
#include <string.h>

//-- AZO decode -----------------------
//
// Decode-only port of the reference (alzgg/src/egg_azo.c).  See the header
// for the stream layout.  The arithmetic coder below is the "carry-less"
// range coder of appendix A: low/up/tag are 32-bit quantities, the range is
// halved while both ends share the top bit, and the second loop handles the
// underflow case.

struct AzoBits
{
	const BYTE* p;
	DWORD       size;
	DWORD       pos;    // bytes consumed
	unsigned    cur;    // current byte, MSB first
	unsigned    bits;   // bits left in cur
	unsigned    over;   // reads past the end (zero padding)
};

static unsigned AzoReadBit( AzoBits* b )
{
	if( !b->bits )
	{
		if( b->pos < b->size )
			b->cur = b->p[b->pos++];
		else
		{
			b->cur = 0;         // end-of-stream padding
			b->over++;
		}
		b->bits = 8;
	}
	b->bits--;
	return (b->cur >> b->bits) & 1u;
}

#define AZO_MASK 0xFFFFFFFFu
#define AZO_MSB  0x80000000u
#define AZO_SMSB 0x40000000u

struct AzoDec
{
	AzoBits  in;
	unsigned __int64 low, up, tag;
};

static void AzoInit( AzoDec* d, const BYTE* src, DWORD size )
{
	int i;

	d->in.p = src;
	d->in.size = size;
	d->in.pos = 0;
	d->in.cur = 0;
	d->in.bits = 0;
	d->in.over = 0;
	d->low = 0;
	d->up = AZO_MASK;
	d->tag = 0;
	for( i = 0; i < 32; i++ )       // 32 bits of priming, MSB first
		d->tag = ((d->tag << 1) | AzoReadBit(&d->in)) & AZO_MASK;
}

static void AzoRescale( AzoDec* d )
{
	while( (d->low & AZO_MSB) == (d->up & AZO_MSB) )
	{
		unsigned bit = AzoReadBit(&d->in);

		d->tag = ((d->tag << 1) & AZO_MASK) | bit;
		d->low = (d->low << 1) & AZO_MASK;
		d->up  = ((d->up << 1) & AZO_MASK) | 1u;
	}
	while( (d->low & AZO_SMSB) && !(d->up & AZO_SMSB) )
	{
		unsigned bit = AzoReadBit(&d->in);

		d->tag = (((d->tag << 1) | bit) ^ AZO_MSB) & AZO_MASK;
		d->low = (d->low << 1) & (AZO_MASK >> 1);
		d->up  = ((d->up << 1) | 1u | AZO_MSB) & AZO_MASK;
	}
}

// cum = probability of 0 over total_bit bits (4096 for AzoBool,
// 1024 for AzoEbp).  Returns the symbol.
static unsigned AzoDecodeBool( AzoDec* d, DWORD cum, int total_bit )
{
	unsigned __int64 t = ((d->up - d->low) + 1) >> total_bit;
	unsigned __int64 v = (d->tag - d->low) / t;

	if( v >= cum )
	{
		d->low += t * cum;
		AzoRescale( d );
		return 1;
	}
	d->up = d->low + t * cum - 1;
	AzoRescale( d );
	return 0;
}

static DWORD AzoDecodeUniform( AzoDec* d, int total_bit )
{
	unsigned __int64 t = ((d->up - d->low) + 1) >> total_bit;
	unsigned __int64 v = (d->tag - d->low) / t;

	d->up  = d->low + t * (v + 1) - 1;
	d->low = d->low + t * v;
	AzoRescale( d );
	return (DWORD)v;
}

//-- probability models -----------------------

#define AZO_NODES 256           // 2^8: the largest alphabet used

struct AzoEbp                    // EntropyBitProb<N>
{
	int  bit_n;
	DWORD prob[AZO_NODES];
};

static void EbpInit( AzoEbp* m, int bit_n )
{
	int i;

	m->bit_n = bit_n;
	for( i = 0; i < (1 << bit_n); i++ )
		m->prob[i] = 512;       // TOTAL_COUNT / 2, TOTAL_COUNT = 1024
}

static unsigned EbpDecode( AzoDec* d, AzoEbp* m )
{
	unsigned value = 0, pre = 1;
	int i;

	for( i = m->bit_n - 1; i >= 0; i-- )
	{
		unsigned bit = AzoDecodeBool( d, m->prob[pre], 10 );

		if( bit ) m->prob[pre] -= m->prob[pre] >> 4;
		else      m->prob[pre] += (1024u - m->prob[pre]) >> 4;
		if( bit ) value |= 1u << i;
		pre = (pre << 1) | bit;
	}
	return value;
}

// The same update without reading the stream, for the other model of a
// PredictProb pair.
static void EbpUpdate( AzoEbp* m, unsigned value )
{
	unsigned pre = 1;
	int i;

	for( i = m->bit_n - 1; i >= 0; i-- )
	{
		unsigned bit = (value >> i) & 1u;

		if( bit ) m->prob[pre] -= m->prob[pre] >> 4;
		else      m->prob[pre] += (1024u - m->prob[pre]) >> 4;
		pre = (pre << 1) | bit;
	}
}

struct AzoBool                   // BoolState<8>
{
	DWORD    prob[256];
	unsigned state;
};

static void AbInit( AzoBool* b )
{
	int i;

	for( i = 0; i < 256; i++ )
		b->prob[i] = 2048;      // TOTAL_COUNT / 2, TOTAL_COUNT = 4096
	b->state = 0;
}

static unsigned AbDecode( AzoDec* d, AzoBool* b )
{
	unsigned bit = AzoDecodeBool( d, b->prob[b->state], 12 );

	if( bit ) b->prob[b->state] -= b->prob[b->state] >> 6;
	else      b->prob[b->state] += (4096u - b->prob[b->state]) >> 6;
	b->state = ((b->state << 1) | bit) & 0xFF;
	return bit;
}

//-- PredictProb: fine and coarse models plus a luck counter ------------------

struct AzoPp
{
	AzoEbp* fine;           // one model per context
	AzoEbp* coarse;         // one per group of contexts
	int*    lucky;
};

static unsigned PpDecode( AzoDec* d, AzoPp* pp, int pre, int bit_n, int shift )
{
	AzoEbp*  m1 = &pp->fine[pre];
	AzoEbp*  m2 = &pp->coarse[pre >> shift];
	unsigned value;
	DWORD    prod1 = 1, prod2 = 1;
	unsigned node = 1;
	int      i, r;

	if( pp->lucky[pre] >= 0 )
	{
		value = EbpDecode( d, m1 );
		EbpUpdate( m2, value );
	}
	else
	{
		value = EbpDecode( d, m2 );
		EbpUpdate( m1, value );
	}

	// A.7: compare the product of the probabilities along the decoded path.
	for( i = bit_n - 1; i >= 0; i-- )
	{
		DWORD p1 = m1->prob[node];
		DWORD p2 = m2->prob[node];
		unsigned v = (value >> i) & 1u;

		if( (prod1 | prod2) & ((1024u - 1u) << (32 - 10)) )
		{
			prod1 >>= 10;
			prod2 >>= 10;
		}
		prod1 *= v ? (1024u - p1) : p1;
		prod2 *= v ? (1024u - p2) : p2;
		node = (node << 1) | v;
	}
	r = (prod1 > prod2) - (prod1 < prod2);
	if( r > 0 )      pp->lucky[pre]++;
	else if( r < 0 ) pp->lucky[pre]--;
	return value;
}

//-- MRU lists (A.8) -----------------------

struct AzoHist
{
	int      n;
	DWORD*   rep;
};

// The length, distance and match lists are seeded with init, init+1, ...;
// the position list starts at zero.
static void HistInit( AzoHist* h, DWORD* mem, int n, DWORD init, int step )
{
	int i;

	h->n = n;
	h->rep = mem;
	for( i = 0; i < n; i++ )
		h->rep[i] = step ? init + (DWORD)i : init;
}

static DWORD HistTake( AzoHist* h, int idx )
{
	DWORD v = h->rep[idx];
	int i;

	for( i = idx; i > 0; i-- )
		h->rep[i] = h->rep[i - 1];
	h->rep[0] = v;
	return v;
}

static void HistAdd( AzoHist* h, DWORD v )
{
	int i;

	for( i = h->n - 1; i > 0; i-- )
		h->rep[i] = h->rep[i - 1];
	h->rep[0] = v;
}

//-- code tables -----------------------

static DWORD AzoLenBase[128];
static int   AzoLenExtra[128];
static DWORD AzoDistBase[128];
static int   AzoDistExtra[128];
static int   AzoTablesReady;

static int AzoIlog2( DWORD v )
{
	int n = 0;

	while( v >>= 1 )
		n++;
	return n;
}

static void AzoBuildTables( void )
{
	int i;

	if( AzoTablesReady )
		return;
	for( i = 0; i < 128; i++ )
	{
		AzoLenExtra[i] = i < 32 ? 0 : (i - 32) / 8;
		AzoDistExtra[i] = i < 16 ? 0 : (i - 16) / 4;
	}
	AzoLenBase[0] = 2;
	AzoDistBase[0] = 1;
	for( i = 1; i < 128; i++ )
	{
		AzoLenBase[i] = AzoLenBase[i - 1] + (1u << AzoLenExtra[i - 1]);
		AzoDistBase[i] = AzoDistBase[i - 1] + (1u << AzoDistExtra[i - 1]);
	}
	AzoTablesReady = 1;
}

// A.11.2: distance value to code index (the length model's context).
static unsigned AzoDistCode( DWORD value )
{
	DWORD v = value - 1;
	DWORD extra;

	if( v < 16 )
		return v;
	v -= 16;
	extra = (DWORD)AzoIlog2( v / 4 + 1 );
	return 16 + extra * 4 + (v - ((1u << extra) - 1) * 4) / (1u << extra);
}

//-- x86 filter -----------------------
//
// AZO.dll's filter (0x619094b0).  forward = encoder direction (relative to
// absolute), zero = decoder direction (absolute to relative).  Both ends of
// the call target are tested (0xE8/0xE9 and a high byte of 0x00 or 0xFF),
// and a hit always steps 5 bytes.
static void AzoX86Filter( BYTE* buf, DWORD size, int forward )
{
	DWORD i = 0;

	if( size < 5 )
		return;
	while( i < size - 4 )
	{
		if( buf[i] == 0xE8 || buf[i] == 0xE9 )
		{
			if( buf[i + 4] == 0x00 || buf[i + 4] == 0xFF )
			{
				DWORD addr = (DWORD)buf[i + 1] | ((DWORD)buf[i + 2] << 8) |
				             ((DWORD)buf[i + 3] << 16) | ((DWORD)buf[i + 4] << 24);

				addr = forward ? addr + i : addr - i;
				if( (addr >> 24) & 1 )
					addr |= 0xFF000000u;
				else
					addr &= 0x00FFFFFFu;
				buf[i + 1] = (BYTE)addr;
				buf[i + 2] = (BYTE)(addr >> 8);
				buf[i + 3] = (BYTE)(addr >> 16);
				buf[i + 4] = (BYTE)(addr >> 24);
			}
			i += 5;
		}
		else
			i += 1;
	}
}

//-- one slice -----------------------

static bool AzoDecodeBlock( const BYTE* comp, DWORD csize, BYTE* out,
                            DWORD usize )
{
	AzoDec  d;
	AzoBool matchFlag, reuseFlag, shortReuse, shortDist;
	AzoEbp  idxShort, idxLong, distShort, distLong;
	AzoPp   alpha, lengths;
	AzoEbp* alpha_fine = NULL; AzoEbp* alpha_coarse = NULL;
	AzoEbp* len_fine = NULL;   AzoEbp* len_coarse = NULL;
	int*    alpha_lucky = NULL; int* len_lucky = NULL;
	DWORD   len_rep[128], pos_rep[128], dist_rep[2], match_rep[2];
	AzoHist lenHist, posHist, recentDist, recentMatch;
	DWORD   i = 0;
	DWORD   k;
	bool    ok = false;

	AzoBuildTables();
	AzoInit( &d, comp, csize );

	// Four distinct boolean models: sharing any of them diverges at the
	// first misplaced byte, which is how the reference established it.
	AbInit( &matchFlag );
	AbInit( &reuseFlag );
	AbInit( &shortReuse );
	AbInit( &shortDist );
	EbpInit( &idxShort, 1 );
	EbpInit( &idxLong, 7 );
	EbpInit( &distShort, 1 );
	EbpInit( &distLong, 7 );

	alpha_fine = (AzoEbp*)calloc( 256, sizeof(AzoEbp) );
	alpha_coarse = (AzoEbp*)calloc( 8, sizeof(AzoEbp) );
	alpha_lucky = (int*)calloc( 256, sizeof(int) );
	len_fine = (AzoEbp*)calloc( 128, sizeof(AzoEbp) );
	len_coarse = (AzoEbp*)calloc( 8, sizeof(AzoEbp) );
	len_lucky = (int*)calloc( 128, sizeof(int) );
	if( !alpha_fine || !alpha_coarse || !alpha_lucky ||
	    !len_fine || !len_coarse || !len_lucky )
		goto done;
	for( k = 0; k < 256; k++ ) EbpInit( &alpha_fine[k], 8 );
	for( k = 0; k < 8; k++ )   EbpInit( &alpha_coarse[k], 8 );
	for( k = 0; k < 128; k++ ) EbpInit( &len_fine[k], 7 );
	for( k = 0; k < 8; k++ )   EbpInit( &len_coarse[k], 7 );
	alpha.fine = alpha_fine;
	alpha.coarse = alpha_coarse;
	alpha.lucky = alpha_lucky;
	lengths.fine = len_fine;
	lengths.coarse = len_coarse;
	lengths.lucky = len_lucky;

	HistInit( &lenHist, len_rep, 128, 2, 1 );
	HistInit( &posHist, pos_rep, 128, 0, 0 );
	HistInit( &recentDist, dist_rep, 2, 1, 1 );
	HistInit( &recentMatch, match_rep, 2, 0, 1 );

	if( usize == 0 )
	{
		ok = true;
		goto done;
	}

	out[0] = (BYTE)PpDecode( &d, &alpha, 0, 8, 5 );
	i = 1;
	while( i < usize )
	{
		DWORD distance, length;

		if( !AbDecode( &d, &matchFlag ) )
		{
			out[i] = (BYTE)PpDecode( &d, &alpha, out[i - 1], 8, 5 );
			i++;
			continue;
		}

		if( AbDecode( &d, &reuseFlag ) )
		{
			unsigned hidx;

			if( AbDecode( &d, &shortReuse ) )
				hidx = HistTake( &recentMatch, (int)EbpDecode(&d, &idxShort) );
			else
			{
				hidx = EbpDecode( &d, &idxLong );
				HistAdd( &recentMatch, hidx );
			}
			length = HistTake( &lenHist, (int)hidx );
			distance = i - HistTake( &posHist, (int)hidx );
		}
		else
		{
			if( AbDecode( &d, &shortDist ) )
			{
				distance = HistTake( &recentDist,
				                     (int)EbpDecode(&d, &distShort) );
			}
			else
			{
				unsigned code = EbpDecode( &d, &distLong );

				distance = AzoDistBase[code];
				if( AzoDistExtra[code] )
					distance += AzoDecodeUniform( &d, AzoDistExtra[code] );
				HistAdd( &recentDist, distance );
			}
			if( distance == 0 || distance > i )
				goto done;
			{
				unsigned code = AzoDistCode( distance );
				unsigned lcode = PpDecode( &d, &lengths, (int)code, 7, 4 );

				length = AzoLenBase[lcode];
				if( AzoLenExtra[lcode] )
					length += AzoDecodeUniform( &d, AzoLenExtra[lcode] );
				HistAdd( &lenHist, length );
				HistAdd( &posHist, i );
			}
		}

		if( distance == 0 || distance > i )
			goto done;
		if( length > usize - i )
			length = usize - i;     // truncated; the size check catches it
		while( length-- )
		{
			out[i] = out[i - distance];
			i++;
		}
	}

	ok = true;

done:
	free( alpha_fine );
	free( alpha_coarse );
	free( alpha_lucky );
	free( len_fine );
	free( len_coarse );
	free( len_lucky );
	return ok;
}

//-- stream -----------------------

static DWORD AzoBe32( const BYTE* p )
{
	return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) |
	       ((DWORD)p[2] << 8) | (DWORD)p[3];
}

bool EggAzoDecode( const BYTE* src, DWORD csize, BYTE* dst, DWORD usize )
{
	DWORD pos = 0, out = 0;
	unsigned flags;

	if( csize < 2 )
		return false;
	if( src[0] != '1' )
		return false;
	flags = src[1];
	pos = 2;

	for(;;)
	{
		DWORD bus, bcs, chk;

		if( pos + 12 > csize )
			return false;
		bus = AzoBe32( src + pos );
		bcs = AzoBe32( src + pos + 4 );
		chk = AzoBe32( src + pos + 8 );
		pos += 12;

		if( !bus && !bcs )
			break;                  // terminal slice
		if( (bus ^ bcs) != chk )
			return false;
		if( bus > usize - out )
			return false;
		if( bcs > csize - pos )
			return false;

		if( bcs == bus )
			memcpy( dst + out, src + pos, bus );
		else if( !AzoDecodeBlock( src + pos, bcs, dst + out, bus ) )
			return false;
		pos += bcs;
		out += bus;
		if( flags & 1 )
			AzoX86Filter( dst + out - bus, bus, 0 );
	}

	return out == usize;
}
