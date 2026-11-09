/* Cores 0 STORE + 1 RLE + shared core selector. Clean-room. */

#include <stddef.h>
#include <string.h>
#include "UhaDec0.h"

int UhaCoreOf(int method, int ver)
{
	if( method < 0 )
		return -1;
	if( ver == 2 )
	{
		if( method > 43 )
			return -1;
		if( method <= 1 )
			return method;
		if( method <= 7 )
			return 2;
		return 3;
	}
	if( ver == 3 )
	{
		if( method == 80 )
			return 0;
		if( method > 79 )
			return -1;
		if( method <= 1 )
			return method;
		if( method <= 7 )
			return 2;
		if( method <= 21 )
			return 3;
		if( method <= 35 )
			return 4;
		return 5;
	}
	if( method > 87 )
		return -1;
	if( method <= 1 )
		return method;
	if( method <= 7 )
		return 2;
	if( method <= 23 )
		return 3;
	if( method <= 39 )
		return 4;
	return 5;
}

unsigned long UhaDictSize(int method, int ver)
{
	int base, span;
	if( ver == 2 )
	{
		if( method < 8 || method > 43 )
			return 0;
		base = 8; span = 12;
	}
	else if( ver == 3 )
	{
		if( method >= 8 && method <= 21 )
		{
			base = 8; span = 14;
		}
		else if( method >= 22 && method <= 35 )
		{
			base = 22; span = 14;
		}
		else if( method >= 36 && method <= 79 )
		{
			base = 36; span = 14;
		}
		else
			return 0;
	}
	else
	{
		if( method >= 8 && method <= 23 )
		{
			base = 8; span = 16;
		}
		else if( method >= 24 && method <= 39 )
		{
			base = 24; span = 16;
		}
		else if( method >= 40 && method <= 87 )
		{
			base = 40; span = 16;
		}
		else
			return 0;
	}
	return 1UL << (unsigned)(((method - base) % span) + 10);
}

int UhaDecStore(UhaReadCb rcb, void* rctx, UhaWriteCb wcb, void* wctx,
	unsigned long out_len)
{
	unsigned char buf[8192];
	unsigned long left = out_len;
	if( !rcb || !wcb )
		return 1;
	while( left )
	{
		unsigned want = left > sizeof(buf) ? sizeof(buf) : (unsigned)left;
		int n = rcb(rctx, buf, want);
		if( n <= 0 )
			return 3;
		if( wcb(wctx, buf, (unsigned)n) != 0 )
			return 4;
		left -= (unsigned long)n;
	}
	return 0;
}

/* Buffered byte reader over the read callback. */
typedef struct RleReader
{
	UhaReadCb rcb;
	void* rctx;
	UhaWriteCb wcb;
	void* wctx;
	unsigned char buf[8192];
	unsigned pos;
	unsigned avail;
	unsigned long produced;
	unsigned long expect;
	int err;
} RleReader;

static int rle_byte(RleReader* r)
{
	if( r->err )
		return -1;
	if( r->pos >= r->avail )
	{
		int n = r->rcb(r->rctx, r->buf, sizeof(r->buf));
		if( n <= 0 )
		{
			r->err = 3;
			return -1;
		}
		r->avail = (unsigned)n;
		r->pos = 0;
	}
	return r->buf[r->pos++];
}

static int rle_emit(RleReader* r, unsigned char b)
{
	if( r->produced >= r->expect )
		return 0;
	if( r->wcb(r->wctx, &b, 1) != 0 )
	{
		r->err = 4;
		return 0;
	}
	r->produced++;
	return 1;
}

static int rle_fill(RleReader* r, unsigned long n, unsigned char v)
{
	unsigned char chunk[8192];
	if( n > r->expect - r->produced )
		n = r->expect - r->produced;
	memset(chunk, v, sizeof(chunk));
	while( n )
	{
		unsigned step = n > sizeof(chunk) ? sizeof(chunk) : (unsigned)n;
		if( r->wcb(r->wctx, chunk, step) != 0 )
		{
			r->err = 4;
			return 0;
		}
		r->produced += step;
		n -= step;
	}
	return 1;
}

/* Core 1 (RLE/delta). Predictor starts at 0xED. For each control
 * byte c: diff = c - pred; if (diff >> 1) != 0 the byte is a
 * literal. Otherwise the next byte d selects:
 *   c == pred  -> d repeated 3 times
 *   d < 2      -> pred+d once, pred -= 0x40
 *   d <= 3     -> pred+d-2 twice, pred -= 0x40
 *   d == 0xFF  -> 16-bit LE length, then a value byte: fill that
 *                 many copies
 *   else       -> value byte: fill d copies
 */
int UhaDecRle(UhaReadCb rcb, void* rctx, UhaWriteCb wcb, void* wctx,
	unsigned long out_len)
{
	RleReader r;
	unsigned char pred;
	if( !rcb || !wcb )
		return 1;
	if( out_len == 0 )
		return 0;
	r.rcb = rcb; r.rctx = rctx;
	r.wcb = wcb; r.wctx = wctx;
	r.pos = 0; r.avail = 0;
	r.produced = 0;
	r.expect = out_len;
	r.err = 0;
	pred = 0xED;
	while( r.produced < r.expect && !r.err )
	{
		int c = rle_byte(&r);
		int d, v;
		long diff;
		unsigned long ln;
		if( c < 0 )
			break;
		diff = (long)c - (long)pred;
		if( (diff >> 1) != 0 )
		{
			rle_emit(&r, (unsigned char)c);
			continue;
		}
		d = rle_byte(&r);
		if( d < 0 )
			break;
		if( c == (int)pred )
		{
			rle_emit(&r, (unsigned char)d);
			rle_emit(&r, (unsigned char)d);
			rle_emit(&r, (unsigned char)d);
		}
		else if( d < 2 )
		{
			rle_emit(&r, (unsigned char)(pred + d));
			pred = (unsigned char)(pred - 0x40);
		}
		else if( d <= 3 )
		{
			rle_emit(&r, (unsigned char)(pred + d - 2));
			rle_emit(&r, (unsigned char)(pred + d - 2));
			pred = (unsigned char)(pred - 0x40);
		}
		else if( d == 0xFF )
		{
			int lo = rle_byte(&r);
			int hi;
			if( lo < 0 )
				break;
			hi = rle_byte(&r);
			if( hi < 0 )
				break;
			ln = (unsigned long)(lo | (hi << 8));
			v = rle_byte(&r);
			if( v < 0 )
				break;
			rle_fill(&r, ln, (unsigned char)v);
		}
		else
		{
			v = rle_byte(&r);
			if( v < 0 )
				break;
			rle_fill(&r, (unsigned long)d, (unsigned char)v);
		}
	}
	return r.err;
}
