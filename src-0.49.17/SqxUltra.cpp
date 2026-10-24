/* SqxUltra.cpp -- SQX method 7 (Ultra) decode glue.
 *
 * Method 7 ("Ultra", on-disk 0x07, SQX 2.0 only) is an LZ77 scheme driven
 * by a probabilistic range coder: binary is_match/is_rep models, 9-group
 * length tables, 50-symbol distance tables and nibble literal models.
 *
 * Ported from Squeez ultra.c (MIT, (c) 2026 seb3773), reverse-engineered
 * from UnSqueez.exe sub_40D8B1 (main loop), sub_40D03E (model setup),
 * sub_40CF40 (reset), sub_4097FA (binary), sub_40A05E (Fenwick),
 * sub_4099AB (literal) and sub_409BA3/sub_409C2A/sub_409C92 (range core,
 * TOP=0x1000000, BOT=0x10000), with tables dword_420B70/420C38 (distance
 * base/ebits, 50 entries) and dword_420D00 (length base/next/alphabet/
 * incr/max, 9 groups).
 *
 * The port keeps the algorithm (tables, update rules, state machine)
 * identical and streams through the unsqx output path instead of
 * buffering the member: the word-swapped bit reader and the TOP/BOT
 * range core are the SqxPpmd ones, and output goes through WriteOutBuf
 * so CRC/progress/volumes behave like every other method. Integer types
 * are the house UWORD32 (all counts fit: sizes above 0x7FFFFFFF are
 * refused up front, like SqxUnPpmd); declarations are C89-style so VC6
 * and MSVC 4.x both accept the TU.
 */
#include "stdafx.h"
#include "SqxUltra.h"
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "unsqx/cx_type.h"
#include "unsqx/sqx_h.h"
#include "unsqx/sqx_vars.h"
#include "unsqx/arcio.h"
#include "unsqx/cx_err.h"

extern void SetProgress(int iFinished);
}

/* ---- tables (dword_420D00 / dword_420B70 / dword_420C38) ---- */
static const UWORD32 UL_LT_BASE[9] = { 2, 3, 4, 6, 10, 18, 34, 66, 130 };
static const UWORD32 UL_LT_NEXT[9] = { 0, 0, 2, 3, 4, 5, 6, 7, 8 };
static const UWORD32 UL_LT_NSYM[9] = { 0, 0, 2, 4, 8, 16, 32, 64, 128 };
static const UWORD32 UL_LT_INCR[9] = { 0, 0, 1, 1, 2, 2, 2, 4, 4 };
static const UWORD32 UL_LT_MAX[9]  = { 0, 0, 255, 255, 255, 255, 255, 255, 255 };
static const UWORD32 UL_SEC_CUM[9] = { 0, 0, 0, 2, 6, 14, 30, 62, 126 };

static const UWORD32 UL_DT_BASE[50] = {
	0, 1, 2, 3, 4, 6, 8, 0xC, 0x10, 0x18, 0x20, 0x30, 0x40, 0x60,
	0x80, 0xC0, 0x100, 0x180, 0x200, 0x300, 0x400, 0x600, 0x800, 0xC00,
	0x1000, 0x1800, 0x2000, 0x3000, 0x4000, 0x6000, 0x8000, 0xC000,
	0x10000, 0x18000, 0x20000, 0x30000, 0x40000, 0x60000, 0x80000, 0xC0000,
	0x100000, 0x180000, 0x200000, 0x300000, 0x400000, 0x600000,
	0x800000, 0xC00000, 0x1000000, 0x1800000
};
static const UWORD32 UL_DT_EBITS[50] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
	9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
	17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23
};

/* ---- model pool layout (one malloc, fixed offsets) ----
   lit: 2 planes x 256 ctx x 17 sub x 16 syms; tots per sub.
   sec: 4 planes x 9 groups, cumulative (only groups 2..8 are used). */
#define UL_LITF   0
#define UL_LITT   (UL_LITF + 2*256*17*16)
#define UL_LDF    (UL_LITT + 2*256*17)
#define UL_LDT    (UL_LDF + 4*50)
#define UL_DLF    (UL_LDT + 4)
#define UL_DLT    (UL_DLF + 16)
#define UL_LNF    (UL_DLT + 1)
#define UL_LNT    (UL_LNF + 4*9)
#define UL_LSF    (UL_LNT + 4)
#define UL_LST    (UL_LSF + 4*9)
#define UL_LRF    (UL_LST + 4)
#define UL_LRT    (UL_LRF + 4*6)
#define UL_SNF    (UL_LRT + 4)
#define UL_SNT    (UL_SNF + 4*254)
#define UL_SRF    (UL_SNT + 4*9)
#define UL_SRT    (UL_SRF + 4*254)
#define UL_POOL   (UL_SRT + 4*9)

#define UL_MAX_DIC 0x2000000UL   /* 32 MB: the largest dictionary Squeez writes */

/* ---- session (solid archives keep the models across members) ---- */
struct SqxUltraSession
{
	UWORD32 *pool;         /* UL_POOL model freqs+tots */
	UBYTE *window;         /* dictionary window, dic bytes */
	UWORD32 dic;
	UWORD32 curr_pos;
	int ready;
	/* binary models: is_match[4][4][4] and is_rep[4][4] (f0,f1,tot) */
	UWORD32 im_f0[4][4][4], im_f1[4][4][4], im_tot[4][4][4];
	UWORD32 ir_f0[4][4], ir_f1[4][4], ir_tot[4][4];
	/* per-member range state (TOP/BOT carryless coder, re-inited per member) */
	UWORD32 rLow, rRange, rCode;
	/* per-member word-swapped bit reader over the member payload */
	const unsigned char *bitBase;
	UWORD32 bitLen, bitWpos, bitWord;
	unsigned bitBits;
};

static SqxUltraSession g_su;

void SqxUltra_BeginArchive(void)
{
	if( g_su.pool ) free(g_su.pool);
	if( g_su.window ) free(g_su.window);
	memset(&g_su, 0, sizeof(g_su));
}

/* ---- word-swapped bit reader (same semantics as SqxPpmd's) ---- */
static UWORD32 UltraBitsWord(SqxUltraSession *s, UWORD32 w)
{
	UWORD32 o = w * 4;
	UWORD32 v = 0;
	unsigned i;
	for( i = 0; i < 4; i++ )
		if( o + i < s->bitLen )
			v |= (UWORD32)s->bitBase[o + i] << (8 * i);
	return v;
}

static void UltraBitsInit(SqxUltraSession *s, const unsigned char *buf, UWORD32 len)
{
	s->bitBase = buf;
	s->bitLen = len;
	s->bitWpos = 0;
	s->bitBits = 0;
	s->bitWord = len ? UltraBitsWord(s, 0) : 0;
}

static UWORD32 UltraBitsPeek(SqxUltraSession *s, unsigned n)
{
	if( n == 0 )
		return 0;
	return n >= 32 ? s->bitWord : (s->bitWord >> (32 - n));
}

static void UltraBitsDump(SqxUltraSession *s, unsigned n)
{
	s->bitBits += n;
	s->bitWpos += s->bitBits >> 5;
	s->bitBits &= 31;
	s->bitWord = UltraBitsWord(s, s->bitWpos) << s->bitBits;
	if( s->bitBits )
		s->bitWord |= UltraBitsWord(s, s->bitWpos + 1) >> (32 - s->bitBits);
}

static UWORD32 UltraBitsGet(SqxUltraSession *s, unsigned n)
{
	UWORD32 v = UltraBitsPeek(s, n);
	UltraBitsDump(s, n);
	return v;
}

/* ---- TOP/BOT range core (same shape as SqxPpmd's) ---- */
#define ULTRA_TOP ((UWORD32)1 << 24)
#define ULTRA_BOT ((UWORD32)1 << 16)

static void UltraRcNorm(SqxUltraSession *s)
{
	for( ;; )
	{
		if( (s->rLow ^ (s->rLow + s->rRange)) >= ULTRA_TOP )
		{
			if( s->rRange >= ULTRA_BOT )
				break;
			s->rRange = (0UL - s->rLow) & (ULTRA_BOT - 1UL);
		}
		s->rCode = (s->rCode << 8) | UltraBitsGet(s, 8);
		s->rRange <<= 8;
		s->rLow <<= 8;
	}
}

/* ---- Fenwick model decode: symbol 0..nsym-1, or -1 on a corrupt stream ---- */
static int UltraFenwick(SqxUltraSession *s, UWORD32 fbase, UWORD32 nsym,
	UWORD32 incr, UWORD32 maxf, UWORD32 tbase)
{
	UWORD32 tot, q, rf, sym, cum = 0, i;
	tot = s->pool[tbase];
	q = s->rRange / tot;
	s->rRange = q;
	if( q )
		rf = (s->rCode - s->rLow) / q;
	else
		rf = tot;
	if( rf >= tot )
		return -1;
	for( sym = 0; sym < nsym; sym++ )
	{
		if( cum + s->pool[fbase + sym] > rf )
			break;
		cum += s->pool[fbase + sym];
	}
	if( sym >= nsym )
		return -1;
	s->rLow += cum * s->rRange;
	s->rRange *= s->pool[fbase + sym];
	s->pool[fbase + sym] += incr;
	s->pool[tbase] += incr;
	if( s->pool[tbase] > maxf )
	{
		UWORD32 t = 0;
		for( i = 0; i < nsym; i++ )
		{
			s->pool[fbase + i] -= (s->pool[fbase + i] >> 1);
			t += s->pool[fbase + i];
		}
		s->pool[tbase] = t;
	}
	UltraRcNorm(s);
	return (int)sym;
}

/* ---- binary model decode (incr 8, maxf 1023): 0/1, or -1 ---- */
static int UltraBinary(SqxUltraSession *s, UWORD32 *f0p, UWORD32 *f1p, UWORD32 *totp)
{
	UWORD32 tot, q, split, res;
	tot = *totp;
	q = s->rRange / tot;
	s->rRange = q;
	if( q )
		split = (s->rCode - s->rLow) / q;
	else
		split = tot;
	if( *f0p <= split )
	{
		if( *f0p + *f1p <= split )
			return -1;
		s->rLow += *f0p * s->rRange;
		s->rRange *= *f1p;
		*f1p += 8;
		res = 1;
	}
	else
	{
		s->rRange *= *f0p;
		*f0p += 8;
		res = 0;
	}
	*totp += 8;
	if( *totp > 1023 )
	{
		*f0p -= (*f0p >> 1);
		*f1p -= (*f1p >> 1);
		*totp = *f0p + *f1p;
	}
	UltraRcNorm(s);
	return (int)res;
}

/* ---- uniform n-bit decode, MSB first; -1 when the range collapses ---- */
static int UltraRaw(SqxUltraSession *s, unsigned n)
{
	UWORD32 v = 0;
	while( n-- )
	{
		UWORD32 r, q;
		s->rRange >>= 1;
		r = s->rRange;
		if( r == 0 )
			return -1;
		q = (s->rCode - s->rLow) / r;
		v = (v << 1) | (q & 1UL);
		s->rLow += q * r;
		UltraRcNorm(s);
	}
	return (int)v;
}

/* ---- literal: nibble pair hi = RC(16), byte = 16*hi + RC(lo[hi]) ---- */
static int UltraLiteral(SqxUltraSession *s, unsigned plane, UWORD32 prev)
{
	UWORD32 incr;
	int hi, lo;
	incr = plane ? 2 : 1;
	hi = UltraFenwick(s, UL_LITF + ((plane*256+prev)*17+0)*16, 16, incr, 511, UL_LITT + (plane*256+prev)*17+0);
	if( hi < 0 )
		return -1;
	lo = UltraFenwick(s, UL_LITF + ((plane*256+prev)*17+1+(UWORD32)hi)*16, 16, incr, 511, UL_LITT + (plane*256+prev)*17+1+(UWORD32)hi);
	if( lo < 0 )
		return -1;
	return hi * 16 + lo;
}

/* ---- model init/reset (incr/tot constants from the reference setup) ---- */
static void UltraInitModels(SqxUltraSession *s)
{
	unsigned a, b, c, p, n;
	for( a = 0; a < 2; a++ )
		for( b = 0; b < 256; b++ )
			for( c = 0; c < 17; c++ )
			{
				UWORD32 incr = a ? 2 : 1, k;
				for( k = 0; k < 16; k++ )
					s->pool[UL_LITF + ((a*256+b)*17+c)*16 + k] = incr;
				s->pool[UL_LITT + (a*256+b)*17+c] = 16 * incr;
			}
	for( a = 0; a < 4; a++ )
	{
		unsigned k;
		for( k = 0; k < 50; k++ )
			s->pool[UL_LDF + a*50+k] = 4;
		s->pool[UL_LDT + a] = 200;
		for( k = 0; k < 9; k++ )
		{
			s->pool[UL_LNF + a*9+k] = 8;
			s->pool[UL_LSF + a*9+k] = 8;
		}
		s->pool[UL_LNT + a] = 72;
		s->pool[UL_LST + a] = 72;
		for( k = 0; k < 6; k++ )
			s->pool[UL_LRF + a*6+k] = 4;
		s->pool[UL_LRT + a] = 24;
	}
	for( n = 0; n < 16; n++ )
		s->pool[UL_DLF + n] = 4;
	s->pool[UL_DLT] = 64;
	for( p = 0; p < 4; p++ )
		for( n = 0; n < 9; n++ )
		{
			unsigned k;
			for( k = 0; k < UL_LT_NSYM[n]; k++ )
			{
				s->pool[UL_SNF + p*254+UL_SEC_CUM[n]+k] = UL_LT_INCR[n];
				s->pool[UL_SRF + p*254+UL_SEC_CUM[n]+k] = UL_LT_INCR[n];
			}
			s->pool[UL_SNT + p*9+n] = UL_LT_NSYM[n] * UL_LT_INCR[n];
			s->pool[UL_SRT + p*9+n] = UL_LT_NSYM[n] * UL_LT_INCR[n];
		}
	for( a = 0; a < 4; a++ )
	{
		for( b = 0; b < 4; b++ )
		{
			for( c = 0; c < 4; c++ )
			{
				s->im_f0[a][b][c] = 8;
				s->im_f1[a][b][c] = 8;
				s->im_tot[a][b][c] = 16;
			}
			s->ir_f0[a][b] = 8;
			s->ir_f1[a][b] = 8;
			s->ir_tot[a][b] = 16;
		}
	}
}

/* ---- flush the window head through the house output path ----
   (mirrors the reference write_dictionary; the running to_flush count
   is not needed because the main loop is bounded by the member size) */
static void UltraFlush(SqxUltraSession *s, UWORD32 *wpos)
{
	UWORD32 curr = s->curr_pos, wposv = *wpos;
	if( curr == s->dic && wposv == s->dic )
	{
		WriteOutBuf(s->window, curr);
		*wpos = 0;
	}
	else if( curr < wposv )
	{
		WriteOutBuf(&s->window[wposv], (UWORD32)(-(SLONG32)wposv) & (s->dic - 1));
		WriteOutBuf(s->window, curr);
		*wpos = curr;
	}
	else
	{
		WriteOutBuf(&s->window[wposv], curr - wposv);
		*wpos = curr;
	}
}

/* ---- the symbol loop (reference sub_40D8B1).
   Returns 1 when exactly usz bytes were produced, 0 on a corrupt
   stream (or when the output path reports an error). Every iteration
   emits at least one byte or fails, so the loop always terminates. */
static int UltraDecodeBody(SqxUltraSession *s, UWORD32 usz)
{
	UWORD32 dist_mru[4];
	UWORD32 last_len, last_dist_m1, state, prev_state, prev_byte, cur_dist;
	UWORD32 write_pos, remaining, decoded;
	unsigned i;

	dist_mru[0] = dist_mru[1] = dist_mru[2] = dist_mru[3] = 0;
	last_len = 0; last_dist_m1 = 0;
	state = 0; prev_state = 0;
	prev_byte = 0; cur_dist = 1;
	s->curr_pos = 0; write_pos = 0;
	remaining = usz; decoded = 0;

	while( remaining > 0 && ArcStruct.ArchiveStatus == SQX_EC_OK )
	{
		int is_m, new_state, n;
		UWORD32 pos3 = decoded & 3;
		is_m = UltraBinary(s, &s->im_f0[pos3][state][prev_state], &s->im_f1[pos3][state][prev_state], &s->im_tot[pos3][state][prev_state]);
		if( is_m < 0 )
			return 0;
		if( is_m )
		{
			int is_r = UltraBinary(s, &s->ir_f0[state][prev_state], &s->ir_f1[state][prev_state], &s->ir_tot[state][prev_state]);
			if( is_r < 0 )
				return 0;
			if( is_r )
			{
				int g = UltraFenwick(s, UL_LRF + state*6, 6, 4, 1023, UL_LRT + state);
				if( g < 0 )
					return 0;
				if( g == 4 )
				{
					/* copy 1 byte at the current distance */
					UBYTE byte;
					if( cur_dist == 0 || cur_dist > s->dic )
						return 0;
					byte = s->window[(s->curr_pos - cur_dist) & (s->dic - 1)];
					s->window[s->curr_pos] = byte;
					if( ++s->curr_pos >= s->dic ) { UltraFlush(s, &write_pos); s->curr_pos &= s->dic - 1; }
					/* NB: prev becomes dict[(curr-dist)&mask] (the next
					   source byte), not the last output byte. */
					prev_byte = s->window[(s->curr_pos - cur_dist) & (s->dic - 1)];
					n = 1; new_state = 3;
				}
				else if( g == 5 )
				{
					/* repeat the last match */
					UWORD32 len = last_len, dist = last_dist_m1 + 1;
					if( len == 0 || len > remaining )
						return 0;
					if( dist == 0 || dist > s->dic )
						return 0;
					while( len-- )
					{
						s->window[s->curr_pos] = s->window[(s->curr_pos - dist) & (s->dic - 1)];
						if( ++s->curr_pos >= s->dic ) { UltraFlush(s, &write_pos); s->curr_pos &= s->dic - 1; }
					}
					cur_dist = dist;
					n = (int)last_len; new_state = 2;
					prev_byte = s->window[(s->curr_pos - dist) & (s->dic - 1)];
				}
				else
				{
					/* reuse distance MRU slot k = g+1, length from len_sub */
					UWORD32 g2, base, extra = 0, len, dist_m1, dist;
					int e;
					if( (unsigned)g > 3 )
						return 0;
					dist_m1 = dist_mru[g];
					e = UltraFenwick(s, UL_LSF + state*9, 9, 8, 1023, UL_LST + state);
					if( e < 0 )
						return 0;
					g2 = (UWORD32)e;
					if( g2 > 8 )
						return 0;
					base = UL_LT_BASE[g2];
					if( UL_LT_NEXT[g2] )
					{
						e = UltraFenwick(s, UL_SRF + state*254+UL_SEC_CUM[UL_LT_NEXT[g2]], UL_LT_NSYM[UL_LT_NEXT[g2]], UL_LT_INCR[UL_LT_NEXT[g2]], UL_LT_MAX[UL_LT_NEXT[g2]], UL_SRT + state*9+UL_LT_NEXT[g2]);
						if( e < 0 )
							return 0;
						extra = (UWORD32)e;
					}
					len = base + extra;
					if( len == 257 )
					{
						e = UltraRaw(s, 14);
						if( e < 0 )
							return 0;
						len = 257u + (UWORD32)e;
					}
					if( len == 0 || len > remaining )
						return 0;
					dist = dist_m1 + 1;
					if( dist == 0 || dist > s->dic )
						return 0;
					for( i = (unsigned)g; i > 0; i-- )
						dist_mru[i] = dist_mru[i-1];
					dist_mru[0] = dist_m1;
					last_len = len; last_dist_m1 = dist_m1;
					while( len-- )
					{
						s->window[s->curr_pos] = s->window[(s->curr_pos - dist) & (s->dic - 1)];
						if( ++s->curr_pos >= s->dic ) { UltraFlush(s, &write_pos); s->curr_pos &= s->dic - 1; }
					}
					cur_dist = dist;
					prev_byte = s->window[(s->curr_pos - dist) & (s->dic - 1)];
					n = (int)last_len; new_state = 2;
				}
			}
			else
			{
				/* fresh match with a new distance */
				UWORD32 g2, base, extra = 0, len, dclass, d, eb, dm1, dist;
				int e;
				e = UltraFenwick(s, UL_LNF + state*9, 9, 8, 1023, UL_LNT + state);
				if( e < 0 )
					return 0;
				g2 = (UWORD32)e;
				if( g2 > 8 )
					return 0;
				base = UL_LT_BASE[g2];
				if( UL_LT_NEXT[g2] )
				{
					e = UltraFenwick(s, UL_SNF + state*254+UL_SEC_CUM[UL_LT_NEXT[g2]], UL_LT_NSYM[UL_LT_NEXT[g2]], UL_LT_INCR[UL_LT_NEXT[g2]], UL_LT_MAX[UL_LT_NEXT[g2]], UL_SNT + state*9+UL_LT_NEXT[g2]);
					if( e < 0 )
						return 0;
					extra = (UWORD32)e;
				}
				len = base + extra;
				if( len == 257 )
				{
					e = UltraRaw(s, 14);
					if( e < 0 )
						return 0;
					len = 257u + (UWORD32)e;
				}
				if( len == 2 )
					dclass = 0;
				else if( len == 3 )
					dclass = 1;
				else if( len == 4 )
					dclass = 2;
				else
					dclass = 3;
				e = UltraFenwick(s, UL_LDF + dclass*50, 50, 4, 1023, UL_LDT + dclass);
				if( e < 0 )
					return 0;
				d = (UWORD32)e;
				if( d > 49 )
					return 0;
				eb = UL_DT_EBITS[d];
				dm1 = UL_DT_BASE[d];
				if( eb > 0 )
				{
					if( eb >= 4 )
					{
						e = UltraRaw(s, (unsigned)(eb - 4));
						if( e < 0 )
							return 0;
						dm1 += (UWORD32)e << 4;
						e = UltraFenwick(s, UL_DLF, 16, 4, 1023, UL_DLT);
						if( e < 0 )
							return 0;
						dm1 += (UWORD32)e;
					}
					else
					{
						e = UltraRaw(s, (unsigned)eb);
						if( e < 0 )
							return 0;
						dm1 += (UWORD32)e;
					}
				}
				dist = dm1 + 1;
				if( dist == 0 || dist > s->dic )
					return 0;
				if( len == 0 || len > remaining )
					return 0;
				{
					UWORD32 l = len;
					for( i = 3; i > 0; i-- )
						dist_mru[i] = dist_mru[i-1];
					dist_mru[0] = dm1;
					while( l-- )
					{
						s->window[s->curr_pos] = s->window[(s->curr_pos - dist) & (s->dic - 1)];
						if( ++s->curr_pos >= s->dic ) { UltraFlush(s, &write_pos); s->curr_pos &= s->dic - 1; }
					}
				}
				cur_dist = dist;
				prev_byte = s->window[(s->curr_pos - dist) & (s->dic - 1)];
				/* NB: last_len/last_dist_m1 are intentionally left
				   untouched here (the reference updates them only in
				   the repeat branch). */
				n = (int)len; new_state = 1;
			}
		}
		else
		{
			int sym = UltraLiteral(s, state ? 1 : 0, prev_byte);
			if( sym < 0 )
				return 0;
			s->window[s->curr_pos] = (UBYTE)sym;
			if( ++s->curr_pos >= s->dic ) { UltraFlush(s, &write_pos); s->curr_pos &= s->dic - 1; }
			prev_byte = (UWORD32)sym;
			n = 1; new_state = 0;
		}
		prev_state = state;
		state = (UWORD32)new_state;
		decoded += (UWORD32)n;
		remaining -= (UWORD32)n;
	}

	UltraFlush(s, &write_pos);
	return (remaining == 0 && ArcStruct.ArchiveStatus == SQX_EC_OK) ? 1 : 0;
}

/* ---- member driver: slurp the payload, decode, stream out ---- */
int SqxUnUltra(unsigned long dicSize, int solid, unsigned long compFlags)
{
	SLONG64 csz64 = IoStruct.l64CompSize;
	SLONG64 usz64 = IoStruct.l64OrigSize;
	unsigned char *comp;
	SLONG64 got;
	UWORD32 csz, usz;
	int i;

	if( csz64 < 0 || csz64 > 0x7FFFFFFFL || usz64 < 0 || usz64 > 0x7FFFFFFFL )
	{
		ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return 0;
	}
	csz = (UWORD32)csz64;
	usz = (UWORD32)usz64;

	if( dicSize == 0 || dicSize > UL_MAX_DIC || (dicSize & (dicSize - 1)) != 0 )
	{
		ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return 0;
	}
	if( compFlags != 0 )
	{
		/* extended-compressor prologue: never observed in the wild,
		   refused like the reference (which returns 0 here too) */
		ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return 0;
	}
	if( usz == 0 )
		return 1;

	comp = (unsigned char *)malloc(csz ? csz : 1);
	if( !comp )
	{
		ArcStruct.ArchiveStatus = SQX_EC_OUT_OF_MEMORY;
		return 0;
	}
	got = 0;
	while( got < csz64 )
	{
		UWORD32 want = (UWORD32)(csz64 - got);
		SLONG32 r = ReadDataBlock(comp + got, want);
		if( r <= 0 )
			break;
		got += r;
	}
	if( got != csz64 )
	{
		free(comp);
		ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return 0;
	}

	if( !g_su.ready )
	{
		g_su.pool = (UWORD32 *)malloc(UL_POOL * sizeof(UWORD32));
		if( !g_su.pool )
		{
			free(comp);
			ArcStruct.ArchiveStatus = SQX_EC_OUT_OF_MEMORY;
			return 0;
		}
		UltraInitModels(&g_su);
		g_su.ready = 1;
	}
	else if( !solid )
		UltraInitModels(&g_su);
	if( g_su.dic != (UWORD32)dicSize || !g_su.window )
	{
		if( g_su.window )
			free(g_su.window);
		g_su.window = 0;
		g_su.window = (UBYTE *)malloc(dicSize ? dicSize : 1);
		if( !g_su.window )
		{
			free(comp);
			ArcStruct.ArchiveStatus = SQX_EC_OUT_OF_MEMORY;
			return 0;
		}
		g_su.dic = (UWORD32)dicSize;
	}

	UltraBitsInit(&g_su, comp, csz);
	g_su.rLow = 0;
	g_su.rCode = 0;
	g_su.rRange = 0xFFFFFFFFUL;
	for( i = 0; i < 4; i++ )
		g_su.rCode = (g_su.rCode << 8) | UltraBitsGet(&g_su, 8);

	if( !UltraDecodeBody(&g_su, usz) )
	{
		free(comp);
		if( ArcStruct.ArchiveStatus == SQX_EC_OK )
			ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return 0;
	}
	free(comp);

	if( ArcStruct.ArchiveStatus != SQX_EC_OK )
		return 0;
	IoStruct.l64OrigSize = 0;
	ProgressStruct.l64Processed += usz;
	SetProgress(1);
	return 1;
}
