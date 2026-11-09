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

/* 0.6 core 2 MSB (methods 2..7, tree coder). Port of the reference
 * uharc_dec2.c (UNUHARC06). Internal dec2_/t_ symbols kept verbatim;
 * only the public entries are renamed to house msb_* names. */

typedef struct MsbCtx_s {
    unsigned b;
    unsigned char *tree;
    unsigned char *tmp;
} MsbCtx;

/* ---- tree access (little-endian, 5-byte stride) ---- */
static unsigned t_get_w0(unsigned char *t, unsigned i)
{
    unsigned o = i * 5u;
    return (unsigned)(t[o] | ((unsigned)t[o + 1] << 8));
}
static void t_set_w0(unsigned char *t, unsigned i, unsigned v)
{
    unsigned o = i * 5u;
    t[o] = (unsigned char)(v & 0xFFu);
    t[o + 1] = (unsigned char)((v >> 8) & 0xFFu);
}
static unsigned t_get_w2(unsigned char *t, unsigned i)
{
    unsigned o = i * 5u;
    return (unsigned)(t[o + 2] | ((unsigned)t[o + 3] << 8));
}
static void t_set_w2(unsigned char *t, unsigned i, unsigned v)
{
    unsigned o = i * 5u;
    t[o + 2] = (unsigned char)(v & 0xFFu);
    t[o + 3] = (unsigned char)((v >> 8) & 0xFFu);
}
static unsigned t_get_ch(unsigned char *t, unsigned i)
{
    return (unsigned)t[i * 5u + 4];
}
static void t_set_ch(unsigned char *t, unsigned i, unsigned v)
{
    t[i * 5u + 4] = (unsigned char)(v & 0xFFu);
}

/* ---- public init/free ---- */
static int msb_init(void **pctx, int method)
{
    MsbCtx *c;
    unsigned b;
    unsigned char *tree;
    unsigned char *tmp;
    if (!pctx || method < 2 || method > 7)
        return 1;
    b = 1u << (unsigned)(method + 8);
    c = (MsbCtx *)malloc(sizeof(*c));
    if (!c)
        return 2;
    tree = (unsigned char *)malloc(5u * b);
    tmp = (unsigned char *)malloc(b ? b : 1u);
    if (!tree || !tmp) {
        free(tree);
        free(tmp);
        free(c);
        return 2;
    }
    memset(tree, 0, 5u * b);
    if (b)
        memset(tmp, 0, b);
    c->b = b;
    c->tree = tree;
    c->tmp = tmp;
    *pctx = (void *)c;
    return 0;
}

static void msb_free(void *ctx)
{
    MsbCtx *c = (MsbCtx *)ctx;
    if (!c)
        return;
    free(c->tree);
    free(c->tmp);
    free(c);
}

/* ---- decode I/O state ---- */
typedef struct dec2_io_s {
    Uha2ReadCb rcb;
    void *rctx;
    Uha2WriteCb wcb;
    void *wctx;
    unsigned char *inb;
    unsigned in_pos;
    unsigned in_avail;
    unsigned in_cap;
    unsigned char *outb;
    unsigned out_fill;
    unsigned out_cap;
    unsigned long out_total;
    unsigned long expected;
    int err;
    unsigned bitbuf;
    unsigned bitcnt;
} dec2_io;

static void dec2_progress(unsigned long a, unsigned long b)
{
    (void)a;
    (void)b;
}

static int dec2_refill(dec2_io *s)
{
    int n;
    s->in_pos = 0;
    s->in_avail = 0;
    n = s->rcb(s->rctx, s->inb, s->in_cap);
    if (n < 0)
        n = 0;
    if (n == 0) {
        s->err = 3;
        return 0;
    }
    s->in_avail = (unsigned)n;
    return 1;
}

static int dec2_get_byte(dec2_io *s)
{
    int c;
    if (s->err)
        return 0;
    if (s->in_pos >= s->in_avail) {
        if (!dec2_refill(s))
            return 0;
    }
    c = s->inb[s->in_pos++];
    return c & 0xFF;
}

static int dec2_flush(dec2_io *s)
{
    int w;
    if (s->out_fill == 0)
        return 1;
    w = s->wcb(s->wctx, s->outb, s->out_fill);
    if (w != 0) {
        s->err = 4;
        return 0;
    }
    s->out_fill = 0;
    return 1;
}

/* sub_40B178 */
static void dec2_emit1(dec2_io *s, unsigned b)
{
    if (s->err)
        return;
    s->outb[s->out_fill++] = (unsigned char)(b & 0xFFu);
    s->out_total++;
    if (s->out_fill == s->out_cap)
        dec2_flush(s);
}

/* sub_40B246: memset with flush */
static void dec2_emit_fill(dec2_io *s, unsigned long n, unsigned b)
{
    unsigned char v;
    v = (unsigned char)(b & 0xFFu);
    while (n > 0 && !s->err) {
        unsigned space;
        unsigned k;
        unsigned i;
        if (s->out_fill == s->out_cap) {
            if (!dec2_flush(s))
                break;
        }
        space = s->out_cap - s->out_fill;
        k = n < (unsigned long)space ? (unsigned)n : space;
        for (i = 0; i < k; i++)
            s->outb[s->out_fill + i] = v;
        s->out_fill += k;
        s->out_total += k;
        n -= k;
        if (s->out_fill == s->out_cap) {
            if (!dec2_flush(s))
                break;
        }
    }
}

/* sub_40B31F: memcpy with flush */
static void dec2_emit_copy(dec2_io *s, unsigned long n, unsigned char *src)
{
    while (n > 0 && !s->err) {
        unsigned space;
        unsigned k;
        if (s->out_fill == s->out_cap) {
            if (!dec2_flush(s))
                break;
        }
        space = s->out_cap - s->out_fill;
        k = n < (unsigned long)space ? (unsigned)n : space;
        memcpy(s->outb + s->out_fill, src, k);
        s->out_fill += k;
        s->out_total += k;
        src += k;
        n -= (unsigned long)k;
        if (s->out_fill == s->out_cap) {
            if (!dec2_flush(s))
                break;
        }
    }
}

/* sub_40B704 */
static unsigned dec2_bits(dec2_io *s, unsigned n)
{
    unsigned mask;
    if (s->err)
        return 0;
    if (n == 0)
        return 0;
    while (n > s->bitcnt) {
        int c;
        if (s->err)
            return 0;
        c = dec2_get_byte(s);
        if (s->err)
            return 0;
        s->bitbuf = (s->bitbuf << 8) | (unsigned)c;
        s->bitcnt += 8;
    }
    s->bitcnt -= n;
    if (n >= 32)
        mask = 0xFFFFFFFFUL;
    else
        mask = (unsigned)(((1UL << n) - 1UL) & 0xFFFFFFFFUL);
    return (s->bitbuf >> s->bitcnt) & mask;
}

/* sub_40BA80 prune; *pM = nextFree in/out */
static void dec2_prune(MsbCtx *c, unsigned *pM)
{
    unsigned m;
    unsigned v0;
    unsigned i;
    m = *pM;
    /* phase A: top-down, free trailing leaves, build child lists */
    if (m > 0) {
        v0 = m - 1;
        for (;;) {
            unsigned parent;
            if (v0 < 0x102u)
                break;
            parent = t_get_w2(c->tree, v0);
            if (parent < c->b) {
                if (t_get_w0(c->tree, parent) == 0x100u)
                    t_set_w0(c->tree, parent, 0u);
            }
            if (t_get_w0(c->tree, v0) == 0x100u) {
                t_set_w0(c->tree, v0, 0u);
                if (v0 < m)
                    m = v0;
            } else {
                unsigned tmp;
                if (t_get_w0(c->tree, v0) == 0u)
                    t_set_w0(c->tree, v0, 0x100u);
                if (parent < c->b) {
                    if (t_get_w0(c->tree, parent) == 0u)
                        t_set_w0(c->tree, parent, 0x100u);
                    tmp = t_get_w0(c->tree, parent);
                    t_set_w2(c->tree, v0, tmp);
                    t_set_w0(c->tree, parent, v0);
                }
            }
            if (v0 == 0)
                break;
            v0--;
        }
    }
    /* phase B: restore first 258 */
    for (i = 0; i < 0x102u; i++) {
        if (t_get_w0(c->tree, i) == 0u)
            t_set_w0(c->tree, i, 0x100u);
    }
    /* phase C: compact links pointing past m */
    i = 0;
    while (i < m) {
        unsigned cc;
        cc = t_get_w0(c->tree, i);
        if (cc != 0x100u && cc > m) {
            if (cc < c->b && m < c->b) {
                unsigned k;
                for (k = 0; k < 5u; k++)
                    c->tree[m * 5u + k] = c->tree[cc * 5u + k];
                t_set_w0(c->tree, i, m);
                t_set_w0(c->tree, cc, 0u);
                do {
                    m++;
                } while (m < c->b && t_get_w0(c->tree, m) != 0u);
            }
        }
        cc = t_get_w2(c->tree, i);
        if (cc != 0x100u && cc > m) {
            if (cc < c->b && m < c->b) {
                unsigned k;
                for (k = 0; k < 5u; k++)
                    c->tree[m * 5u + k] = c->tree[cc * 5u + k];
                t_set_w2(c->tree, i, m);
                t_set_w0(c->tree, cc, 0u);
                do {
                    m++;
                } while (m < c->b && t_get_w0(c->tree, m) != 0u);
            }
        }
        i++;
        if (i >= c->b)
            break;
        if (m > c->b)
            break;
    }
    /* phase D: child lists -> parent pointers */
    {
        unsigned res = 0;
        while (res < m) {
            unsigned cur;
            unsigned guard;
            cur = t_get_w0(c->tree, res);
            t_set_w0(c->tree, res, 0x100u);
            guard = 0;
            while (cur != 0x100u) {
                unsigned nxt;
                if (cur >= c->b)
                    break;
                nxt = t_get_w2(c->tree, cur);
                t_set_w2(c->tree, cur, res);
                cur = nxt;
                guard++;
                if (guard > c->b + 16u)
                    break;
            }
            res++;
        }
    }
    *pM = m;
}

static int msb_decode(void *ctx,
                      Uha2ReadCb rcb, void *rctx,
                      Uha2WriteCb wcb, void *wctx,
                      unsigned long out_len)
{
    MsbCtx *c = (MsbCtx *)ctx;
    dec2_io s;
    unsigned char *inb = 0;
    unsigned char *outb = 0;
    unsigned nextFree; /* 43454C */
    unsigned prev;     /* 434534 */
    unsigned flag;     /* 434550 */
    unsigned bits;     /* 434554 (byte) */
    unsigned i;

    if (!c || !rcb || !wcb)
        return 1;
    if (out_len == 0)
        return 0;
    if (c->b == 0 || !c->tree || !c->tmp)
        return 1;

    inb = (unsigned char *)malloc(0x10000u);
    outb = (unsigned char *)malloc(0x10000u);
    if (!inb || !outb) {
        free(inb);
        free(outb);
        return 2;
    }

    s.rcb = rcb;
    s.rctx = rctx;
    s.wcb = wcb;
    s.wctx = wctx;
    s.inb = inb;
    s.in_pos = 0;
    s.in_avail = 0;
    s.in_cap = 0x10000u;
    s.outb = outb;
    s.out_fill = 0;
    s.out_cap = 0x10000u;
    s.out_total = 0;
    s.expected = out_len;
    s.err = 0;
    s.bitbuf = 0;
    s.bitcnt = 0;

    /* sub_40B6BA reset */
    for (i = 0; i < 0x102u; i++) {
        t_set_w0(c->tree, i, 0x100u);
        t_set_w2(c->tree, i, 0x100u);
        t_set_ch(c->tree, i, i & 0xFFu);
    }
    nextFree = 0x101u;
    prev = 0x100u;
    flag = 0u;
    bits = 9u;

    while (s.out_total < s.expected && !s.err) {
        unsigned code; /* v1/esi */
        unsigned ncode;
        unsigned direct;
        unsigned sym;
        unsigned off;
        unsigned cur;
        unsigned rootb;

        /* sub_40B7F3 next code */
        ncode = 1u << bits;
        if (flag != 0u && bits != 9u) {
            if (dec2_bits(&s, 1u) != 0u) {
                unsigned v;
                unsigned thresh;
                v = dec2_bits(&s, bits - 1u);
                thresh = ncode - (nextFree + 1u);
                if (v + 1u >= thresh) {
                    if (dec2_bits(&s, 1u) != 0u)
                        v = (ncode - 1u) - v;
                }
                flag >>= 1;
                code = v + 256u;
            } else {
                code = dec2_bits(&s, 8u);
                flag += 15u;
            }
        } else {
            unsigned v;
            unsigned thresh;
            v = dec2_bits(&s, bits - 1u);
            thresh = ncode - (nextFree + 1u);
            if (v + 1u >= thresh) {
                if (dec2_bits(&s, 1u) != 0u)
                    v = (ncode - 1u) - v;
            }
            if (v < 0x100u && bits != 9u)
                flag = 1u;
            code = v;
        }
        if (s.err)
            break;

        if (code > nextFree)
            continue;
        if (code == 0x100u) {
            /* reset */
            for (i = 0; i < 0x102u; i++) {
                t_set_w0(c->tree, i, 0x100u);
                t_set_w2(c->tree, i, 0x100u);
                t_set_ch(c->tree, i, i & 0xFFu);
            }
            nextFree = 0x101u;
            prev = 0x100u;
            flag = 0u;
            bits = 9u;
            continue;
        }
        if (code == 0x101u) {
            /* sub_40B757 RLE block */
            unsigned long len = 0;
            unsigned bval;
            if (dec2_bits(&s, 1u) != 0u) {
                len = (unsigned long)dec2_bits(&s, 8u);
            } else if (dec2_bits(&s, 1u) != 0u) {
                len = (unsigned long)dec2_bits(&s, 12u);
                len += 0x100UL;
            } else if (dec2_bits(&s, 1u) != 0u) {
                len = (unsigned long)dec2_bits(&s, 16u);
                len += 0x1100UL;
            } else {
                unsigned lo;
                unsigned hi;
                lo = dec2_bits(&s, 16u);
                hi = dec2_bits(&s, 16u);
                len = (unsigned long)lo + ((unsigned long)hi << 16);
                len += 0x1100UL;
            }
            if (s.err)
                break;
            len += 0x100UL;
            bval = dec2_bits(&s, 8u);
            if (s.err)
                break;
            dec2_emit_fill(&s, len, bval);
            continue;
        }

        direct = (code < nextFree || prev == 0x100u) ? 1u : 0u;
        sym = direct ? code : prev;

        /* walk parent chain, collect reversed in tmp */
        off = c->b;
        cur = sym;
        while (t_get_w2(c->tree, cur) != 0x100u) {
            unsigned pw;
            if (off == 0) {
                s.err = 7;
                break;
            }
            pw = t_get_w2(c->tree, cur);
            if (pw >= c->b) {
                s.err = 7;
                break;
            }
            off--;
            c->tmp[off] = (unsigned char)t_get_ch(c->tree, cur);
            cur = pw;
            if (cur >= c->b) {
                s.err = 7;
                break;
            }
        }
        if (s.err)
            break;
        rootb = t_get_ch(c->tree, cur);
        if (off == c->b) {
            dec2_emit1(&s, rootb);
        } else {
            unsigned long ln;
            if (off == 0) {
                s.err = 7;
                break;
            }
            off--;
            c->tmp[off] = (unsigned char)rootb;
            ln = (unsigned long)(c->b - off);
            dec2_emit_copy(&s, ln, c->tmp + off);
        }
        if (s.err)
            break;
        if (!direct)
            dec2_emit1(&s, rootb);
        if (s.err)
            break;
        if (prev != 0x100u) {
            unsigned idx = nextFree;
            if (idx < c->b) {
                t_set_w0(c->tree, idx, 0x100u);
                t_set_ch(c->tree, idx, rootb);
                t_set_w2(c->tree, idx, prev);
            }
        }
        nextFree++;
        if ((1u << bits) == nextFree)
            bits++;
        prev = code;
        if (nextFree == c->b) {
            dec2_prune(c, &nextFree);
            while (bits > 1 && ((1u << (bits - 1u)) >= nextFree))
                bits--;
            prev = 0x100u;
            if (nextFree > 0)
                nextFree--;
        }
        if ((s.out_total & 0xFu) == 0u)
            dec2_progress(s.out_total, s.out_total);
    }

    if (!s.err && s.out_fill)
        dec2_flush(&s);
    free(inb);
    free(outb);
    return s.err;
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
	{
		void* mctx;
		int mrc;
		mrc = msb_init(&mctx, method);
		if( mrc != 0 )
			return mrc;
		mrc = msb_decode(mctx, rcb, rctx, wcb, wctx, out_len);
		msb_free(mctx);
		return mrc;
	}
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
