/* Core 2 LSB (methods 2..7, 0.2/0.3/0.4). Clean-room LZW decoder.
 * Stream (LSB-first within each byte):
 *   width starts at 9; limit = 1<<maxbits, maxbits = method+8.
 *   A code is (width-1) bits; if value+1 >= ncode-var2, one more bit
 *   follows and, when set, the value becomes (ncode-1-value). var2 is
 *   the encoder's next-free counter capped at maxcode.
 *   0x100 = reset, 0x101 = RLE block (8-bit len hi/lo, 8-bit value;
 *   first block after reset carries one extra literal byte), strings
 *   are parent/char chains; code == nextFree is the classic
 *   previous-string + its first char (KwKwK) case.
 */

#include <stdlib.h>
#include <string.h>
#include "UhaDec2.h"

typedef struct UhaLzwDict
{
	unsigned short par[1 << 15];/* max method 7 -> 1<<15 entries */
	unsigned char ch[1 << 15];
	unsigned char tmp[1 << 15];
	unsigned cap;
	unsigned maxbits;
} UhaLzwDict;

/* Buffered bit reader, LSB-first. */
typedef struct UhaBits
{
	Uha2ReadCb rcb;
	void* rctx;
	Uha2WriteCb wcb;
	void* wctx;
	unsigned char* in;
	unsigned in_pos;
	unsigned in_avail;
	unsigned in_cap;
	unsigned char* out;
	unsigned out_fill;
	unsigned out_cap;
	unsigned long produced;
	unsigned long expect;
	unsigned long bitbuf;
	unsigned bitcnt;
	int err;
} UhaBits;

static int bits_refill(UhaBits* s)
{
	int n;
	s->in_pos = 0;
	s->in_avail = 0;
	n = s->rcb ? s->rcb(s->rctx, s->in, s->in_cap) : 0;
	if( n < 0 )
		n = 0;
	if( n == 0 )
	{
		s->err = 3;
		return 0;
	}
	s->in_avail = (unsigned)n;
	return 1;
}

static int bits_byte(UhaBits* s)
{
	if( s->err )
		return 0;
	if( s->in_pos >= s->in_avail )
	{
		if( !bits_refill(s) )
			return 0;
	}
	return s->in[s->in_pos++] & 0xFF;
}

static int bits_flush(UhaBits* s)
{
	int w;
	if( s->out_fill == 0 )
		return 1;
	w = s->wcb(s->wctx, s->out, s->out_fill);
	if( w != 0 )
	{
		s->err = 4;
		return 0;
	}
	s->out_fill = 0;
	return 1;
}

static void bits_emit1(UhaBits* s, unsigned b)
{
	if( s->err || s->produced >= s->expect )
		return;
	s->out[s->out_fill++] = (unsigned char)(b & 0xFF);
	s->produced++;
	if( s->out_fill == s->out_cap )
		bits_flush(s);
}

static void bits_emit_fill(UhaBits* s, unsigned long n, unsigned b)
{
	unsigned char v = (unsigned char)(b & 0xFF);
	if( n > s->expect - s->produced )
		n = s->expect - s->produced;
	while( n > 0 && !s->err )
	{
		unsigned space, k, i;
		if( s->out_fill == s->out_cap )
		{
			if( !bits_flush(s) )
				break;
		}
		space = s->out_cap - s->out_fill;
		k = n < (unsigned long)space ? (unsigned)n : space;
		for( i = 0; i < k; i++ )
			s->out[s->out_fill + i] = v;
		s->out_fill += k;
		s->produced += k;
		n -= k;
		if( s->out_fill == s->out_cap )
		{
			if( !bits_flush(s) )
				break;
		}
	}
}

static unsigned bits_take(UhaBits* s, unsigned n)
{
	unsigned long mask, v;
	if( s->err || n == 0 )
		return 0;
	while( n > s->bitcnt )
	{
		int c = bits_byte(s);
		if( s->err )
			return 0;
		s->bitbuf |= (unsigned long)(unsigned)c << s->bitcnt;
		s->bitcnt += 8;
	}
	mask = (n >= 32) ? 0xFFFFFFFFUL : ((1UL << n) - 1UL);
	v = s->bitbuf & mask;
	s->bitbuf >>= n;
	s->bitcnt -= n;
	return (unsigned)v;
}

/* One code value, complement scheme included. */
static unsigned next_code(UhaBits* s, unsigned w1, unsigned ncode, unsigned enc_next)
{
	unsigned v = bits_take(s, w1);
	unsigned thresh;
	if( s->err )
		return 0;
	thresh = (enc_next < ncode) ? (ncode - enc_next) : 0;
	if( v + 1 >= thresh )
	{
		if( bits_take(s, 1) != 0 )
			v = (ncode - 1) - v;
	}
	return v;
}

int UhaDec2(int method, int lsb,
	Uha2ReadCb rcb, void* rctx, Uha2WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	UhaLzwDict* d;
	UhaBits s;
	unsigned char* inb;
	unsigned char* outb;
	unsigned maxcode, bits, ncode, w1, next_free, enc_next, prev, first;
	unsigned pending;
	unsigned i;

	if( method < 2 || method > 7 || !rcb || !wcb )
		return 1;
	if( out_len == 0 )
		return 0;
	if( !lsb )
		return 7;	/* 0.6 MSB core 2: Phase 3b */
	(void)rcb;

	d = (UhaLzwDict*)malloc(sizeof(*d));
	inb = (unsigned char*)malloc(0x10000);
	outb = (unsigned char*)malloc(0x10000);
	if( !d || !inb || !outb )
	{
		free(d); free(inb); free(outb);
		return 2;
	}
	d->cap = 1u << (unsigned)(method + 8);
	d->maxbits = (unsigned)(method + 8);
	for( i = 0; i < d->cap; i++ )
	{
		d->par[i] = 0;
		d->ch[i] = (unsigned char)(i & 0xFF);
		d->tmp[i] = 0;
	}

	memset(&s, 0, sizeof(s));
	s.rcb = rcb; s.rctx = rctx;
	s.wcb = wcb; s.wctx = wctx;
	s.in = inb; s.in_cap = 0x10000;
	s.out = outb; s.out_cap = 0x10000;
	s.expect = out_len;

	maxcode = d->cap - 1;
	bits = 9; ncode = 1u << bits; w1 = bits - 1;
	next_free = 0x102; enc_next = 0x102;
	prev = 0x100; first = 0; pending = 0;

	/* opening code: emit its low byte, no table entry */
	{
		unsigned code = next_code(&s, w1, ncode, enc_next);
		if( !s.err )
		{
			first = code & 0xFF;
			prev = code;
			enc_next++;
			bits_emit1(&s, first);
		}
	}

	while( s.produced < s.expect && !s.err )
	{
		unsigned code = next_code(&s, w1, ncode, enc_next);
		if( s.err )
			break;

		if( code == 0x100 )		/* reset */
		{
			bits = 9; ncode = 1u << bits; w1 = bits - 1;
			next_free = 0x102; enc_next = 0x102;
			code = next_code(&s, w1, ncode, enc_next);
			if( s.err )
				break;
			first = code & 0xFF;
			prev = code;
			enc_next++;
			bits_emit1(&s, first);
			continue;
		}
		if( code == 0x101 )		/* RLE block */
		{
			unsigned hi = bits_take(&s, 8);
			unsigned lo = bits_take(&s, 8);
			unsigned val = bits_take(&s, 8);
			if( s.err )
				break;
			if( !pending )
			{
				unsigned extra = bits_take(&s, 8);
				if( s.err )
					break;
				bits_emit1(&s, extra);
				pending = 1;
			}
			bits_emit_fill(&s, ((unsigned long)hi << 8) | lo, val);
			continue;
		}

		{
			unsigned orig = code;
			unsigned is_self = (code >= next_free) ? 1 : 0;
			unsigned len = 0, cur = code, root, k;
			if( is_self )
				code = prev;
			cur = code;
			while( cur >= 0x102 )
			{
				if( len >= d->cap || cur >= d->cap )
				{
					s.err = 7;
					break;
				}
				d->tmp[len++] = d->ch[cur];
				cur = d->par[cur];
			}
			if( s.err )
				break;
			if( cur >= d->cap )
			{
				s.err = 7;
				break;
			}
			root = cur & 0xFF;
			d->tmp[len++] = (unsigned char)root;
			first = root;
			if( pending )
			{
				pending = 0;
				if( len >= 2 )
					for( k = len - 1; k-- > 0; )
						bits_emit1(&s, d->tmp[k]);
			}
			else
			{
				for( k = len; k-- > 0; )
					bits_emit1(&s, d->tmp[k]);
			}
			if( s.err )
				break;
			if( is_self )
			{
				bits_emit1(&s, first);
				if( s.err )
					break;
			}
			if( next_free < d->cap )
			{
				d->par[next_free] = (unsigned short)prev;
				d->ch[next_free] = (unsigned char)root;
			}
			next_free++;
			if( next_free == ncode && bits < d->maxbits )
			{
				ncode <<= 1;
				bits++;
				w1 = bits - 1;
			}
			prev = orig;
			if( enc_next <= maxcode )
				enc_next++;
		}
	}

	if( !s.err && s.out_fill )
		bits_flush(&s);
	free(d);
	free(inb);
	free(outb);
	return s.err;
}
