/* UHARC 0.6 core 4 (PPM) - clean typed implementation (C89).
 * See uharc_dec4_clean.h for status. Port of uharc-rs ppm.rs.
 */
#include <stdlib.h>
#include <string.h>
#include "UhaDec4.h"
#ifndef _MSC_VER
#ifndef UHI64
#define UHI64 unsigned long long
#endif
#else
#ifndef UHI64
#define UHI64 unsigned __int64
#endif
#endif
#ifndef uharc_dec_read_cb
typedef Uha4ReadCb uharc_dec_read_cb;
typedef Uha4WriteCb uharc_dec_write_cb;
#endif
#include "dec4c_rom.h"
#include "dec4c_rom2.h"

#define D4_DICT_MAX (32UL << 20) /* 32 MB max for method 39 */

#define DEC4C_SEE_LEN 0x8000U
#define SI(a) (((a) - 0x446464U) / 4U)
#define D4C_ML SI(0x446464U)
#define D4C_S_O4BIN SI(0x448464U)
#define D4C_S_O4ESC SI(0x450464U)
#define D4C_S_O2BIN SI(0x452464U)
#define D4C_S_O4FST SI(0x452864U)
#define D4C_S_O2MRU SI(0x453064U)
#define D4C_S_O1TOT SI(0x453464U)
#define D4C_S_O2O2 SI(0x4534e4U)
#define D4C_S_O4O2 SI(0x4554e4U)
#define D4C_S_O1MRU SI(0x4558e4U)
#define D4C_S_O2ESC SI(0x4578e4U)
#define D4C_MARKS SI(0x457964U)
#define D4C_S_O2FST SI(0x457d64U)
#define D4C_S_O4MPS SI(0x458564U)
#define D4C_S_O4MRU SI(0x45a564U)

static unsigned char dec4c_rom8(unsigned long a)
{
    unsigned long off = a - 0x422BA0UL;
    if (a < 0x422BA0UL || off >= sizeof(DEC4C_DG_422BA0))
        return 0;
    return DEC4C_DG_422BA0[off];
}

static unsigned long dec4c_rom32(unsigned long a)
{
    unsigned long v = dec4c_rom8(a);
    v |= ((unsigned long)dec4c_rom8(a + 1UL)) << 8;
    v |= ((unsigned long)dec4c_rom8(a + 2UL)) << 16;
    v |= ((unsigned long)dec4c_rom8(a + 3UL)) << 24;
    return v;
}

typedef struct dec4c_ctx_s {
    int method;
    unsigned long dictsize;
    unsigned long dictmask;
    unsigned char *dict;      /* history buffer (wraps at dictsize) */
    unsigned long pos;        /* write position (0x45d110) */
    /* Range coder (0x419a4d): 5 priming bytes, 32-bit code/range */
    unsigned long code;
    unsigned long range;
    unsigned long rc_t;       /* 0x45c67c total */
    unsigned char *inbuf;     /* stream bytes (whole packed stream) */
    unsigned long in_len;
    unsigned long in_pos;
    /* Stream header (0x413a96) */
    unsigned char binmode;    /* binary mode from start */
    unsigned long sw_pos;     /* text->binary switch position */
    /* File boundaries / kennedy (flags2 bit6) */
    unsigned long kennedy_end; /* 0x45d124 equivalent, 0xFFFFFFFF if off */
    unsigned char g_e395;      /* 0x45e395 equivalent (0 or 10) */
    unsigned long out_pos;
    unsigned long out_len;
    /* Segment/multimedia type (0x40deb6/0x41013c) */
    unsigned char mmtype;      /* 0x45e390 */
    unsigned long mmctx;       /* 0x45ae20 */
    unsigned long mmper;       /* 0x45ae24 */
    unsigned long channel;          /* 0x45ae2c channel */
    unsigned char g_e38c;      /* 0x45e38c */
    unsigned char g_e38b;       /* 0x45e38b predicted byte */
    unsigned char g_e38e;      /* 0x45e38e */
    unsigned char g_e38d;      /* 0x45e38d (type==5||6) */
    /* Mm predictor state (0x412fb0, types 5..9) */
    struct {
        unsigned long sel, d730, d734, d738, d73c, d740, d744;
        unsigned long acc[0x80];
        unsigned long hist[0x80];
        unsigned long pred[8];
        unsigned long errs[6][0x101];
        unsigned long rec[4][31];
        unsigned char sgn[4];
    } mm;
    /* Order-1 tables (0x40cdd1): 0x301 contexts */
    unsigned short esc[0x400];   /* 0x4236c8 escape counts */
    unsigned short inc_u[0x400]; /* 0x45d768 */
    unsigned short inc_b[0x400]; /* 0x45d166 */
    unsigned long ba3c[0x400];   /* 0x45ba3c */
    unsigned long ae38[0x400];   /* 0x45ae38 */
    unsigned char mru_s[0x400];  /* 0x45e086 */
    unsigned char mru_n[0x400];  /* 0x45dd85 */
    /* SEE tables (0x40dbc5): one array, indexed by SI() */
    unsigned long see[DEC4C_SEE_LEN];
    /* Segment state (0x40deb6) */
    unsigned short o1t[0x310][2][256]; /* CUM=0, FRQ=1 per context */
    unsigned short mflag[0x400];       /* 0x445db4 */
    unsigned char pair[0x80];          /* 0x45ad64 delta pairs */
    unsigned long g6b8[0x400];         /* 0x45c6b8/0x45ccd4 */
    unsigned long recent[256], lastpos[256], per[256];
    unsigned long ring[256], avg_a[2], avg_b[2];
    unsigned long qfb[4], qctx[4];      /* 0x444584/94 queued contexts */
    unsigned char qsym[4];             /* 0x45ae34 queued symbols */
    unsigned long g45c6b0, g45c6ac, g45c6a0, g45c6b4, g45c68c, g45c688;
    unsigned long g45c66c, g45c674, g45c64c, g45c670;
    unsigned long g45c650, g45c644, g45c640, g45c654, g45c648, g45c690;
    unsigned long seg_end, text_end, file_idx;
    unsigned long g45d104, g45d120, g45d124, g45d130, g45d134, g45d138, g45d140;
    unsigned long nbytes, c0, c1, c2;  /* 0x45d0f8/fc/100 last symbols */
    unsigned long pend, mchk_p, pair_p; /* 0x45d11c/148/14c, 0xFFFFFFFF=none */
    unsigned long *mh1, *mh2;           /* 0x423688/8c hashes, 0x80000 each */
    unsigned char *mlen, *mchk, *x698; /* 0x423690/94/98, 0x40000 each */
    unsigned long x698p[4];             /* 0x45d150.. */
    unsigned char dd6a[2], dd6c[2];     /* 0x45dd6a/c */
    unsigned char ty3[8], cnt3[8];      /* 0x45d160/0x44316c */
    unsigned char g_e389, g_e38a, g_e38f;
    unsigned long g45ae18;
    unsigned char g_e391;               /* 0x45e391 (mm replay gate) */
    unsigned long g45ae14;              /* 0x45ae14 */
    unsigned long imgq_n;               /* 0x45ade4 (never set by decoder) */
    unsigned char g_e3a0, g_e3a2;       /* 0x45e3a0/a2 */
    unsigned long g45d114;              /* 0x45d114 match continuation */
    /* decode_symbol locals (0x41030f) */
    unsigned long g45c6a4, g45c694, g45c69c, g45c678, g45c698, g45c6a8;
    unsigned char g_e394, g_e397, g_e398, g_e39d, g_e39a, g_e399, g_e39f, g_e393, g_e396;
    unsigned long g45c658, g45c668;
    unsigned long didx[8];              /* 0x446044 delta indices */
    unsigned char g_e3a1, g_e3a4, g_e3a5;
    unsigned char g_e3a3;
    unsigned long g_l108;               /* order-4 bucket weight (w54d) */
    unsigned char g_l28;                /* order-4 bucket symbol (l28) */
    unsigned char bad;
    unsigned char g_e39b, g_e39e;      /* 0x45e39b/e9e text states */
    unsigned char *outflush;            /* streaming output buffer */
    unsigned out_fill;
    unsigned out_cap;
    uharc_dec_write_cb wcb;
    void *wctx;
    int out_err;
    unsigned char excl[256];            /* 0x45dd6e excluded symbols */
    unsigned char nex;                  /* 0x45e39c */
    unsigned long *fe;                 /* file ends */
    unsigned long fe_n, fe_idx;
    unsigned char fe_on;
    /* Order-2 records (0x423684, 20 B each) + delta (binary mode).
     * o2 covers 0..0x10000, delta covers 0x10000.. via o2r/o2_put
     * (each 4-byte word of a 20-byte record is one delta entry). */
    unsigned long *o2;                   /* 0x10000 * 5 u32 */
    unsigned char (*delta)[4];            /* 0x100000 * 4 bytes */
    unsigned char *o4;                   /* 0x40000 * 32 bytes */
} dec4c_ctx;

static unsigned char g_c_flags2;

void UhaDec4_06SetFlags2(unsigned char flags2)
{
    g_c_flags2 = flags2;
}

int UhaDec4_06Init(void **ctx, int method)
{
    dec4c_ctx *c;
    if (!ctx) return 1;
    *ctx = 0;
    if (method < 24 || method > 39) return 1;
    c = (dec4c_ctx *)calloc(1, sizeof(*c));
    if (!c) return 2;
    /* dict = 1 << (((m-24) % 16) + 10), as in 0.6 dec4; +0x100 mirror */
    c->dictsize = 1UL << (unsigned)(((method - 24) % 16) + 10);
    c->dictmask = c->dictsize - 1UL;
    c->dict = (unsigned char *)calloc(1, (size_t)(c->dictsize + 0x100UL));
    c->mh1 = (unsigned long *)calloc(0x80000UL, sizeof(unsigned long));
    c->mh2 = (unsigned long *)calloc(0x80000UL, sizeof(unsigned long));
    c->mlen = (unsigned char *)calloc(0x40000UL, 1);
    c->mchk = (unsigned char *)calloc(0x40000UL, 1);
    c->x698 = (unsigned char *)calloc(0x40000UL, 1);
    c->o2 = (unsigned long *)calloc(0x10000UL * 5UL, sizeof(unsigned long));
    c->delta = (unsigned char (*)[4])(void *)calloc(0x100000UL, 4U);
    c->o4 = (unsigned char *)calloc(0x40000UL, 32U);
    if (!c->dict || !c->mh1 || !c->mh2 || !c->mlen || !c->mchk || !c->x698 ||
        !c->o2 || !c->delta || !c->o4) {
        free(c->dict); free(c->mh1); free(c->mh2);
        free(c->mlen); free(c->mchk); free(c->x698);
        free(c->o2); free(c->delta); free(c->o4);
        free(c); return 2;
    }
    /* hashes init to 0xFFFFFFFF (calloc gives 0, so fill) */
    memset(c->mh1, 0xFF, 0x80000UL * sizeof(unsigned long));
    memset(c->mh2, 0xFF, 0x80000UL * sizeof(unsigned long));
    c->method = method;
    c->kennedy_end = 0xFFFFFFFFUL;
    c->pend = c->mchk_p = c->pair_p = 0xFFFFFFFFUL;
    /* Rust Ppm::new scalar inits (0x40cdd1 globals). */
    c->g45d104 = 1UL;
    c->g45d120 = 1UL;
    c->g45d138 = 1UL;
    c->g45d124 = 0xFFFFFFFFUL;
    c->g45d130 = 0xFFFFFFFFUL;
    c->text_end = 0xFFFFFFFFUL;
    c->x698p[0] = 0UL;
    c->x698p[1] = 0x10000UL;
    c->x698p[2] = 0x20000UL;
    c->x698p[3] = 0x30000UL;
    *ctx = c;
    return 0;
}

void UhaDec4_06Free(void *ctx)
{
    dec4c_ctx *c = (dec4c_ctx *)ctx;
    if (!c) return;
    free(c->dict);
    free(c->mh1); free(c->mh2);
    free(c->mlen); free(c->mchk); free(c->x698);
    free(c->o2); free(c->delta); free(c->o4);
    free(c->fe);
    free(c->outflush);
    free(c->inbuf);
    free(c);
}

static unsigned char dec4c_get_byte(dec4c_ctx *c)
{
    if (c->in_pos < c->in_len)
        return c->inbuf[c->in_pos++];
    return 0;
}

/* Range coder init (0x419a4d): code=0, range=0xFFFFFFFF, 5 bytes. */
static void dec4c_rc_init(dec4c_ctx *c)
{
    int i;
    c->code = 0;
    c->range = 0xFFFFFFFFUL;
    for (i = 0; i < 5; i++)
        c->code = (c->code << 8) | dec4c_get_byte(c);
}

/* Decode a value in [0,total) using range coder (decoder side). */
static unsigned long dec4c_rc_div(dec4c_ctx *c, unsigned long total)
{
    unsigned long range, code, v;
    c->rc_t = total & 0xFFFFFFFFUL;
    if (total == 0) { c->bad = 1; return 0; }
    range = c->range / total;
    if (range == 0) { c->bad = 1; return 0; }
    v = code = c->code / range;
    c->range = range;
    return v;
}

static void dec4c_rc_dec(dec4c_ctx *c, unsigned long lo, unsigned long hi)
{
    unsigned long range = c->range;
    c->code = c->code - lo * range;
    c->range = (hi - lo) * range;
    while (c->range < (1UL << 24)) {
        c->code = (c->code << 8) | dec4c_get_byte(c);
        c->range <<= 8;
    }
}

static unsigned char dec4c_tb(dec4c_ctx *c, unsigned long i)
{
    if (i < c->dictsize + 0x100UL && c->dict)
        return c->dict[i];
    return 0;
}

static unsigned char dec4c_hist(dec4c_ctx *c, unsigned long back)
{
    return dec4c_tb(c, (c->pos - back) & c->dictmask);
}

static void dec4c_push_hist(dec4c_ctx *c, unsigned char b)
{
    unsigned long pos = c->pos;
    c->dict[pos] = b;
    if (pos < 0x100UL)
        c->dict[pos + c->dictsize] = b;
    c->pos = (pos + 1UL) & c->dictmask;
}

/* Equal-byte count backwards from a and b (max 64). */
static unsigned dec4c_match_len(dec4c_ctx *c, unsigned long a, unsigned long b)
{
    unsigned n = 0;
    while (n < 0x40U &&
           dec4c_tb(c, (a - n) & 0xFFFFFFFFUL) == dec4c_tb(c, (b - n) & 0xFFFFFFFFUL))
        n++;
    return n;
}

/* Candidate search part of 0x40f9d0 (order-12 + last-distance pre-check).
 * NOTE: this helper is superseded by dec4c_match_model(), which performs
 * the same search inline; kept out of the build to avoid a duplicate. */

/* 0x40e810: update match-flag model. */
static void dec4c_match_flag_update(dec4c_ctx *c, unsigned ctx, unsigned k)
{
    unsigned base = 4U * ctx;
    if (k == 1)
        c->mflag[base + 2] = (unsigned short)(c->mflag[base + 2] + 0x20U);
    c->mflag[base + 3] = (unsigned short)(c->mflag[base + 3] + 0x20U);
    if (c->mflag[base + 3] >= 0x4000U) {
        unsigned long datv = c->mflag[base + 2];
        unsigned long cntv = c->mflag[base + 1];
        unsigned long cc = (datv - cntv + 1U) >> 1;
        unsigned long ss = ((c->mflag[base + 3] - datv + 1U) >> 1) & 0xFFFFFFFFUL;
        cc = (cc + cntv) & 0xFFFFUL;
        ss = (ss + cc) & 0xFFFFUL;
        c->mflag[base + 2] = (unsigned short)cc;
        c->mflag[base + 3] = (unsigned short)ss;
    }
    {
        unsigned i = (unsigned)(c->g_e3a2) * 2U + k;
        c->g_e3a2 = dec4c_rom8(0x4239bbUL + (i & 0x1FU));
    }
}

/* Forward: defined after init_see. */
static void dec4c_set_mmtype(dec4c_ctx *c, unsigned char symb);

/* 0x41013c: 3-way match flag (0=change type, 1=use, 2=reject). */
static unsigned dec4c_match_flag(dec4c_ctx *c, unsigned ctx)
{
    for (;;) {
        unsigned base = 4U * ctx;
        unsigned long tot;
        unsigned v;
        unsigned k = 3U;
        unsigned basv;
        unsigned long lo, hi;
        tot = c->mflag[base + 3];
        v = (unsigned)dec4c_rc_div(c, tot);
        basv = base + 3;
        for (;;) {
            basv -= 1U;
            k -= 1U;
            if (v >= c->mflag[basv] || k == 0)
                break;
        }
        lo = c->mflag[basv];
        hi = c->mflag[basv + 1];
        dec4c_rc_dec(c, lo, hi);
        if (k != 0) {
            dec4c_match_flag_update(c, ctx, k);
            return k;
        }
        if (c->bad)
            return 2;
        {
            unsigned long vv = dec4c_rc_div(c, 10UL);
            unsigned long t = vv & 0xFFUL;
            dec4c_rc_dec(c, t, t + 1UL);
            dec4c_set_mmtype(c, (unsigned char)vv);
        }
    }
}

/* First symbol (0x413a96): 1 bit binary mode, else switch position. */
static void dec4c_read_switch_pos(dec4c_ctx *c)
{
    unsigned long bit, v, more;
    unsigned shift;
    bit = dec4c_rc_div(c, 2);
    c->binmode = (unsigned char)(bit & 0xFFUL);
    dec4c_rc_dec(c, bit & 0xFFUL, (bit & 0xFFUL) + 1UL);
    if (c->binmode != 0) {
        c->text_end = 0xFFFFFFFFUL;
        return;
    }
    c->sw_pos = 0;
    shift = 0;
    for (;;) {
        v = dec4c_rc_div(c, 0x800UL) & 0xFFFFUL;
        dec4c_rc_dec(c, v, v + 1UL);
        c->sw_pos = (c->sw_pos + (v << shift)) & 0xFFFFFFFFUL;
        more = dec4c_rc_div(c, 2) & 0xFFFFUL;
        dec4c_rc_dec(c, more & 0xFFUL, (more & 0xFFUL) + 1UL);
        shift += 11;
        if (more == 0)
            break;
    }
    c->text_end = c->sw_pos;
}

/* SEE init (0x40dbc5). */
static void dec4c_init_see(dec4c_ctx *c)
{
    unsigned long k, j, u, a, b, cc;
    unsigned long slot;
    unsigned long *see = c->see;
    for (k = 0; k < 0x20UL; k++) {
        for (j = 0; j < 16UL; j++) {
            unsigned long x, i;
            x = (k & 4UL) + dec4c_rom32(0x42758dUL + 4UL * j) + ((k >= 16UL) ? 8UL : 0UL);
            i = 16UL * k + j;
            see[D4C_S_O2FST + i] = ((x << 13) >> 6) & 0xFFFFFFFFUL;
            see[D4C_S_O4FST + i] = see[D4C_S_O2FST + i];
        }
        see[D4C_S_O2ESC + k] = 0x1000UL;
        see[D4C_S_O1TOT + k] = (dec4c_rom32(0x427651UL + 4UL * k) << 7) & 0xFFFFFFFFUL;
    }
    slot = 0;
    for (u = 0; u < 0x80UL; u++) {
        for (j = 0; j < 16UL; j++) {
            unsigned long t = dec4c_rom32(0x4275cdUL + 4UL * j);
            unsigned long a8 = (u >= 8UL) ? 4UL : 0UL;
            unsigned long a16 = (u >= 16UL) ? 4UL : 0UL;
            unsigned long a64 = (u >= 64UL) ? 4UL : 0UL;
            unsigned long b16 = (u >= 16UL) ? 1UL : 0UL;
            if (u < 16UL)
                see[D4C_S_O2MRU + slot] = (((t + a8) << 13) >> 6) & 0xFFFFFFFFUL;
            if (u < 32UL)
                see[D4C_S_O4MRU + slot] = (((t + a16) << 13) >> 6) & 0xFFFFFFFFUL;
            if (u < 16UL)
                see[D4C_S_O4O2 + slot] = (((t + a8) << 13) >> 6) & 0xFFFFFFFFUL;
            see[D4C_S_O1MRU + slot] = (((t + a64) << 13) >> 6) & 0xFFFFFFFFUL;
            see[D4C_S_O2O2 + slot] = (((b16 + t + a64) << 13) >> 6) & 0xFFFFFFFFUL;
            see[D4C_S_O4MPS + slot] = (((a64 + b16 + t) << 13) >> 6) & 0xFFFFFFFFUL;
            slot += 1UL;
        }
    }
    for (a = 0; a < 8UL; a++) {
        for (b = 0; b < 8UL; b++) {
            for (cc = 0; cc < 32UL; cc++) {
                unsigned long v = 0x1000UL;
                if (a > b) v = (v - dec4c_rom32(0x42760dUL + 4UL * (a - b))) & 0xFFFFFFFFUL;
                if (b > a) v = (v + dec4c_rom32(0x42760dUL + 4UL * (b - a))) & 0xFFFFFFFFUL;
                see[D4C_S_O4ESC + (a * 0x100UL + b * 0x20UL + cc)] = v;
            }
        }
        for (cc = 0; cc < 0x20UL; cc++) {
            unsigned long x = dec4c_rom32(0x42762dUL + 4UL * ((cc & 1UL) + a));
            see[D4C_S_O2BIN + (a * 0x20UL + cc)] = ((x << 13) >> 6) & 0xFFFFFFFFUL;
        }
        for (cc = 0; cc < 0x400UL; cc++) {
            unsigned long x = dec4c_rom32(0x42762dUL + 4UL * ((cc & 1UL) + a));
            see[D4C_S_O4BIN + (a * 0x400UL + cc)] = ((x << 13) >> 6) & 0xFFFFFFFFUL;
        }
        for (k = 0; k < 0x100UL; k++)
            see[D4C_ML + (a * 0x100UL + k)] = 0x800UL;
    }
}

/* Order-1 init (0x40cdd1 top): esc/inc/ba3c/ae38/mru fills. */
static void dec4c_init_model(dec4c_ctx *c)
{
    unsigned i;
    for (i = 0x209U; i <= 0x300U; i++)
        c->esc[i] = 1;
    for (i = 0; i <= 0x300U; i++) {
        c->inc_u[i] = 0x20U;
        c->inc_b[i] = 0x10U;
        c->ba3c[i] = 1UL;
        c->ae38[i] = 0x4000UL;
    }
    for (i = 0x2f9U; i <= 0x300U; i++)
        c->inc_u[i] = 0x0AU;
    for (i = 0x2e9U; i <= 0x300U; i++)
        c->inc_b[i] = 8U;
    c->inc_b[0x200] = 6U;
    for (i = 0; i <= 0x300U; i++)
        c->mru_s[i] = 0xFFU;
    dec4c_init_see(c);
}

/* Mm reset (0x412fb0) for types 5..9. */
static void dec4c_mm_new(dec4c_ctx *c)
{
    int r;
    memset(&c->mm, 0, sizeof(c->mm));
    c->mm.d734 = 2UL;
    for (r = 0; r < 4; r++) {
        c->mm.rec[r][28] = 0xFFFFFFFFUL;
        c->mm.rec[r][29] = 0xFFFFFFFFUL;
        c->mm.rec[r][30] = 0xFFFFFFFFUL;
    }
}

/* Data-type switch (shared tail of 0x40deb6/0x41013c). */
static void dec4c_set_mmtype(dec4c_ctx *c, unsigned char blo)
{
    unsigned long i;
    if (blo == c->mmtype)
        return;
    c->mmtype = blo;
    i = (unsigned long)blo;
    c->mmctx = dec4c_rom32(0x4246c8UL + 4UL * i);
    c->mmper = dec4c_rom32(0x4246a0UL + 4UL * i);
    c->g_e38c = (unsigned char)dec4c_rom8(0x424718UL + i);
    c->g_e38e = 0;
    c->channel = 0;
    /* 0x40deb6/0x41013c type switch: the predicted byte (0x45e38b) and the
     * prediction flag (0x45e38f) are cleared together with the channel
     * counter.  Leaving the stale predicted byte in place makes the next
     * non-multimedia segment add the wrong context byte to the order-1
     * symbol (arena 0x40deb6 tail: 45e38b = 0 whenever the model id
     * changes). */
    c->g_e38b = 0;
    c->g_e38f = 0;
    c->g_e38d = (blo == 5 || blo == 6) ? 1 : 0;
    if (blo > 4)
        dec4c_mm_new(c);
}

static unsigned long dec4c_fe_get(dec4c_ctx *c, unsigned long i)
{
    if (i < c->fe_n)
        return c->fe[i];
    c->bad = 1;
    return 0;
}

/* Fenwick rebuild (o1.rs fenwick_build). */
static void dec4c_fenwick_build(unsigned short *t)
{
    unsigned s = 1, i;
    while (s < 0x80U) {
        i = 256U - 2U * s;
        for (;;) {
            t[i] = (unsigned short)(t[i] + t[i - s]);
            i -= 2U * s;
            if (i == 0) break;
        }
        s *= 2U;
    }
}

/* 0x40e4ff: rescale context c. */
static void dec4c_o1_rescale(dec4c_ctx *c, unsigned long ctx)
{
    unsigned idx = (ctx >= 0x310UL) ? 0x30F : (unsigned)ctx;
    unsigned bm = c->binmode;
    unsigned long sum = 0;
    int k;
    for (k = 0; k < 256; k++) {
        unsigned nw;
        if (bm == 0) nw = (c->o1t[idx][1][k] + 1U) >> 1;
        else if (c->o1t[idx][1][k] > 0x10U) nw = c->o1t[idx][1][k] >> 1;
        else nw = 0;
        c->o1t[idx][1][k] = (unsigned short)nw;
        sum += nw;
    }
    for (k = 0; k < 255; k++)
        c->o1t[idx][0][k + 1] = c->o1t[idx][1][k];
    c->o1t[idx][0][0] = (unsigned short)sum;
    dec4c_fenwick_build(c->o1t[idx][0]);
    if (c->o1t[idx][0][0] != 0) {
        unsigned e = (c->esc[idx] + 1U) >> 1;
        unsigned long v;
        c->esc[idx] = (unsigned short)e;
        v = (unsigned long)e + (unsigned long)c->o1t[idx][0][0];
        if (ctx < (0x200U >> bm))
            v = (v * 0x60UL) & 0xFFFFFFFFUL;
        else
            v <<= 8;
        if (c->mmtype == 3 && (long)c->o1t[idx][0][0] > ((long)e << 3))
            v <<= 4;
        c->ae38[idx] = v;
    }
}

/* Stubs for the two rescalers called from new_segment file-size tweaks.
 * Correct for the zero-model first segment; full ports pending. */
static void dec4c_rescale_o2(dec4c_ctx *c);
static void dec4c_rescale_o4(dec4c_ctx *c);

#define DEC4C_O4_N 0x40000UL
static unsigned long dec4c_o4s(unsigned long i)
{
    return ((i & 0x7FFFUL) << 3) | ((i >> 15) & 7UL);
}

static void dec4c_rescale_o4(dec4c_ctx *c)
{
    unsigned long acc = 0;
    for (;;) {
        unsigned long dstv = dec4c_o4s(acc);
        unsigned char *b = c->o4 + dstv * 32UL;
        unsigned sumv = 0;
        int k;
        if (b[0] == 0) {
            acc = (acc & 0x7FFFUL) + 1UL;
            if (acc >= 0x8000UL)
                return;
            continue;
        }
        for (k = 11; k >= 0; k--) {
            unsigned v = (unsigned)(b[1 + k] >> 2);
            b[1 + k] = (unsigned char)v;
            sumv = (sumv + v) & 0xFFUL;
        }
        if (sumv == 0) {
            unsigned long dst = dstv, basev = acc;
            for (;;) {
                unsigned long src;
                basev += 0x8000UL;
                if (basev >= DEC4C_O4_N)
                    break;
                src = dec4c_o4s(basev);
                if (c->o4[src * 32UL] == 0)
                    break;
                memcpy(c->o4 + dst * 32UL, c->o4 + src * 32UL, 32UL);
                dst = src;
            }
            c->o4[dst * 32UL] = (unsigned char)(c->o4[dst * 32UL] & 0x00U);
            continue;
        }
        b[0] = (unsigned char)(((unsigned)b[0] >> 2) + 1U);
        b[0x19] = (unsigned char)(((unsigned)b[0x19] & 0x3FUL) >> 1);
        {
            unsigned v = b[0x1A] & 0x0FUL;
            b[0x1A] = (unsigned char)v;
            b[0x1B] = 0;
            if (b[1 + v] == 0)
                b[0x1A] = 0;
        }
        acc += 0x8000UL;
        if (acc >= DEC4C_O4_N) {
            acc = (acc & 0x7FFFUL) + 1UL;
            if (acc >= 0x8000UL)
                return;
        }
    }
}

/* Order-2 record access (0x423684, 20 bytes = 5 u32 LE). */
#define DEC4C_O2_LOW 0x10000UL
static void dec4c_o2r(dec4c_ctx *c, unsigned long i, unsigned char r[20])
{
    unsigned k;
    if (i < DEC4C_O2_LOW) {
        for (k = 0; k < 5; k++) {
            unsigned long v = c->o2[i * 5UL + k];
            r[k * 4UL + 0] = (unsigned char)(v & 0xFFUL);
            r[k * 4UL + 1] = (unsigned char)((v >> 8) & 0xFFUL);
            r[k * 4UL + 2] = (unsigned char)((v >> 16) & 0xFFUL);
            r[k * 4UL + 3] = (unsigned char)((v >> 24) & 0xFFUL);
        }
    } else {
        unsigned long d = ((i - DEC4C_O2_LOW) * 5UL) & 0x7FFFFUL;
        for (k = 0; k < 5; k++) {
            r[k * 4UL + 0] = c->delta[d + k][0];
            r[k * 4UL + 1] = c->delta[d + k][1];
            r[k * 4UL + 2] = c->delta[d + k][2];
            r[k * 4UL + 3] = c->delta[d + k][3];
        }
    }
}

static void dec4c_o2_put(dec4c_ctx *c, unsigned long i, const unsigned char r[20])
{
    unsigned k;
    if (i < DEC4C_O2_LOW) {
        for (k = 0; k < 5; k++)
            c->o2[i * 5UL + k] = ((unsigned long)r[k * 4UL]) |
                ((unsigned long)r[k * 4UL + 1] << 8) |
                ((unsigned long)r[k * 4UL + 2] << 16) |
                ((unsigned long)r[k * 4UL + 3] << 24);
    } else {
        unsigned long d = ((i - DEC4C_O2_LOW) * 5UL) & 0x7FFFFUL;
        for (k = 0; k < 5; k++) {
            c->delta[d + k][0] = r[k * 4UL + 0];
            c->delta[d + k][1] = r[k * 4UL + 1];
            c->delta[d + k][2] = r[k * 4UL + 2];
            c->delta[d + k][3] = r[k * 4UL + 3];
        }
    }
}

/* 0x40d58d: halve (>>2) order-2 statistics. */
static void dec4c_rescale_o2(dec4c_ctx *c)
{
    unsigned long n = 0x20000UL >> (unsigned)c->binmode;
    unsigned long i;
    for (i = 0; i < n; i++) {
        unsigned char r[20];
        unsigned sumv = 0;
        unsigned d, v;
        int k;
        dec4c_o2r(c, i, r);
        if (r[0] == 0)
            continue;
        for (k = 7; k >= 0; k--) {
            v = (unsigned)(r[1 + k] >> 2);
            r[1 + k] = (unsigned char)v;
            sumv = (sumv + v) & 0xFFUL;
        }
        d = (sumv != 0) ? ((((unsigned)r[0] >> 2) + 1U) & 0xFFUL) : 0;
        r[0] = (unsigned char)d;
        r[0x11] >>= 1;
        v = r[0x13] & 0x0FUL;
        r[0x13] = (unsigned char)v;
        r[0x12] = (unsigned char)((sumv + d) & 0xFFUL);
        if (r[1 + v] == 0)
            r[0x13] = 0;
        dec4c_o2_put(c, i, r);
    }
}

/* 0x40deb6: segment start. */
static void dec4c_new_segment(dec4c_ctx *c)
{
    unsigned bm = c->binmode;
    unsigned c_ctr;
    unsigned long next;
    c_ctr = 0;
    for (c_ctr = 0; c_ctr < 0x209UL; c_ctr++) {
        c->ba3c[c_ctr] = bm;
        c->ae38[c_ctr] = 0x4000UL;
    }
    for (c_ctr = 0; c_ctr < 0x208UL; c_ctr++) {
        c->mru_s[c_ctr] = 0xFFU;
        c->mru_n[c_ctr] = 0;
    }
    if (bm != 0) {
        unsigned i;
        for (i = 0; i < 0x80UL; i++) c->pair[i] = 1;
        if (!c->delta)
            c->delta = (unsigned char (*)[4])calloc(0x100000UL, sizeof(*c->delta));
        if (c->delta) memset(c->delta, 0, 0x100000UL * sizeof(*c->delta));
    }
    /* order-1 tables: 0..0x208 zeroed, static fallbacks loaded */
    {
        unsigned k;
        unsigned short o0[256];
        for (k = 0; k < 256; k++)
            o0[k] = (unsigned short)dec4c_rom8(0x422ba0UL + 2UL * k) |
                    ((unsigned short)dec4c_rom8(0x422ba0UL + 2UL * k + 1UL) << 8);
        for (c_ctr = 0; c_ctr < 0x209UL; c_ctr++) {
            memset(c->o1t[c_ctr][0], 0, sizeof(c->o1t[c_ctr][0]));
            memset(c->o1t[c_ctr][1], 0, sizeof(c->o1t[c_ctr][1]));
        }
        for (c_ctr = 0x301UL; c_ctr < 0x30FUL; c_ctr++) {
            unsigned n, x;
            for (x = 0; x < 256; x++)
                c->o1t[c_ctr][0][x] = o0[x];
            n = (c_ctr == 0x30EUL) ? 255UL : 256UL;
            for (x = 0; x < n; x++)
                c->o1t[c_ctr][0][x] = (unsigned short)(c->o1t[c_ctr][0][x] << 3);
            for (x = 0; x < 256; x++)
                c->o1t[c_ctr][1][x] = 8U;
        }
    }
    for (c_ctr = 0; c_ctr < 0x209UL; c_ctr++)
        c->esc[c_ctr] = 1;
    for (c_ctr = 0; c_ctr < 0x148UL; c_ctr++)
        c->mflag[c_ctr] = (unsigned short)dec4c_rom8(0x42372cUL + 2UL * c_ctr) |
                          ((unsigned short)dec4c_rom8(0x42372cUL + 2UL * c_ctr + 1UL) << 8);
    for (c_ctr = 0x209UL; c_ctr < 0x301UL; c_ctr++)
        dec4c_o1_rescale(c, c_ctr);
    dec4c_rescale_o2(c);
    c->g45c6b0 = 0;
    c->g45c6ac = 0;
    c->g45c6a0 = 0;
    c->g45c6b4 = 0x1ffUL;
    c->g45c68c = 0x187UL; /* (0x45ccd4-0x45c6b8)/4 */
    c->g45c688 = 0;
    memset(c->g6b8, 0, 0x290UL * sizeof(c->g6b8[0]));
    memset(c->recent, 0, sizeof(c->recent));
    memset(c->lastpos, 0, sizeof(c->lastpos));
    memset(c->per, 0, sizeof(c->per));
    c->g45c66c = 0;
    c->g45c674 = 1;
    c->g45c64c = 1;
    c->g45c670 = 0;
    for (c_ctr = 0; c_ctr < 256UL; c_ctr++) c->ring[c_ctr] = 0x9240UL;
    c->g45c650 = 0xffUL;
    c->g45c644 = 0x91adc0UL;
    c->avg_a[0] = c->avg_a[1] = 0;
    c->avg_b[0] = c->avg_b[1] = 0;
    c->g45c640 = 0;
    c->g45c654 = 0;
    /* Advance segment end (255 KB text / 16 KB binary) + file walk. */
    if (c->fe_on) {
        unsigned long step = 0x40000UL >> (2UL * bm);
        unsigned long end = c->seg_end + step;
        unsigned datv;
        if (end > c->out_len) end = c->out_len;
        c->seg_end = end;
        datv = (unsigned)c->file_idx;
        for (;;) {
            unsigned long e = dec4c_fe_get(c, datv);
            if (e >= c->seg_end) break;
            datv = (datv + 1UL) & 0xFFFFFFFFUL;
            {
                unsigned long e2 = dec4c_fe_get(c, datv);
                c->file_idx = (c->file_idx + 1UL) & 0xFFFFFFFFUL;
                if (e2 >= c->text_end || e2 >= c->g45d124 || c->bad) break;
            }
        }
        next = dec4c_fe_get(c, (unsigned)c->file_idx);
    } else {
        next = c->out_len;
    }
    c->seg_end = next;
    {
        unsigned long fi = c->file_idx;
        if (fi != 0) {
            unsigned long prev = dec4c_fe_get(c, (unsigned)(fi - 1UL));
            unsigned long cntv = (next - prev) & 0xFFFFFFFFUL;
            unsigned long datv = prev;
            unsigned long srcv;
            if (fi > 1UL)
                datv = (prev - dec4c_fe_get(c, (unsigned)(fi - 2UL))) & 0xFFFFFFFFUL;
            srcv = c->out_pos;
            if (datv == 0x8f4d0UL && cntv == 0x2d9a22UL) {
                dec4c_rescale_o4(c);
                dec4c_rescale_o4(c);
            }
            if (datv == 0x75a45UL && cntv == 0x68302UL)
                dec4c_rescale_o4(c);
            if ((datv == 0x17e5f8UL && cntv == 0x2cc640UL) ||
                (datv == 0x2cc640UL && cntv == 0x6af80UL)) {
                dec4c_rescale_o4(c);
                dec4c_rescale_o2(c);
            }
            if ((datv == 0x388dc2UL && cntv == 0x16dd2eUL) ||
                (datv == 0x7d4c0UL && cntv == 0x3c41eUL) ||
                (datv != 0xfb670UL && cntv == 0xfb670UL && srcv >= c->g45d124)) {
                /* First 1 MB of order-4 / 1.25 MB of order-2 cleared
                 * (0x40deb6 block). */
                {
                    unsigned long h;
                    for (h = 0; h < 0x8000UL; h++) {
                        unsigned long sbi = (((h & 0x7FFFUL) << 3) | ((h >> 15) & 7UL));
                        memset(c->o4 + sbi * 32UL, 0, 32UL);
                    }
                }
                memset(c->o2, 0, 0x50000UL * sizeof(unsigned long));
                if (srcv >= c->g45d124)
                    c->g_e395 = 0x0AU;
            }
        }
    }
    {
        unsigned long v = dec4c_rc_div(c, 10UL);
        unsigned long t = v & 0xFFUL;
        dec4c_rc_dec(c, t, t + 1UL);
        dec4c_set_mmtype(c, (unsigned char)v);
    }
}

static int dec4c_flush(dec4c_ctx *c)
{
    if (c->out_err)
        return 0;
    if (c->out_fill == 0)
        return 1;
    if (c->wcb(c->wctx, c->outflush, c->out_fill) != 0) {
        c->out_err = 1;
        c->bad = 1;
        return 0;
    }
    c->out_fill = 0;
    return 1;
}

static void dec4c_put_byte(dec4c_ctx *c, unsigned char b)
{
    if (c->out_pos < c->out_len) {
        if (!c->out_err) {
            c->outflush[c->out_fill++] = b;
            if (c->out_fill >= c->out_cap)
                dec4c_flush(c);
        }
    }
    c->out_pos = (c->out_pos + 1UL) & 0xFFFFFFFFUL;
}

/* 0x40fffd: text transform (symbols are complemented bytes). */
static void dec4c_output(dec4c_ctx *c, unsigned char alo)
{
    unsigned dlo = (unsigned char)(~alo);
    unsigned dhi;
    unsigned long src;
    unsigned bval;
    if (c->g_e39b == 1) {
        c->g_e39b = 0;
        if (c->out_pos >= c->g45d124) {
            if (dlo == 0xFEUL) {
                dec4c_put_byte(c, 0xFEU);
                return;
            }
            dec4c_put_byte(c, 0);
            dec4c_put_byte(c, 0);
            dec4c_put_byte(c, 3);
            dec4c_put_byte(c, 3);
            dec4c_put_byte(c, (unsigned char)dlo);
            return;
        }
        {
            unsigned long i = (unsigned long)((unsigned char)(c->g_e39e - 0xA6U) * 0x0BUL);
            dec4c_put_byte(c, dec4c_rom8(0x427179UL + i + dlo));
            return;
        }
    }
    if (c->out_pos >= c->g45d124 && dlo == 0xFEUL) {
        c->g_e39b = 1;
        return;
    }
    if (c->out_pos >= c->sw_pos || (dlo < 0x80UL && c->g_e39b == 0)) {
        dec4c_put_byte(c, (unsigned char)dlo);
        return;
    }
    dhi = dec4c_rom8(0x42711fUL + dlo);
    if (dhi >= 0xC8UL) {
        dec4c_put_byte(c, (unsigned char)(dlo - c->g_e39b));
        c->g_e39b = 0;
        return;
    }
    if (dhi == 0x64UL) {
        c->g_e39b = 1;
        c->g_e39e = (unsigned char)dlo;
        return;
    }
    if (dhi == 0x63UL) {
        c->g_e39b = 0x20U;
        return;
    }
    src = dec4c_rom32(0x426ac3UL + 4UL * dhi);
    bval = dec4c_rom8(0x426c53UL + dhi);
    if (c->g_e39b != 0) {
        unsigned a = (unsigned char)(~dec4c_rom8(src));
        a = (a - 0x20UL) & 0xFFUL;
        bval = (bval - 1UL) & 0xFFUL;
        src = (src + 1UL) & 0xFFFFFFFFUL;
        dec4c_put_byte(c, (unsigned char)a);
        c->g_e39b = 0;
    }
    for (;;) {
        unsigned a = (unsigned char)(~dec4c_rom8(src));
        src = (src + 1UL) & 0xFFFFFFFFUL;
        dec4c_put_byte(c, (unsigned char)a);
        bval = (bval - 1UL) & 0xFFUL;
        if (bval == 0)
            break;
    }
}


/* ROM tables for mm predictors (0x424d3c,54,5554,5954,5d54,6154,6554) */
static const unsigned long dec4c_t424D3C[6] = {

    127, 127, 127, 127, 7, 3,

};
static const unsigned long dec4c_t424D54[512] = {

    0, 1, 2, 2, 2, 3, 3, 3,

    3, 3, 4, 4, 4, 4, 4, 4,

    4, 5, 5, 5, 5, 5, 5, 5,

    5, 5, 6, 6, 6, 6, 6, 6,

    6, 6, 6, 6, 6, 7, 7, 7,

    7, 7, 7, 7, 7, 7, 7, 7,

    7, 7, 8, 8, 8, 8, 8, 8,

    8, 8, 8, 8, 8, 8, 8, 8,

    8, 9, 9, 9, 9, 9, 9, 9,

    9, 9, 9, 9, 9, 9, 9, 9,

    9, 9, 10, 10, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 13, 13, 13, 13, 13, 13, 13,

    13, 13, 13, 13, 13, 13, 13, 13,

    13, 13, 13, 13, 13, 13, 13, 13,

    13, 13, 14, 14, 14, 14, 14, 14,

    14, 14, 14, 14, 14, 14, 14, 14,

    14, 14, 14, 14, 14, 14, 14, 14,

    14, 14, 14, 14, 14, 15, 15, 15,

    15, 15, 15, 15, 15, 15, 15, 15,

    15, 15, 15, 15, 15, 15, 15, 15,

    15, 15, 15, 15, 15, 15, 15, 15,

    15, 15, 16, 16, 16, 16, 16, 16,

    16, 16, 16, 16, 16, 16, 16, 16,

    16, 16, 16, 16, 16, 16, 16, 16,

    16, 16, 16, 16, 16, 16, 16, 16,

    16, 16, 16, 16, 16, 16, 16, 16,

    16, 16, 16, 16, 16, 16, 16, 16,

    16, 16, 16, 16, 16, 16, 16, 16,

    16, 16, 16, 16, 16, 16, 16, 15,

    15, 15, 15, 15, 15, 15, 15, 15,

    15, 15, 15, 15, 15, 15, 15, 15,

    15, 15, 15, 15, 15, 15, 15, 15,

    15, 15, 15, 15, 14, 14, 14, 14,

    14, 14, 14, 14, 14, 14, 14, 14,

    14, 14, 14, 14, 14, 14, 14, 14,

    14, 14, 14, 14, 14, 14, 14, 13,

    13, 13, 13, 13, 13, 13, 13, 13,

    13, 13, 13, 13, 13, 13, 13, 13,

    13, 13, 13, 13, 13, 13, 13, 13,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 10, 10, 9,

    9, 9, 9, 9, 9, 9, 9, 9,

    9, 9, 9, 9, 9, 9, 9, 9,

    8, 8, 8, 8, 8, 8, 8, 8,

    8, 8, 8, 8, 8, 8, 8, 7,

    7, 7, 7, 7, 7, 7, 7, 7,

    7, 7, 7, 7, 6, 6, 6, 6,

    6, 6, 6, 6, 6, 6, 6, 5,

    5, 5, 5, 5, 5, 5, 5, 5,

    4, 4, 4, 4, 4, 4, 4, 3,

    3, 3, 3, 3, 2, 2, 2, 1,

};
static const unsigned long dec4c_t425554[256] = {

    0, 0, 0, 0, 1, 1, 1, 1,

    2, 2, 2, 2, 3, 3, 3, 3,

    4, 4, 4, 4, 5, 5, 5, 5,

    6, 6, 6, 6, 7, 7, 7, 7,

    8, 8, 8, 8, 9, 9, 9, 9,

    10, 10, 10, 10, 11, 11, 11, 11,

    12, 12, 12, 12, 13, 13, 13, 13,

    14, 14, 14, 14, 15, 15, 15, 15,

    16, 16, 16, 16, 17, 17, 17, 17,

    18, 18, 18, 18, 19, 19, 19, 19,

    20, 20, 20, 20, 21, 21, 21, 21,

    22, 22, 22, 22, 23, 23, 23, 23,

    24, 24, 24, 24, 25, 25, 25, 25,

    26, 26, 26, 26, 27, 27, 27, 27,

    28, 28, 28, 28, 29, 29, 29, 29,

    30, 30, 30, 30, 31, 31, 31, 31,

    32, 31, 31, 31, 31, 30, 30, 30,

    30, 29, 29, 29, 29, 28, 28, 28,

    28, 27, 27, 27, 27, 26, 26, 26,

    26, 25, 25, 25, 25, 24, 24, 24,

    24, 23, 23, 23, 23, 22, 22, 22,

    22, 21, 21, 21, 21, 20, 20, 20,

    20, 19, 19, 19, 19, 18, 18, 18,

    18, 17, 17, 17, 17, 16, 16, 16,

    16, 15, 15, 15, 15, 14, 14, 14,

    14, 13, 13, 13, 13, 12, 12, 12,

    12, 11, 11, 11, 11, 10, 10, 10,

    10, 9, 9, 9, 9, 8, 8, 8,

    8, 7, 7, 7, 7, 6, 6, 6,

    6, 5, 5, 5, 5, 4, 4, 4,

    4, 3, 3, 3, 3, 2, 2, 2,

    2, 1, 1, 1, 1, 0, 0, 0,

};
static const unsigned long dec4c_t425D54[256] = {

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    2, 2, 2, 2, 2, 2, 2, 2,

    2, 2, 2, 2, 2, 2, 2, 2,

    3, 3, 3, 3, 3, 3, 3, 3,

    3, 3, 3, 3, 3, 3, 3, 3,

    4, 4, 4, 4, 4, 4, 4, 4,

    4, 4, 4, 4, 4, 4, 4, 4,

    5, 5, 5, 5, 5, 5, 5, 5,

    5, 5, 5, 5, 5, 5, 5, 5,

    6, 6, 6, 6, 6, 6, 6, 6,

    6, 6, 6, 6, 6, 6, 6, 6,

    7, 7, 7, 7, 7, 7, 7, 7,

    7, 7, 7, 7, 7, 7, 7, 7,

    8, 7, 7, 7, 7, 7, 7, 7,

    7, 7, 7, 7, 7, 7, 7, 7,

    7, 6, 6, 6, 6, 6, 6, 6,

    6, 6, 6, 6, 6, 6, 6, 6,

    6, 5, 5, 5, 5, 5, 5, 5,

    5, 5, 5, 5, 5, 5, 5, 5,

    5, 4, 4, 4, 4, 4, 4, 4,

    4, 4, 4, 4, 4, 4, 4, 4,

    4, 3, 3, 3, 3, 3, 3, 3,

    3, 3, 3, 3, 3, 3, 3, 3,

    3, 2, 2, 2, 2, 2, 2, 2,

    2, 2, 2, 2, 2, 2, 2, 2,

    2, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

};
static const unsigned long dec4c_t426554[256] = {

    0, 1, 2, 3, 4, 5, 5, 6,

    6, 6, 7, 7, 7, 7, 7, 8,

    8, 8, 8, 8, 8, 8, 8, 9,

    9, 9, 9, 9, 9, 9, 9, 9,

    9, 9, 9, 9, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 10, 10, 10,

    10, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 12, 12,

    12, 12, 12, 12, 12, 12, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    11, 11, 11, 11, 11, 11, 11, 11,

    10, 10, 10, 10, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 10, 10, 10,

    10, 10, 10, 10, 10, 9, 9, 9,

    9, 9, 9, 9, 9, 9, 9, 9,

    9, 9, 8, 8, 8, 8, 8, 8,

    8, 8, 7, 7, 7, 7, 7, 6,

    6, 6, 5, 5, 4, 3, 2, 1,

};
static const unsigned long dec4c_t425954[256] = {

    0, 0, 0, 0, 0, 0, 0, 0,

    1, 1, 1, 1, 1, 1, 1, 1,

    2, 2, 2, 2, 2, 2, 2, 2,

    3, 3, 3, 3, 3, 3, 3, 3,

    4, 4, 4, 4, 4, 4, 4, 4,

    5, 5, 5, 5, 5, 5, 5, 5,

    6, 6, 6, 6, 6, 6, 6, 6,

    7, 7, 7, 7, 7, 7, 7, 7,

    8, 8, 8, 8, 8, 8, 8, 8,

    9, 9, 9, 9, 9, 9, 9, 9,

    10, 10, 10, 10, 10, 10, 10, 10,

    11, 11, 11, 11, 11, 11, 11, 11,

    12, 12, 12, 12, 12, 12, 12, 12,

    13, 13, 13, 13, 13, 13, 13, 13,

    14, 14, 14, 14, 14, 14, 14, 14,

    15, 15, 15, 15, 15, 15, 15, 15,

    16, 15, 15, 15, 15, 15, 15, 15,

    15, 14, 14, 14, 14, 14, 14, 14,

    14, 13, 13, 13, 13, 13, 13, 13,

    13, 12, 12, 12, 12, 12, 12, 12,

    12, 11, 11, 11, 11, 11, 11, 11,

    11, 10, 10, 10, 10, 10, 10, 10,

    10, 9, 9, 9, 9, 9, 9, 9,

    9, 8, 8, 8, 8, 8, 8, 8,

    8, 7, 7, 7, 7, 7, 7, 7,

    7, 6, 6, 6, 6, 6, 6, 6,

    6, 5, 5, 5, 5, 5, 5, 5,

    5, 4, 4, 4, 4, 4, 4, 4,

    4, 3, 3, 3, 3, 3, 3, 3,

    3, 2, 2, 2, 2, 2, 2, 2,

    2, 1, 1, 1, 1, 1, 1, 1,

    1, 0, 0, 0, 0, 0, 0, 0,

};
static const unsigned long dec4c_t426154[256] = {

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    2, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1,

    1, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0,

};

static unsigned long dec4c_abs12(unsigned long x)
{
    long v = (long)(x & 0xFFFUL);
    if (v & 0x800L) v |= ~0xFFFL;
    return (unsigned long)(v < 0 ? -v : v);
}
static unsigned long dec4c_sdiv16(unsigned long s)
{
    return (unsigned long)(((long)s) / 16L);
}
static void dec4c_mm_update_errs(unsigned long r[31], unsigned long acc, unsigned long last)
{
    unsigned long hs[5];
    int i;
    r[12] += dec4c_abs12(acc);
    hs[0]=r[0]; hs[1]=r[1]; hs[2]=r[2]; hs[3]=r[3]; hs[4]=last;
    for (i=0;i<5;i++) {
        r[13+2*i] += dec4c_abs12(acc - hs[i]);
        r[14+2*i] += dec4c_abs12(hs[i] + acc);
    }
}
static void dec4c_mm_adapt_weights(unsigned long r[31])
{
    unsigned long best = r[12];
    unsigned sel = 0;
    unsigned k, wk;
    long d = 0;
    r[12] = 0;
    for (k=1;k<=10;k++) {
        unsigned long v = r[12+k];
        if (v < best) { best = v; sel = k; }
        r[12+k] = 0;
    }
    #define D4W(i) (5+(i))
    if (sel==1 && (long)r[D4W(0)] > -0x20) { wk=0; d=-1; }
    else if (sel==2 && (long)r[D4W(0)] < 0x20) { wk=0; d=1; }
    else if (sel==3 && (long)r[D4W(1)] > -0x20) { wk=1; d=-1; }
    else if (sel==4 && (long)r[D4W(1)] < 0x20) { wk=1; d=1; }
    else if (sel==5 && (long)r[D4W(2)] > -0x20) { wk=2; d=-1; }
    else if (sel==6 && (long)r[D4W(2)] < 0x20) { wk=2; d=1; }
    else if (sel==7 && (long)r[D4W(3)] > -0x20) { wk=3; d=-1; }
    else if (sel==8 && (long)r[D4W(3)] < 0x20) { wk=3; d=1; }
    else if (sel==9 && (long)r[D4W(3)] > -0x20) { wk=4; d=-1; } /* original tests w3, changes w4 */
    else if (sel==10 && (long)r[D4W(3)] < 0x20) { wk=4; d=1; }
    else return;
    r[D4W(wk)] = (unsigned long)((long)r[D4W(wk)] + d);
    #undef D4W
}
/* 0x412ffa */
static void dec4c_mm_predict(dec4c_ctx *c)
{
    unsigned long n = c->nbytes;
    unsigned ty = c->mmtype;
    unsigned long next_ctx;
    if (ty < 7) {
        unsigned e1 = (unsigned)((n - c->mm.d730) & 0x7FUL);
        unsigned long *r = c->mm.rec[0];
        unsigned long eaxv = c->mm.d738;
        unsigned long s;
        unsigned char pred_byte;
        unsigned sel;
        c->mm.pred[0] = 0;
        c->mm.pred[1] = eaxv;
        c->mm.pred[2] = eaxv + eaxv - c->mm.d73c;
        r[4] = c->mm.hist[e1];
        r[3] = r[2]; r[2] = r[1]; r[1] = r[11] - r[0]; r[0] = r[11];
        s = r[5]*r[0] + (r[10]<<4) + r[6]*r[1] + r[7]*r[2] + r[8]*r[3] + r[9]*r[4];
        c->mm.pred[3] = dec4c_sdiv16(s);
        c->mm.pred[4] = c->mm.hist[e1];
        c->mm.pred[5] = c->mm.hist[(unsigned)((n - c->mm.d734) & 0x7FUL)];
        sel = (unsigned)c->mm.sel;
        pred_byte = (unsigned char)(c->mm.pred[sel < 6 ? sel : 0] & 0xFFUL);
        if (sel >= 4) pred_byte = (unsigned char)(pred_byte + (unsigned char)c->mm.d738);
        c->g_e38b = pred_byte;
        next_ctx = (ty == 5) ? 0x2E9UL : 0x2EDUL;
        if (ty == 5) c->g_e38b = (unsigned char)(c->g_e38b + 0x80U);
        if (r[28] == 0) next_ctx += 1;
        else if (r[29] == 0) next_ctx += 2;
        else if (r[30] == 0) next_ctx += 3;
    } else {
        unsigned channel = c->channel & 3U;
        unsigned long *r = c->mm.rec[channel];
        unsigned long s;
        unsigned char pred_byte;
        r[3]=r[2]; r[2]=r[1]; r[1]=r[11]-r[0]; r[0]=r[11];
        s = r[5]*r[0] + (r[10]<<4) + r[6]*r[1] + r[7]*r[2] + r[8]*r[3] + r[9]*c->mm.d740;
        c->mm.pred[0] = dec4c_sdiv16(s);
        pred_byte = (r[23] > r[24]) ? (unsigned char)r[10] : (unsigned char)c->mm.pred[0];
        c->g_e38b = pred_byte;
        if (ty == 7) {
            next_ctx = (unsigned long)channel + 0x2F1UL;
            c->g_e38b = (unsigned char)(c->g_e38b + 0x80U);
            if (r[28]==0) next_ctx += 2; else if (r[29]==0) next_ctx += 4; else if (r[30]==0) next_ctx += 6;
        } else if (ty == 8) {
            next_ctx = (r[28]!=0) ? (unsigned long)channel : 2UL;
            next_ctx += 0x2F9UL;
        } else {
            next_ctx = (r[28]!=0) ? (unsigned long)channel : 4UL;
            next_ctx += 0x2FCUL;
        }
        c->g_e38e = c->mm.sgn[channel];
    }
    c->mmctx = next_ctx;
}
/* 0x413347 */
static void dec4c_mm_update(dec4c_ctx *c, unsigned char sym, unsigned char delta)
{
    unsigned long n = c->nbytes;
    unsigned ty = c->mmtype;
    /* Arena 0x413347 gates the multimedia update on
     *   flag_45e391 || 45ae14 >= M32(0x45ade4),
     * and 0x45ade4 is written exactly once (to 0) at init and never
     * afterwards, so the gate is always true and the "backoff" path of
     * 0x412ffa (which restores the predicted byte from the pred_cache ring
     * at 0x45e0900) is unreachable.  The check is kept for fidelity. */
    if (c->g_e391 == 0 && c->g45ae14 < c->imgq_n)
        return;
    if (ty < 7) {
        unsigned long cntv = (ty == 6) ? (unsigned long)(long)(signed char)sym : ((unsigned long)sym - 0x80UL);
        unsigned long e14 = cntv - c->mm.d738;
        unsigned long eaxv = (unsigned long)(((long)((signed char)((unsigned char)c->mm.pred[3] - (unsigned char)cntv))) << 4);
        unsigned long *r = c->mm.rec[0];
        unsigned long last = r[4];
        unsigned char pred_byte;
        unsigned sel, ni;
        int k;
        dec4c_mm_update_errs(r, eaxv, last);
        pred_byte = (unsigned char)cntv - (unsigned char)r[10];
        r[27] += 1;
        r[10] = cntv;
        r[11] = (unsigned long)(long)(signed char)pred_byte;
        if ((r[27] & 0xFUL) == 0) dec4c_mm_adapt_weights(r);
        c->mm.hist[n & 0x7FUL] = e14;
        sel = (unsigned)c->mm.sel;
        if ((dec4c_t424D3C[sel & 7U] & n) == 0) {
            unsigned long best = 0xFFFFUL;
            unsigned long ebxv;
            c->mm.d730 = 2; c->mm.d734 = 2;
            for (ebxv = 2; ebxv < 0x7CUL; ebxv++) {
                unsigned long d1 = c->mm.hist[(n-1UL)&0x7FUL] - c->mm.hist[(n-1UL-ebxv)&0x7FUL];
                unsigned long d0 = c->mm.hist[n&0x7FUL] - c->mm.hist[(n-ebxv)&0x7FUL];
                unsigned long sv = dec4c_abs12(d0) + dec4c_abs12(d1);
                unsigned long d2 = c->mm.hist[(n-2UL)&0x7FUL] - c->mm.hist[(n-2UL-ebxv)&0x7FUL];
                unsigned long accv;
                sv += dec4c_abs12(d2);
                accv = ((c->mm.acc[ebxv & 0x7FUL] * 13UL) >> 4) + sv;
                c->mm.acc[ebxv & 0x7FUL] = accv;
                if (accv < c->mm.acc[c->mm.d730 & 0x7FUL]) c->mm.d730 = ebxv;
                if (sv < best) { best = sv; c->mm.d734 = ebxv; }
            }
        }
        {
            unsigned long errs[6];
            errs[0]=cntv-c->mm.pred[0]; errs[1]=cntv-c->mm.pred[1]; errs[2]=cntv-c->mm.pred[2];
            errs[3]=cntv-c->mm.pred[3]; errs[4]=e14-c->mm.pred[4]; errs[5]=e14-c->mm.pred[5];
            c->mm.sel = 0;
            ni = (unsigned)(n & 0xFFUL);
            for (k=0;k<6;k++) {
                unsigned long old = c->mm.errs[k][ni];
                unsigned long vv = dec4c_t424D54[errs[k] & 0x1FFUL];
                c->mm.errs[k][0x100] = c->mm.errs[k][0x100] - old + vv;
                c->mm.errs[k][ni] = vv;
                if ((long)c->mm.errs[k][0x100] < (long)c->mm.errs[c->mm.sel][0x100])
                    c->mm.sel = (unsigned long)k;
            }
        }
        c->mm.d73c = c->mm.d738;
        c->mm.d738 = cntv;
        r = c->mm.rec[0];
        r[28] = (r[28]<<8) + (unsigned long)delta;
        r[29] = (r[29]<<8) + dec4c_t425554[delta];
        r[30] = (r[30]<<8) + dec4c_t425D54[delta];
    } else {
        unsigned channel = c->channel & 3U;
        unsigned long *r = c->mm.rec[channel];
        unsigned long cntv = (ty >= 8) ? (unsigned long)(long)(signed char)sym : ((unsigned long)sym - 0x80UL);
        unsigned char p = (unsigned char)c->mm.pred[0];
        unsigned long eaxv = (unsigned long)(((long)((signed char)(p - (unsigned char)cntv))) << 4);
        unsigned long l = c->mm.d740;
        unsigned char a;
        c->mm.sgn[channel] = c->g_e38e;
        dec4c_mm_update_errs(r, eaxv, l);
        /* The original masks the difference to a byte before indexing the
         * error curve (0x426554); without the mask the index can go
         * negative and reads outside the table. */
        a = (unsigned char)(p - (unsigned char)cntv);
        r[23] += dec4c_t426554[a];
        a = (unsigned char)((unsigned char)r[10] - (unsigned char)cntv);
        r[24] += dec4c_t426554[a];
        r[11] = (unsigned long)(long)(signed char)((unsigned char)cntv - (unsigned char)r[10]);
        c->mm.d740 = r[11];
        r[10] = cntv;
        r[27] += 1;
        if ((r[27] & 0xFUL) == 0) {
            dec4c_mm_adapt_weights(r);
            if ((r[27] & 0xFFUL) == 0) {
                r[23] -= r[25]; r[25] = r[23];
                r[24] -= r[26]; r[26] = r[24];
            }
        }
        if (ty == 9) {
            unsigned long tmp = c->mm.d740; c->mm.d740 = c->mm.d744; c->mm.d744 = tmp;
        }
        r = c->mm.rec[channel];
        r[28] = (r[28]<<8) + (unsigned long)delta;
        if (ty == 7) {
            r[29] = (r[29]<<8) + dec4c_t425954[delta];
            r[30] = (r[30]<<8) + dec4c_t426154[delta];
        }
    }
}


/* 0x40f39a: per-symbol context/period/predictor bookkeeping. */
static void dec4c_after_symbol(dec4c_ctx *c, unsigned char sym, int dlo)
{
    int i;
    unsigned clv, o;
    for (i = 0; i < 4; i++)
        c->x698[c->x698p[i] & 0x3FFFFUL] = sym;
    if (dlo) {
        clv = (unsigned)(c->nbytes & 1UL);
        o = clv ^ 1UL;
        c->dd6a[clv] = (unsigned char)((c->dd6a[clv] + c->dd6a[clv]) & 0x0FUL);
        c->dd6c[clv] = (unsigned char)((c->dd6c[clv] + c->dd6c[clv]) & 0x0FUL);
        c->g_e3a0 = 0;
        if (sym >= 0x80U) {
            if (sym == 0xFFU) {
                c->dd6c[clv] = (unsigned char)(c->dd6c[clv] + 1U);
                if (c->dd6a[o] == 0x0FU && c->dd6c[clv] == 0x0FU)
                    c->g_e3a0 = 2;
            } else if (sym <= 0xDFU || (sym >= 0xF2U && sym <= 0xF6U)) {
                c->dd6a[clv] = (unsigned char)(c->dd6a[clv] + 1U);
                if (c->dd6c[o] == 0x0FU && c->dd6a[clv] == 0x0FU)
                    c->g_e3a0 = 1;
            }
        }
    }
    if (sym != 0 && sym != 0xFFU) {
        unsigned long pos = c->pos;
        unsigned long acc = (pos - c->lastpos[sym]) & c->dictmask;
        if (acc >= 0x100UL) {
            acc = (pos - c->lastpos[sym - 1U]) & c->dictmask;
            if (acc >= 0x100UL)
                acc = (pos - c->lastpos[sym + 1U]) & c->dictmask;
        }
        if (acc != 0 && acc < 0x100UL) {
            c->per[acc] = (c->per[acc] + 1UL) & 0xFFFFFFFFUL;
            if (acc != c->g45c674) {
                unsigned long pd = c->g45c674;
                if (sym == dec4c_hist(c, pd))
                    c->per[pd & 0xFFUL] = (c->per[pd & 0xFFUL] + 1UL) & 0xFFFFFFFFUL;
                else
                    c->g45c66c = (c->g45c66c + 2UL) & 0xFFFFFFFFUL;
                if (acc == c->g45c670) {
                    unsigned long datv = c->g45c674;
                    if (c->per[acc] >= c->per[datv & 0xFFUL]) {
                        c->g45c670 = datv;
                        c->g45c674 = acc;
                        if (acc > 1UL)
                            c->g45c64c = acc;
                    }
                } else {
                    c->g45c670 = acc;
                }
            }
        }
        c->g45c66c = (c->g45c66c + 1UL) & 0xFFFFFFFFUL;
        if (c->g45c66c >= 0x28UL) {
            unsigned e;
            for (i = 0; i < 256; i++)
                c->per[i] >>= 1;
            e = c->g45c674;
            c->g45c66c = 0;
            c->per[e & 0xFFUL] = (c->per[e & 0xFFUL] + 1UL) & 0xFFFFFFFFUL;
            if (e == 1UL && c->per[1] < 4UL)
                c->per[1] = (c->per[1] + e) & 0xFFFFFFFFUL;
        }
        c->lastpos[sym] = pos;
    }
    if (c->binmode != 0)
        c->recent[sym] = (c->pos + 0x40UL) & 0xFFFFFFFFUL;
    {
        unsigned ty = c->mmtype;
        if (ty != 0) {
            unsigned alv = (unsigned)(sym - c->g_e38b) & 0xFFUL;
            if (c->g_e38e != 0)
                alv = (-(long)alv) & 0xFFUL;
            if (alv >= 0x80UL)
                c->g_e38e = (c->g_e38e == 0) ? 1 : 0;
            if (ty == 1) {
                unsigned e = (c->mmctx - 0x209UL) & 0x0FUL;
                c->mmctx = ((dec4c_rom8(0x424b2cUL + e) << 4) + dec4c_rom8(0x424b3cUL + alv) + 0x209UL) & 0xFFFFFFFFUL;
            } else {
                unsigned datv = (c->channel << 4) & 0xFFFFFFFFUL;
                if (ty == 2) {
                    c->mmctx = (dec4c_rom8(0x424b3cUL + alv) + datv + 0x229UL) & 0xFFFFFFFFUL;
                } else if (ty == 3) {
                    unsigned cntv;
                    if (c->g_e389 != 0) {
                        unsigned d = (sym - (c->c0 >> 16)) & 0xFFFFFFFFUL;
                        alv = d & 0xFFUL;
                        {
                            unsigned ahv = c->g_e38f;
                            c->g_e38e = (unsigned char)ahv;
                            if (ahv != 0)
                                alv = (-(long)alv) & 0xFFUL;
                            if (alv >= 0x80UL)
                                c->g_e38e = (c->g_e38e == 0) ? 1 : 0;
                        }
                    }
                    cntv = c->g_e38a & 7UL;
                    if (sym == c->ty3[cntv])
                        c->cnt3[cntv] = (c->cnt3[cntv] + 1UL) & 0xFFFFFFFFUL;
                    else
                        c->cnt3[cntv] >>= 1;
                    {
                        unsigned d = dec4c_rom8(0x424c3cUL + alv);
                        c->mmctx = (((c->channel << 5) & 0xFFFFFFFFUL) + d + 0x249UL) & 0xFFFFFFFFUL;
                    }
                } else if (ty == 4) {
                    c->mmctx = (dec4c_rom8(0x424b3cUL + alv) + datv + 0x2a9UL) & 0xFFFFFFFFUL;
                } else {
                    dec4c_mm_update(c, sym, (unsigned char)alv);
                }
            }
        }
    }
    c->nbytes = (c->nbytes + 1UL) & 0xFFFFFFFFUL;
    c->g45c6b0 = (c->g45c6b0 + 1UL) & 0xFFFFFFFFUL;
    {
        unsigned long c0 = c->c0, c1 = c->c1, c2 = c->c2;
        if (c->binmode == 0 && sym >= 4U && sym <= 6U) {
            if ((c0 & 0xFFUL) != 0xDFUL) {
                c->c2 = ((c2 << 8) + (c1 >> 24)) & 0xFFFFFFFFUL;
                c->c1 = ((c1 << 8) + (c0 >> 24)) & 0xFFFFFFFFUL;
                c->c0 = ((c0 << 8) + 0xDFUL) & 0xFFFFFFFFUL;
            }
        } else {
            c->c2 = ((c2 << 8) + (c1 >> 24)) & 0xFFFFFFFFUL;
            c->c1 = ((c1 << 8) + (c0 >> 24)) & 0xFFFFFFFFUL;
            c->c0 = ((c0 << 8) + (unsigned)sym) & 0xFFFFFFFFUL;
        }
    }
    if (c->mmtype != 0) {
        unsigned long n = c->nbytes;
        unsigned long d = c->mmper;
        unsigned long rem;
        if (d == 0) {
            c->bad = 1;
            rem = 0;
        } else {
            rem = n % d;
        }
        {
            unsigned shift = c->g_e38c;
            unsigned long srcv = c->channel;
            c->channel = rem;
            c->g_e38b = (unsigned char)((c->c0 >> shift) & 0xFFUL);
            if (c->mmtype == 3) {
                unsigned dl3 = (unsigned)(n % 6UL);
                unsigned long c0 = c->c0;
                unsigned cntv, big;
                c->g_e38a = (unsigned char)dl3;
                cntv = ((c0 & 0xFFUL) - (c0 >> 24) + (unsigned long)c->g_e38b) & 0xFFFFFFFFUL;
                big = (c->cnt3[dl3] > 8UL) ? 1U : 0U;
                c->ty3[dl3] = (unsigned char)cntv;
                {
                    unsigned alv = (big && c->g45ae18 < 5UL) ? 1U : 0U;
                    c->g_e389 = (unsigned char)alv;
                    if (alv) {
                        c->g_e38f = c->g_e38e;
                        c->mmctx = ((srcv << 5) + 0x259UL) & 0xFFFFFFFFUL;
                        c->g_e38e = 0;
                        c->g_e38b = c->ty3[dl3];
                    }
                }
                if (big)
                    c->g45ae18 = (c->g45ae18 + 1UL) & 0xFFFFFFFFUL;
                else
                    c->g45ae18 = 0;
            } else if (c->mmtype >= 5U) {
                dec4c_mm_predict(c);
            }
        }
    }
    {
        unsigned long c0 = c->c0, c1 = c->c1;
        unsigned long datv = c0 >> 8;
        c->x698p[0] = (((c0 >> 16) & 0xFF00UL) + (c1 >> 24)) & 0xFFFFFFFFUL;
        c->x698p[1] = ((datv & 0xFFFFUL) + 0x10000UL) & 0xFFFFFFFFUL;
        c->x698p[2] = ((datv & 0xFF00UL) + 0x20000UL + (c0 & 0xFFUL)) & 0xFFFFFFFFUL;
        c->x698p[3] = (((c0 << 8) & 0xFF00UL) + 0x30000UL + (c1 & 0xFFUL)) & 0xFFFFFFFFUL;
    }
}

static unsigned long dec4c_dv(unsigned long a, unsigned long b)
{
    if (b == 0) return 0;
    return a / b;
}

/* 0x40f9d0: match model; returns true when bytes were copied. */
static int dec4c_match_model(dec4c_ctx *c)
{
    unsigned long c0 = c->c0, c1 = c->c1, c2 = c->c2;
    unsigned long mask = c->dictmask;
    unsigned long pos = c->pos;
    unsigned long h0 = ((c0 >> 13) ^ c0) & 0x3FFFFUL;
    unsigned long p1 = 2UL * h0;
    unsigned long h2 = ((((c1 >> 15) ^ c1) << 6) ^ (((c0 >> 15) ^ c0) << 4) ^ ((c2 >> 15) ^ c2)) & 0x3FFFFUL;
    unsigned ctx = 0;
    int flag = 0;
    unsigned best = 0;
    unsigned chk = c->mchk[h0];
    unsigned long dstv = 0;
    unsigned long cur = (pos < 0x40UL) ? pos + c->dictsize : pos;
    int goto_fca4;
    {
        static const unsigned slots[2] = {1UL, 0UL};
        int slot;
        for (slot = 0; slot < 2; slot++) {
            unsigned long raw = c->mh2[2UL * h2 + slots[slot]];
            unsigned long basev = raw;
            if (basev != 0xFFFFFFFFUL && ((basev - pos) & mask) >= 0x140UL) {
                if (basev < 0x40UL)
                    basev = (basev + c->dictsize) & 0xFFFFFFFFUL;
                if (dec4c_tb(c, (cur - 0xCUL) & 0xFFFFFFFFUL) == dec4c_tb(c, (basev - 0xCUL) & 0xFFFFFFFFUL)) {
                    unsigned len = dec4c_match_len(c, (cur - 1UL) & 0xFFFFFFFFUL, (basev - 1UL) & 0xFFFFFFFFUL);
                    if (len >= best) {
                        best = len;
                        dstv = raw;
                    }
                }
            }
        }
    }
    c->mh2[2UL * h2 + 1UL] = c->mh2[2UL * h2];
    c->mh2[2UL * h2] = pos;
    c->mchk_p = h0;
    if (c->g45d114 != 0UL) {
        dstv = (pos - c->g45d130) & mask;
        ctx = 0x50U + ((c->g45d114 > 2UL) ? 1U : 0U);
        flag = 1;
        goto_fca4 = 1;
    } else {
        unsigned thr = (c->g45d134 < 0x10UL) ? 0x30U : 0x0CU;
        if ((long)best >= (long)thr) {
            ctx = 0x15U;
            goto_fca4 = 1;
        } else {
            goto_fca4 = 0;
        }
    }
    if (!goto_fca4) {
        int skip = 0;
        if (ctx == 0) {
            dstv = (pos - c->g45d130) & mask;
            if (((dstv - pos) & mask) >= 0x104UL) {
                unsigned long a = (dstv - 4UL) & mask;
                unsigned long v = (unsigned long)dec4c_tb(c, (a + 3UL) & 0xFFFFFFFFUL) |
                    ((unsigned long)dec4c_tb(c, (a + 2UL) & 0xFFFFFFFFUL) << 8) |
                    ((unsigned long)dec4c_tb(c, (a + 1UL) & 0xFFFFFFFFUL) << 16) |
                    ((unsigned long)dec4c_tb(c, a) << 24);
                if ((v & 0xFFFFUL) == (c0 & 0xFFFFUL) ||
                    (v & 0xFFFF00FFUL) == (c0 & 0xFFFF00FFUL) ||
                    (v & 0xFFFFFF00UL) == (c0 & 0xFFFFFF00UL)) {
                    ctx = 0x0BU;
                    flag = 1;
                    skip = 1;
                }
            }
        }
        if (!skip && ctx == 0) {
            int go = 1;
            if (chk >= 0x40U && c->mlen[h0] == 0)
                go = 0;
            if (go) {
                if (c->mh1[p1 + 1UL] == c0) {
                    dstv = c->mh1[p1];
                    ctx = (dstv != 0xFFFFFFFFUL) ? 1U : 0U;
                } else if (dstv != 0xFFFFFFFFUL) {
                    ctx = 0x1FU;
                }
            }
        }
    }
    c->mh1[p1] = pos;
    c->mh1[p1 + 1UL] = c0;
    if (ctx == 0 || ((dstv - pos) & mask) < 0x100UL) {
        c->g45d114 = 0;
        return 0;
    }
    {
        unsigned long mlen_a = h0;
        unsigned long dist, d138, node, tab, lb, sel;
        unsigned len;
        unsigned long rem;
        if (c->g45d114 == 0UL) {
            unsigned long add = (c->mlen[mlen_a] != 0) ? 0xFFFFFFFFUL : 0x27UL;
            unsigned long e = ((unsigned long)c->g_e3a2 + add) & 0xFFFFFFFFUL;
            ctx = ((unsigned)ctx + (unsigned)e) & 0xFFU;
        }
        if (c->pend != 0xFFFFFFFFUL) {
            if (dec4c_tb(c, c->pend) == dec4c_tb(c, dstv)) {
                dec4c_match_flag_update(c, ctx, 2);
                c->mlen[mlen_a] = 0;
                c->g45d114 = 0;
                return 0;
            }
        }
        if (dec4c_match_flag(c, ctx) != 1U) {
            c->g45d134 = (c->g45d134 + 1UL) & 0xFFFFFFFFUL;
            c->mlen[mlen_a] = 0;
            c->g45d114 = 0;
            return 0;
        }
        node = 0x0EUL;
        dist = (pos - dstv) & mask;
        d138 = c->g45d138;
        if (d138 == 0UL) {
            c->bad = 1;
            return 0;
        }
        if (flag) {
            unsigned long q = dec4c_dv((c->g45d120 << 2) & 0xFFFFFFFFUL, d138);
            lb = dec4c_rom8(0x424280UL + q);
        } else if (c->g45d104 > d138) {
            unsigned long q = dec4c_dv((c->g45d104 << 2) & 0xFFFFFFFFUL, d138);
            lb = dec4c_rom8(0x424270UL + q);
        } else {
            unsigned long q = dec4c_dv((c->g45d120 << 2) & 0xFFFFFFFFUL, d138);
            unsigned long v = (dec4c_rom8(0x424290UL + q) - c->binmode) & 0xFFUL;
            if (c->g_e3a0 != 0)
                v = (v << 1) & 0xFFUL;
            lb = v;
        }
        if (flag) {
            unsigned long a = c->g45d114;
            sel = ((a > 0UL) ? 1UL : 0UL) + 3UL + ((a > 2UL) ? 1UL : 0UL);
        } else {
            unsigned long b = c->mlen[mlen_a];
            sel = ((b > 0x0EUL) ? 1UL : 0UL) + ((b > 0x3EUL) ? 1UL : 0UL);
        }
        tab = D4C_ML + (sel << 8);
        for (;;) {
            unsigned long pa, p;
            len = ((unsigned char)node + 1U) & 0xFFU;
            if (lb < len) {
                unsigned long v = dec4c_rc_div(c, 0x1000UL) & 0xFFFFUL;
                pa = (tab + node) & (DEC4C_SEE_LEN - 1UL);
                p = c->see[pa];
                if (v < p) {
                    unsigned long lo = 0, hi = p;
                    len = (unsigned char)node;
                    c->see[pa] = (p + ((0x1000UL - p) >> 4)) & 0xFFFFFFFFUL;
                    dec4c_rc_dec(c, lo, hi);
                } else {
                    unsigned long lo = p, hi = 0x1000UL;
                    c->see[pa] = (p - (p >> 4)) & 0xFFFFFFFFUL;
                    dec4c_rc_dec(c, lo, hi);
                }
            }
            {
                unsigned right = (len > node) ? 1UL : 0UL;
                node = dec4c_rom8(0x4242a0UL + ((2UL * node + right) & 0x1FFUL));
                if (node == 0)
                    break;
            }
        }
        if (dist == c->g45d130) {
            c->g45d134 >>= 1;
        } else {
            c->g45d130 = dist;
            c->g45d134 = (c->g45d134 + len) & 0xFFFFFFFFUL;
        }
        c->mlen[mlen_a] = (unsigned char)len;
        if (len != 0xFFU) {
            c->g45d114 = 0;
            c->pend = ((unsigned long)len + dstv) & mask;
        } else {
            c->pend = 0xFFFFFFFFUL;
            c->g45d114 = (c->g45d114 + 1UL) & 0xFFFFFFFFUL;
        }
        rem = len;
        for (;;) {
            unsigned char b = dec4c_tb(c, dstv);
            dec4c_output(c, b);
            dec4c_after_symbol(c, b, (rem <= 8UL) ? 1 : 0);
            dec4c_push_hist(c, b);
            dstv = (dstv + 1UL) & mask;
            rem = (rem - 1UL) & 0xFFFFFFFFUL;
            if (rem == 0)
                break;
        }
        return 1;
    }
}

/* Order-4 helpers (lit.rs): exclusion list, SEE bumps, recent/marked. */
static void dec4c_excl_push(dec4c_ctx *c, unsigned char s)
{
    unsigned n = c->nex;
    if (n < 256U) {
        c->excl[n] = s;
        c->nex = (unsigned char)(n + 1U);
    }
}

static int dec4c_recent(dec4c_ctx *c, unsigned char s)
{
    return (c->pos <= c->recent[s]) ? 1 : 0;
}

static int dec4c_marked(dec4c_ctx *c, unsigned char s)
{
    return (c->see[D4C_MARKS + s] == c->nbytes) ? 1 : 0;
}

static void dec4c_see_bump(dec4c_ctx *c, unsigned i, int up)
{
    unsigned long *p = &c->see[i & (DEC4C_SEE_LEN - 1UL)];
    if (up)
        *p = (*p + ((0x2000UL - *p) >> 6)) & 0xFFFFFFFFUL;
    else
        *p = (*p - (*p >> 6)) & 0xFFFFFFFFUL;
}

static unsigned dec4c_cost(dec4c_ctx *c, unsigned idx);

static unsigned long dec4c_g68c(dec4c_ctx *c)
{
    return c->g6b8[c->g45c68c & 0x3FFUL];
}

/* decode_symbol preamble (0x41030f up to order-4 probe): context
 * setup + order-2 record load. Returns L10c in *out and l50 in *out50. */
static void dec4c_decode_symbol_pre(dec4c_ctx *c, unsigned long *out_l10c,
                                    unsigned char *out_l50, unsigned char rec[20])
{
    unsigned bm = (c->binmode != 0) ? 1UL : 0UL;
    unsigned long c0 = c->c0;
    unsigned long c6a4, lfc, add, c69c, o2i;
    unsigned long l10c;
    int i;
    c6a4 = (c->g_e3a0 == 2) ?
        (((c0 >> 8) & 0xFFUL) + ((c0 >> 16) & 0xFF00UL) + ((c->c1 << 8) & 0xFF0000UL) + (c->c1 & 0xFF000000UL)) & 0xFFFFFFFFUL :
        c0;
    c->g45c6a4 = c6a4;
    lfc = ((c6a4 & 0xC0UL) == 0xC0UL) ? 0x10UL : 0UL;
    (void)lfc;
    c->g45c694 = ((c6a4 >> 13) ^ c6a4) & 0x7FFFUL;
    add = (!bm && dec4c_rom8(0x42738dUL + ((c6a4 >> 16) & 0xFFUL)) != 0) ? 0x10000UL : 0UL;
    c69c = (c6a4 & 0xFFFFUL) + add;
    c->g45c69c = c69c;
    c->g_e394 = 0;
    c->g_e397 = 0;
    c->g_e39a = 0;
    c->g_e399 = 0;
    c->g_e39f = 0;
    c->g_e39d = 0;
    c->g_e393 = 0;
    c->g_e398 = 0;
    c->nex = 0;
    c->g45c658 = 0;
    c->g45c668 = 0;
    c->g_e3a1 = 0;
    c->g_e3a4 = 0;
    o2i = c69c & 0x1FFFFUL;
    if (c->pend != 0xFFFFFFFFUL) {
        c->nex = 1;
        c->excl[0] = dec4c_tb(c, c->pend);
        c->pend = 0xFFFFFFFFUL; /* take, like Rust Option::take() */
    }
    /* per-phase threshold slots (per_d4/per_e8 tables not yet typed;
     * G_CCD4 base only, matching new_segment). */
    {
        unsigned long c64c = c->g45c64c;
        if (c->g45c674 > 1UL || ((unsigned char)(c64c - 2UL) <= 2U)) {
            unsigned long dstv = c64c, cntv = c64c;
            if (c64c <= 4UL)
                dstv = dec4c_rom32(0x4236d4UL + 4UL * c64c) + (c64c ? (c->nbytes % c64c) : 0);
            if (c64c <= 0x10UL)
                cntv = dec4c_rom32(0x4236e8UL + 4UL * c64c) + (c64c ? (c->nbytes % c64c) : 0);
            c->g45c68c = 0x187UL + dstv;
            c->g45c688 = cntv;
        }
    }
    l10c = c6a4 & 0xFFUL;
    if (!bm && dec4c_rom8(0x42738dUL + ((c6a4 >> 8) & 0xFFUL)) != 0)
        l10c += 0x100UL;
    dec4c_o2r(c, o2i, rec);
    *out_l10c = l10c;
    *out_l50 = c->mru_s[l10c & 0x3FFUL];
    /* MARKS update for the four order-2 contexts */
    for (i = 0; i < 4; i++)
        c->see[D4C_MARKS + c->x698[c->x698p[i] & 0x3FFFFUL]] = c->nbytes;
}

/* Order-4 probe scan (0x41030f head): walk the 8-way chain for bucket
 * o4s(g45c694); on hit copy the 32-byte bucket to b[32] and run the
 * skip tests. Returns 1 if the order should decode (b filled), 0 to
 * fall through to order-2. rec is the order-2 record (o2i). */
static int dec4c_o4_scan(dec4c_ctx *c, unsigned long c6a4, const unsigned char rec[20],
                         unsigned char b[32])
{
    unsigned long bm = (c->binmode != 0) ? 1UL : 0UL;
    unsigned long l50_dummy = 0;
    /* The order-4 "weight by run count" (l108) and the symbol the bucket
     * points at (l28) are taken from the bucket as soon as it is found -
     * before any of the skip tests.  They stay valid when the order-4
     * coder is skipped and the order-2/delta stage runs (Rust lit.rs
     * computes l108/l28 right after the bucket copy, the original does
     * the same at 0x41030f). */
    c->g_l108 = 0UL;
    c->g_l28 = 0U;
    /* l52/l4e/g68c need lit state; for scan we evaluate skip tests that
     * only need b/rec/g45/g6b8. l52/l4e paths return "skip" conservatively. */
    for (;;) {
        unsigned long bucket = ((c->g45c694 & 0x7FFFUL) << 3) | ((c->g45c694 >> 15) & 7UL);
        unsigned char *bptr = c->o4 + bucket * 32UL;
        unsigned long ctx32;
        unsigned f0 = 0x0CU, ec = 0x0CU;
        unsigned k = 0, fk, sum;
        unsigned long b1a, b19;
        if (bptr[0] == 0) {
            c->g45c694 = (c->g45c694 + 0x40000UL) & 0xFFFFFFFFUL;
            return 0;
        }
        ctx32 = ((unsigned long)bptr[28]) | ((unsigned long)bptr[29] << 8) |
                ((unsigned long)bptr[30] << 16) | ((unsigned long)bptr[31] << 24);
        if (ctx32 != c6a4) {
            c->g45c694 = (c->g45c694 + 0x8000UL) & 0xFFFFFFFFUL;
            if (c->g45c694 >= 0x40000UL)
                return 0;
            continue;
        }
        memcpy(b, bptr, 32UL);
        (void)l50_dummy;
        c->g_e3a3 = 0xFFU;
        sum = b[0];
        fk = b[1];
        k = 0;
        for (;;) {
            unsigned s = b[0x0DUL + k];
            if (s == 0) { /* l50/l3c handled by caller via f0/ec; keep scan */ }
            sum = (sum + fk) & 0xFFFFFFFFUL;
            k += 1UL;
            if (k >= 0x0CUL) break;
            fk = b[k + 1UL];
            if (fk == 0) break;
        }
        (void)f0; (void)ec;
        b1a = b[0x1A];
        c->g_l108 = dec4c_rom32(0x42754dUL + 4UL * ((b1a >> 4) & 0x0FUL));
        c->g_l28 = b[0x0DUL + (b1a & 0x0FUL)];
        c->g45c678 = sum;
        /* Skip: g45c6a0/g45c674/g68c threshold */
        if (c->g45c6a0 == 0 && c->g45c674 > 1UL &&
            c->g45d104 <= c->g45d138 && dec4c_g68c(c) > 0x3FEUL) {
            unsigned long q = b1a >> 4;
            unsigned long e = ((unsigned long)b[0x1B] << 6) * q & 0xFFFFFFFFUL;
            if (e < c->g6b8[c->g45c688 & 0x3FFUL]) {
                return 0;
            }
        }
        /* Skip: order-2 vs order-4 cost comparison */
        if (rec[0] != 0 && (bm != 0 || rec[0x13] >= 0x30UL)) {
            b19 = b[0x19];
            if ((b19 & 0x38UL) == 0UL) {
                unsigned long e = (unsigned long)rec[0x12] * ((b19 & 4UL) + b[1] + (b[0x1A] >> 4)) & 0xFFFFFFFFUL;
                unsigned long a = ((unsigned long)rec[1] + ((unsigned long)rec[0x13] >> 4)) * c->g45c678 & 0xFFFFFFFFUL;
                if (e < a) {
                    return 0;
                }
            }
            if (rec[0x13] >= 0x60UL && ((unsigned long)rec[0x13] >> 2) > (unsigned long)b[0x1A]) {
                return 0;
            }
        }
        /* Allowed-symbol mask + escape check */
        {
            unsigned long c678 = c->g45c678;
            unsigned long c698 = c678;
            unsigned mask[16];
            unsigned j;
            int mi;
            for (mi = 0; mi < 16; mi++) mask[mi] = (mi < 12) ? 1UL : 0UL;
            if (c->nex != 0) {
                unsigned x = c->excl[0];
                j = 0;
                for (;;) {
                    if (x == b[j + 0x0DUL]) {
                        c698 = (c698 - b[j + 1UL]) & 0xFFFFFFFFUL;
                        mask[j] = 0;
                        break;
                    }
                    j += 1UL;
                    if (b[j + 1UL] == 0 || j >= 0x0CUL) break;
                }
            }
            (void)mask;
            c->g45c698 = c698;
            if ((unsigned long)b[0] >= c698) {
                c->g_e394 = 1;
                c->g45c6b0 = (c->g45c6b0 + 1UL) & 0xFFFFFFFFUL;
                return 0;
            }
        }
        return 1;
    }
}

/* Order-4 single-symbol binary decision (b[2]==0 path of 0x41030f).
 * On esc==false sym is set; on esc==true s0 is excluded. */
static int dec4c_see_freq(dec4c_ctx *c, unsigned long i, unsigned long rest,
                            unsigned long f, unsigned long *out_q)
{
    unsigned long p = c->see[i & (DEC4C_SEE_LEN - 1UL)];
    unsigned long d = (0x2000UL - p) & 0xFFFFFFFFUL;
    unsigned long q;
    if (d == 0)
        return 0;
    q = (dec4c_dv((rest * p) & 0xFFFFFFFFUL, d) + 1UL) & 0xFFFFFFFFUL;
    if (q > f) {
        *out_q = (q > 0xFFUL) ? 0xFFUL : q;
        return 1;
    }
    return 0;
}

static void dec4c_o4_single(dec4c_ctx *c, const unsigned char b[32],
                            const unsigned char rec[20],
                            unsigned long c6a4, unsigned long l10c,
                            unsigned l3c, unsigned l50, int bm,
                            int *esc, unsigned char *sym, unsigned long *see_esc)
{
    unsigned char s0 = b[0x0D];
    unsigned long datv = b[1];
    unsigned long acc = 0;
    unsigned long pa, v, p;
    if ((unsigned)s0 == l3c) {
        datv = (datv + ((unsigned)rec[0x13] >> 4)) & 0xFFFFFFFFUL;
        acc = 1;
    }
    if ((unsigned)s0 == l50) {
        acc |= 1UL;
        datv = (datv + c->mru_n[l10c & 0x3FFUL]) & 0xFFFFFFFFUL;
    }
    if (rec[7] != 0 && c->g45c6b0 < 6UL)
        acc |= 0x10UL;
    if (bm) {
        if (dec4c_recent(c, s0))
            acc |= 0x100UL;
        if ((unsigned)s0 == (c6a4 & 0xFFUL))
            acc |= 2UL;
    } else if (dec4c_rom8(0x42738dUL + s0) != 0) {
        acc |= 2UL;
    }
    if (datv < 0x1EUL) {
        unsigned long esi_ = c6a4 & 0xE0UL;
        acc |= esi_;
        if (bm) {
            if (esi_ == ((c6a4 >> 8) & 0xE0UL))
                acc |= 8UL;
            if (esi_ == ((unsigned)s0 & 0xE0UL))
                acc |= 4UL;
        } else {
            acc |= ((unsigned long)dec4c_rom8(0x42738dUL + ((c6a4 >> 8) & 0xFFUL)) << 2) +
                   ((unsigned long)dec4c_rom8(0x42738dUL + (c6a4 & 0xFFUL)) << 3);
        }
    }
    if (dec4c_marked(c, s0))
        acc |= 0x200UL;
    {
        unsigned lvl = (datv & 0x1FFUL) < 288UL ? dec4c_rom32(0x423df0UL + 4UL * (datv & 0x1FFUL)) : 0UL;
        pa = D4C_S_O4BIN + ((lvl << 10) + acc);
    }
    v = dec4c_rc_div(c, 0x2000UL) & 0xFFFFUL;
    p = c->see[pa & (DEC4C_SEE_LEN - 1UL)];
    if (v < p) {
        *esc = 0;
        c->g_e3a3 = 0;
        *sym = s0;
        dec4c_rc_dec(c, 0, p);
    } else {
        c->g_e3a3 = 0x0CU;
        dec4c_excl_push(c, s0);
        dec4c_rc_dec(c, p, 0x2000UL);
    }
    *see_esc = pa;
}

/* Order-4 escape total estimate (b[2]!=0 path, S_O4ESC part).
 * Returns t = dv(v<<13, see[pa])+1 (or c698 if ecx >= 0x80). */
static unsigned long dec4c_o4_escape(dec4c_ctx *c, const unsigned char b[32],
                                     const unsigned char rec[20],
                                     unsigned long c6a4, unsigned long c698,
                                     int bm, unsigned long *see_esc_out)
{
    unsigned long cntv = (c698 - (unsigned long)b[0]) & 0xFFFFFFFFUL;
    unsigned long t = c698;
    if (cntv < 0x80UL) {
        unsigned long ctx_same, ctx_level, idx, pa;
        if (bm) {
            unsigned long l2a = (c6a4 >> 8) & 0xE0UL;
            ctx_same = (((c6a4 & 0xE0UL) == l2a) ? 1UL : 0UL);
            ctx_level = (c6a4 & 0xC0UL) >> 5;
        } else {
            unsigned long l2a = (unsigned long)dec4c_rom8(0x42738dUL + ((c6a4 >> 8) & 0xFFUL)) * 2UL;
            ctx_same = (unsigned long)dec4c_rom8(0x42738dUL + (c6a4 & 0xFFUL)) * 4UL + l2a;
            ctx_level = ((c6a4 & 0xFFUL) == 0xDFUL) ? 1UL : 0UL;
        }
        idx = ctx_level + ctx_same + ((rec[7] != 0) ? 8UL : 0UL) + ((c->g45c6b0 >= 6UL) ? 16UL : 0UL);
        {
            unsigned long lb0 = dec4c_rom32(0x4239f0UL + 4UL * (unsigned)b[0]);
            unsigned long lcx = dec4c_rom32(0x4239f0UL + 4UL * (cntv & 0xFFUL));
            pa = D4C_S_O4ESC + ((lb0 << 8) + (lcx << 5) + idx);
        }
        t = (dec4c_dv((cntv << 13) & 0xFFFFFFFFUL, c->see[pa & (DEC4C_SEE_LEN - 1UL)]) + 1UL) & 0xFFFFFFFFUL;
        *see_esc_out = pa;
    } else {
        *see_esc_out = 0;
    }
    return t;
}

/* Order-4 multi-symbol decode (b[2]!=0 path): SEE corrections + loop.
 * b is the local bucket copy (mutated); rec is the order-2 record.
 * On esc==false sym is set. */
static void dec4c_o4_multi(dec4c_ctx *c, unsigned char b[32],
                           const unsigned char rec[20],
                           unsigned long c6a4, unsigned long l10c,
                           unsigned l3c, unsigned l50, int bm,
                           unsigned long c698, unsigned long lfc,
                           int *esc, unsigned char *sym, unsigned long *see_esc)
{
    unsigned long p42 = 0, p3a = 0, p2e = 0, pedi = 0;
    unsigned long k2a, t;
    unsigned long l104, l100, l108, l28, f0, ec;
    unsigned mask[16];
    unsigned long see_esc_local = 0;
    int mi;
    for (mi = 0; mi < 16; mi++) mask[mi] = (mi < 12) ? 1UL : 0UL;
    if (c->nex != 0) {
        /* c698 arrives already exclusion-adjusted from the scan
         * (Rust computes it once); only rebuild the mask here. */
        unsigned x = c->excl[0];
        unsigned j;
        for (j = 0; j < 16UL; j++) {
            if (x == b[j + 0x0DUL]) {
                mask[j] = 0;
                break;
            }
            if (b[j + 1UL] == 0 || j >= 0x0CUL) break;
        }
    }
    /* Recompute f0/ec/l108/l28 from b (scan locals) */
    f0 = 0x0CU; ec = 0x0CU;
    {
        unsigned k = 0;
        unsigned fk;
        /* f0/ec are recomputed directly from the order-4 record below */
        k = 0;
        fk = b[1];
        {
            unsigned long s = b[0];
            unsigned long sum2 = s;
            unsigned kk = 0;
            unsigned fkk = fk;
            for (;;) {
                unsigned bs = b[0x0DUL + kk];
                if ((unsigned)bs == l50) f0 = kk;
                if ((unsigned)bs == l3c) ec = kk;
                sum2 = (sum2 + fkk) & 0xFFFFFFFFUL;
                (void)sum2;
                kk += 1UL;
                if (kk >= 0x0CUL) break;
                fkk = b[kk + 1UL];
                if (fkk == 0) break;
                (void)k;
            }
            (void)fk; (void)k;
        }
    }
    l108 = dec4c_rom32(0x42754dUL + 4UL * ((b[0x1A] >> 4) & 0x0FUL));
    l28 = b[0x0DUL + (b[0x1A] & 0x0FUL)];
    l104 = dec4c_rom32(0x42750dUL + 4UL * ((rec[0x13] >> 4) & 0x0FUL));
    l100 = dec4c_rom32(0x4274cdUL + 4UL * (c->mru_n[l10c & 0x3FFUL] & 0x0FUL));
    k2a = (c->g45d104 < c->g45d138) ? (b[0x1A] & 0x0FUL) : 0x0CUL;
    t = dec4c_o4_escape(c, b, rec, c6a4, c698, bm, &see_esc_local);
    *see_esc = see_esc_local;
    (void)p42; (void)p3a; (void)p2e; (void)pedi;
    /* SEE corrections (S_O4MPS/MRU/O2/FST) */
    if (mask[k2a & 0x0FUL]) {
        unsigned long cc = b[(k2a & 0x0FUL) + 1UL];
        unsigned long bc = (c698 - cc) & 0xFFFFFFFFUL;
        unsigned long b8 = 0, e, x, q;
        e = (b[0x1B] >> 4) + ((cc - (cc == 2UL ? 1UL : 0UL)) & 0xFFFFFFFFUL) + l108;
        e &= 0xFFFFFFFFUL;
        if (l28 == l3c) { b8 = 0x40UL; e = (e + l104) & 0xFFFFFFFFUL; }
        if (l28 == l50) { b8 |= 0x80UL; e = (e + l100) & 0xFFFFFFFFUL; }
        x = dec4c_dv((e << 4) & 0xFFFFFFFFUL, (bc + e) & 0xFFFFFFFFUL);
        if (k2a != 0) x |= 0x100UL;
        if ((l28 & 0xC0UL) == 0xC0UL) x |= 0x20UL;
        if (dec4c_recent(c, (unsigned char)l28)) x |= 0x200UL;
        if (dec4c_marked(c, (unsigned char)l28)) x |= 0x400UL;
        p2e = D4C_S_O4MPS + ((lfc | x | b8) & 0xFFFFFFFFUL);
        if (dec4c_see_freq(c, p2e, bc, cc, &q)) {
            t = (t + (q - cc)) & 0xFFFFFFFFUL;
            b[(k2a & 0x0FUL) + 1UL] = (unsigned char)q;
        }
    }
    if (mask[f0 & 0x0FUL] && k2a != f0) {
        unsigned long d8 = b[(f0 & 0x0FUL) + 1UL];
        unsigned long b4 = (c698 - d8) & 0xFFFFFFFFUL;
        unsigned long e = (d8 + l100) & 0xFFFFFFFFUL;
        unsigned long x = dec4c_dv((e << 4) & 0xFFFFFFFFUL, (b4 + e) & 0xFFFFFFFFUL);
        unsigned long q;
        if ((l50 & 0xC0UL) == 0xC0UL) x |= 0x20UL;
        if ((unsigned)l50 == l3c) x |= 0x40UL;
        if (dec4c_recent(c, (unsigned char)l50)) x |= 0x80UL;
        if (dec4c_marked(c, (unsigned char)l50)) x |= 0x100UL;
        x |= lfc;
        p3a = D4C_S_O4MRU + x;
        if (dec4c_see_freq(c, p3a, b4, d8, &q)) {
            t = (t + (q - d8)) & 0xFFFFFFFFUL;
            b[(f0 & 0x0FUL) + 1UL] = (unsigned char)q;
        }
    }
    if (mask[ec & 0x0FUL] && k2a != ec && f0 != ec) {
        unsigned long d0 = b[(ec & 0x0FUL) + 1UL];
        unsigned long b0 = (c698 - d0) & 0xFFFFFFFFUL;
        unsigned long e = (d0 + l104) & 0xFFFFFFFFUL;
        unsigned long x = dec4c_dv((e << 4) & 0xFFFFFFFFUL, (b0 + e) & 0xFFFFFFFFUL);
        unsigned long q;
        if ((l3c & 0xC0UL) == 0xC0UL) x |= 0x20UL;
        if (dec4c_recent(c, (unsigned char)l3c)) x |= 0x40UL;
        if (dec4c_marked(c, (unsigned char)l3c)) x |= 0x80UL;
        x |= lfc;
        pedi = D4C_S_O4O2 + x;
        if (dec4c_see_freq(c, pedi, b0, d0, &q)) {
            t = (t + (q - d0)) & 0xFFFFFFFFUL;
            b[(ec & 0x0FUL) + 1UL] = (unsigned char)q;
        }
    }
    if (mask[0] && k2a != 0 && f0 != 0 && ec != 0) {
        unsigned long e0 = b[1];
        unsigned long x = dec4c_dv((e0 << 4) & 0xFFFFFFFFUL, c698);
        unsigned s0 = b[0x0D];
        unsigned long q;
        if ((s0 & 0xC0UL) == 0xC0UL) x |= 0x20UL;
        if (dec4c_recent(c, (unsigned char)s0)) x |= 0x80UL;
        if (dec4c_marked(c, (unsigned char)s0)) x |= 0x100UL;
        x |= lfc;
        p42 = D4C_S_O4FST + x;
        q = ((c698 * c->see[p42 & (DEC4C_SEE_LEN - 1UL)]) >> 13) + 1UL;
        if (q >= 0x100UL) q = 0xFFUL;
        t = (t + (q - e0)) & 0xFFFFFFFFUL;
        b[1] = (unsigned char)q;
    }
    /* Multi-symbol decode loop */
    {
        unsigned long v = dec4c_rc_div(c, t) & 0xFFFFUL;
        unsigned long cum = 0, e4 = 0;
        unsigned k = 0;
        while (k < 0x0CU) {
            if (mask[k]) {
                unsigned long f = b[k + 1UL];
                if (f == 0) { k = 0x0CU; break; }
                e4 = (cum + f) & 0xFFFFFFFFUL;
                if (v < e4) {
                    *esc = 0;
                    *sym = b[k + 0x0DUL];
                    break;
                }
                dec4c_excl_push(c, b[k + 0x0DUL]);
                cum = e4;
            }
            k += 1UL;
        }
        c->g_e3a3 = (unsigned char)k;
        {
            unsigned long hi = (k == 0x0CU) ? c->rc_t : e4;
            dec4c_rc_dec(c, cum, hi);
        }
        {
            unsigned long got = k;
            if (p42 != 0) dec4c_see_bump(c, p42, got == 0);
            if (p3a != 0) dec4c_see_bump(c, p3a, got == f0);
            if (pedi != 0) dec4c_see_bump(c, pedi, got == ec);
            if (p2e != 0) dec4c_see_bump(c, p2e, got == k2a);
        }
        /* see_esc is bumped once in decode_symbol (Rust 0x4110e5 shared
         * by single/multi); do not bump here too. */
    }
}

/* Order-2 coder (lit.rs order2, a4=None delta stub for now). */
/* o2_halve_if + o2_sort (update.rs helpers) */
static void dec4c_o2_halve_if(unsigned char r[20])
{
    unsigned lim = dec4c_rom8(0x4245a0UL + r[0]);
    if ((unsigned)r[0x12] > lim) {
        int j;
        r[0x12] = 0;
        for (j = 8; j >= 0; j--) {
            unsigned v = (unsigned)(r[j] + 1U) >> 1;
            r[j] = (unsigned char)v;
            r[0x12] = (unsigned char)((r[0x12] + v) & 0xFFUL);
        }
    }
}

static unsigned dec4c_o2_sort(unsigned char r[20], unsigned k)
{
    while (k != 0) {
        unsigned dlo = r[k + 1UL];
        unsigned dhv, s, bval;
        if (dlo < r[k])
            break;
        dhv = r[k];
        r[k] = (unsigned char)dlo;
        s = r[k + 9UL];
        bval = r[k + 8UL];
        r[k + 8UL] = (unsigned char)s;
        r[k + 1UL] = (unsigned char)dhv;
        r[k + 9UL] = (unsigned char)bval;
        k -= 1UL;
        r[0x13] = (unsigned char)(r[0x13] - 1U);
    }
    return k;
}

/* 0x40f19a light update (after order-4 escape). */
static void dec4c_o2_update_light(dec4c_ctx *c, unsigned char sym)
{
    unsigned long ri = c->g45c69c & 0x1FFFFUL;
    unsigned char r[20];
    unsigned k = 0;
    dec4c_o2r(c, ri, r);
    if (r[0] == 0) {
        memset(r, 0, sizeof(r));
        r[0] = 2;
        r[1] = 2;
        r[0x12] = 4;
        r[9] = sym;
        dec4c_o2_put(c, ri, r);
        return;
    }
    while (r[k + 9UL] != sym) {
        k += 1UL;
        if (k >= 8UL) break;
    }
    dec4c_o2_halve_if(r);
    r[0x12] = (unsigned char)((r[0x12] + 1U) & 0xFFUL);
    if (k < 8UL) {
        r[k + 1UL] = (unsigned char)(r[k + 1UL] + 1U);
        if ((r[0x13] & 0x0FUL) == k) {
            if (r[0x13] < 0x40U)
                r[0x13] = (unsigned char)(r[0x13] + 0x10U);
        } else {
            r[0x13] = (unsigned char)k;
        }
    } else {
        unsigned d;
        r[0x11] = (unsigned char)(r[0x11] + 1U);
        k = 1;
        r[0] = (unsigned char)(r[0] + 1U);
        while (r[k + 1UL] != 0) {
            k += 1UL;
            if (k == 7UL) break;
        }
        if (r[k + 1UL] >= 0x0CU) {
            k = 0;
        } else {
            d = (unsigned char)(1U - r[k + 1UL]);
            r[0x12] = (unsigned char)(r[0x12] + d);
            r[k + 1UL] = 1;
            r[k + 9UL] = sym;
            r[0x13] = (unsigned char)k;
        }
    }
    c->g_e3a5 = (unsigned char)dec4c_o2_sort(r, k);
    dec4c_o2_put(c, ri, r);
}

/* 0x40f19a: full order-2 update (after order-4 escape). */
static void dec4c_o2_update(dec4c_ctx *c, unsigned char sym)
{
    unsigned long ri = c->g45c69c & 0x1FFFFUL;
    unsigned char r[20];
    unsigned k;
    dec4c_o2r(c, ri, r);
    if (r[0] == 0) {
        memset(r, 0, sizeof(r));
        r[0] = 3;
        r[1] = 3;
        r[0x12] = 6;
        r[9] = sym;
        dec4c_o2_put(c, ri, r);
        return;
    }
    k = c->g_e3a5;
    if (k == 0xFFU) {
        k = 0;
        while (r[k + 9UL] != sym) {
            k += 1UL;
            if (k >= 8UL) break;
        }
    }
    if (c->g_e39a != 0) {
        unsigned long x;
        if (k < 8UL) {
            c->g_e399 = 0;
            c->g_e39f = 0;
            x = r[k + 1UL];
        } else {
            x = r[0];
        }
        {
            unsigned long q = dec4c_dv((x << 14) & 0xFFFFFFFFUL, c->g45c6a8);
            c->g45c658 = (c->g45c658 + dec4c_cost(c, q)) & 0xFFFFFFFFUL;
        }
    }
    dec4c_o2_halve_if(r);
    r[0x12] = (unsigned char)((r[0x12] + 4U) & 0xFFUL);
    if (k < 8UL) {
        c->g45c6ac = ((c->g45c6ac * 3UL) >> 2) & 0xFFFFFFFFUL;
        r[0x11] >>= 1;
        r[k + 1UL] = (unsigned char)(r[k + 1UL] + 4U);
        if ((r[0x13] & 0x0FUL) == k) {
            if (r[0x13] < 0xF0U)
                r[0x13] = (unsigned char)(r[0x13] + 0x10U);
        } else {
            r[0x13] = (unsigned char)k;
        }
        c->g45d120 = (c->g45d120 + 5UL) & 0xFFFFFFFFUL;
    } else {
        unsigned d;
        k = 1;
        r[0x11] = (unsigned char)(r[0x11] + 1U);
        c->g45c6ac = (c->g45c6ac + 1UL) & 0xFFFFFFFFUL;
        r[0] = (unsigned char)(r[0] + 4U);
        while (r[k + 1UL] != 0) {
            k += 1UL;
            if (k == 7UL) break;
        }
        if (r[k + 1UL] >= 0x0CU) {
            k = 0;
        } else {
            d = (unsigned char)(4U - r[k + 1UL]);
            r[0x12] = (unsigned char)(r[0x12] + d);
            r[k + 1UL] = 4;
            r[k + 9UL] = sym;
            r[0x13] = (unsigned char)k;
        }
    }
    c->g_e3a5 = (unsigned char)dec4c_o2_sort(r, k);
    dec4c_o2_put(c, ri, r);
    c->g45d138 = (c->g45d138 + 2UL) & 0xFFFFFFFFUL;
    if (c->g45d138 > 0x800UL) {
        c->g45d138 >>= 1;
        c->g45d120 >>= 1;
        c->g45d104 >>= 1;
    }
}

/* Delta helpers (0x423684+0x140000, binary mode). */
static unsigned long dec4c_delta_entry(dec4c_ctx *c, unsigned k)
{
    static const unsigned long base[8] = {0U, 0x10000UL, 0x30000UL, 0x50000UL, 0x60000UL, 0x70000UL, 0x80000UL, 0xF0000UL};
    return (base[k & 7UL] + c->didx[k & 7UL]) & 0xFFFFFUL;
}

static void dec4c_de(dec4c_ctx *c, unsigned long i, unsigned char e[4])
{
    unsigned long idx = i & 0xFFFFFUL;
    e[0] = c->delta[idx][0];
    e[1] = c->delta[idx][1];
    e[2] = c->delta[idx][2];
    e[3] = c->delta[idx][3];
}

static void dec4c_delta_step(dec4c_ctx *c, unsigned char e[4], unsigned char sym)
{
    unsigned e0 = e[0], e1 = e[1], e2 = e[2], e3 = e[3] & 0x7FUL;
    unsigned h0, h1, h3, m0, m1, m2, m3;
    unsigned hv = (e1 >= 0xFEUL) ? 1UL : 0UL;
    unsigned mv = (e0 >= 0xFEUL) ? 1UL : 0UL;
    unsigned low = (e3 < 2UL) ? 1UL : 0UL;
    h0 = hv ? (((e0 + 1UL) >> 1) & 0xFFUL) : e0;
    h1 = (hv ? (((e1 + 1UL) >> 1) & 0xFFUL) : e1) + 1UL;
    h3 = (e3 < 0x7FUL) ? e3 + 1UL : e3;
    m0 = (mv ? (((e0 + 1UL) >> 1) & 0xFFUL) : e0) + 1UL;
    m1 = mv ? (((e1 + 1UL) >> 1) & 0xFFUL) : e1;
    m2 = low ? (unsigned)sym : e2;
    m3 = ((low ? 0UL : (e3 >> 1)) + 0x80UL) & 0xFFFFFFFFUL;
    if (sym == e2) {
        e[0] = (unsigned char)h0;
        e[1] = (unsigned char)h1;
        /* e[2] unchanged */
        e[3] = (unsigned char)h3;
    } else {
        e[0] = (unsigned char)m0;
        e[1] = (unsigned char)m1;
        e[2] = (unsigned char)m2;
        e[3] = (unsigned char)m3;
    }
    (void)c;
}

/* 0x40e885: binary-mode delta predictor lookup. Returns entry index or
 * 0xFFFFFFFF for none. */
static unsigned long dec4c_delta_lookup(dec4c_ctx *c)
{
    unsigned long best = 0;
    unsigned best_idx = 8U;
    unsigned long c0 = c->c0;
    unsigned long c1 = c->c1;
    unsigned long datv = c0 >> 8;
    unsigned long srcv = c0 >> 16;
    unsigned order = 0;
    unsigned long ctx_check;
    unsigned long cntv = srcv & 0xFF00UL;
    unsigned long b0 = c0 & 0xFFUL;
    unsigned long c64c, clo, b4, ph, v, t7;
    unsigned long check;
    c->pair_p = 0xFFFFFFFFUL;
    if ((c0 & 0xFFFF0000UL) == 0xFFFF0000UL)
        check = 0x10000UL;
    else if ((c0 & 0xFFFFUL) == 0xFFFFUL)
        check = 0x20000UL;
    else if ((c0 & 0xFF000000UL) == 0xFF000000UL)
        check = 0x30000UL;
    else if ((c0 & 0x00FF0000UL) == 0x00FF0000UL)
        check = 0x40000UL;
    else if ((c0 & 0xFF00UL) == 0xFF00UL)
        check = 0x50000UL;
    else if ((c0 & 0xFFUL) == 0xFFUL)
        check = 0x60000UL;
    else
        check = 0;
    ctx_check = check;
    c->didx[0] = ((c1 >> 24) + cntv) & 0xFFFFFFFFUL;
    c->didx[1] = datv & 0xFFFFUL;
    c->didx[2] = srcv & 0xFFFFUL;
    c->didx[3] = ((datv & 0xFF00UL) + b0) & 0xFFFFFFFFUL;
    c->didx[4] = cntv + (datv & 0xFFUL);
    c->didx[5] = cntv + b0;
    c->didx[6] = ((c1 & 0xFFFFUL) + ctx_check) & 0xFFFFFFFFUL;
    if (b0 == 0xFFUL) {
        unsigned d;
        for (d = 1; d < 3U; d++) {
            unsigned long hi = (((c->didx[d] >> 16) & 0xFFFFUL) + 1UL) & 0xFFFFUL;
            c->didx[d] = (c->didx[d] & 0xFFFFUL) | (hi << 16);
        }
    }
    c64c = c->g45c64c;
    clo = dec4c_hist(c, c64c);
    b4 = dec4c_hist(c, (c64c * 2UL) & 0xFFFFFFFFUL);
    if (c->g_e395 == c64c) {
        if ((c0 & 0xFFUL) == 1UL)
            order = 8;
        ph = ((c0 >> 16) == 0xF6FFUL) ? 1UL : 0UL;
    } else if (c64c == 0) {
        c->bad = 1;
        ph = 0;
    } else {
        ph = c->nbytes % c64c;
    }
    v = ((ph << 8) + ((unsigned char)(clo - b4))) & 0xFFFFFFFFUL;
    c->didx[7] = v;
    {
        static const unsigned long base7[1] = {0xF0000UL};
        t7 = (base7[0] + v) & 0xFFFFFUL;
    }
    c->delta[t7][2] = (unsigned char)(((clo + clo) - b4) & 0xFFUL);
    while (order < 8U) {
        unsigned long ei = dec4c_delta_entry(c, order);
        unsigned char e[4];
        dec4c_de(c, ei, e);
        if (e[0] != 0 && e[3] < 0x80UL) {
            unsigned long num = ((unsigned long)e[1] << 8) * (unsigned long)e[3];
            unsigned long d = e[0];
            if (num >= (best + 1UL) * d) {
                best = num / d;
                best_idx = order;
            }
        }
        order += 1U;
    }
    if (best_idx < 8U) {
        unsigned long ei = dec4c_delta_entry(c, best_idx);
        unsigned char e[4];
        dec4c_de(c, ei, e);
        if (e[1] > e[0] || (e[3] & 0x7FUL) > 1UL) {
            if (c64c > 1UL)
                c->g_e398 = (best_idx == 7U) ? 1U : 0U;
            if (e[0] == 2U && (unsigned)((e[1] - 3U) & 0xFFUL) < 4U) {
                unsigned long i = (((unsigned long)e[1] << 3) - 0x18UL + best_idx) & 0xFFFFFFFFUL;
                c->pair_p = (i * 2UL) & 0xFFFFFFFFUL;
            }
            if (e[0] == 3U && (unsigned)((e[1] - 4U) & 0xFFUL) < 4U) {
                unsigned long i = (((unsigned long)e[1] << 3) - 0x20UL + best_idx) & 0xFFFFFFFFUL;
                c->pair_p = (i * 2UL + 0x40UL) & 0xFFFFFFFFUL;
            }
            c->g_e396 = (unsigned char)best_idx;
            return ei;
        }
    }
    if (c->g45c674 > 1UL)
        c->g6b8[c->g45c68c & 0x3FFUL] >>= 1;
    return 0xFFFFFFFFUL;
}

/* 0x40eba7: update delta predictor entries. */
static void dec4c_delta_update(dec4c_ctx *c, unsigned char sym)
{
    unsigned order;
    for (order = 0; order < 8U; order++) {
        unsigned long ei = dec4c_delta_entry(c, order);
        unsigned char e[4];
        dec4c_de(c, ei, e);
        if (e[0] != 0 && order == c->g_e396) {
            if (c->g_e399 != 0) {
                unsigned long x = (sym == e[2]) ? (unsigned)e[1] : (unsigned)e[0];
                unsigned long q = dec4c_dv((x << 14) & 0xFFFFFFFFUL, ((unsigned)e[0] + (unsigned)e[1]) & 0xFFFFFFFFUL);
                unsigned cst = dec4c_cost(c, q);
                if (sym == e[2])
                    c->g_e39f = 0;
                c->g45c658 = (c->g45c658 + cst) & 0xFFFFFFFFUL;
            }
            if (c->pair_p != 0xFFFFFFFFUL) {
                unsigned long p = c->pair_p & 0x7EUL;
                unsigned i = (sym == e[2]) ? 1UL : 0UL;
                c->pair[p + i] = (unsigned char)(c->pair[p + i] + 1U);
                if (c->pair[p + 1UL] >= 0xFEU || c->pair[p] >= 0xFEU) {
                    c->pair[p] = (unsigned char)(((unsigned)c->pair[p] + 1U) >> 1);
                    c->pair[p + 1UL] = (unsigned char)(((unsigned)c->pair[p + 1UL] + 1U) >> 1);
                }
            }
        }
        if (e[0] == 0) {
            c->delta[ei][0] = 2;
            c->delta[ei][1] = 2;
            c->delta[ei][2] = sym;
            c->delta[ei][3] = 0;
        } else {
            dec4c_delta_step(c, e, sym);
            c->delta[ei][0] = e[0];
            c->delta[ei][1] = e[1];
            c->delta[ei][2] = e[2];
            c->delta[ei][3] = e[3];
        }
    }
}

static void dec4c_order2(dec4c_ctx *c, unsigned long o2i,
                         unsigned l50, unsigned l3c, unsigned long l10c,
                         unsigned long c6a4, unsigned long lfc,
                         unsigned long l104, unsigned long l100,
                         int l52, int l4e, int bm,
                         unsigned long a4,
                         int *esc, unsigned char *sym)
{
    unsigned char r[20];
    unsigned mask[16];
    unsigned long cnt = 0, f8 = 8UL, tot;
    /* Exclusion bitmap: 8 x 32-bit lanes (Rust 4 x u64 equivalent). */
    unsigned long ex[8];
    unsigned k;
    unsigned g6a8;
    unsigned long see = 0;
    unsigned long l80 = 0, l88 = 0, l90 = 0;
    unsigned long p16 = 0, pe = 0, pedi = 0;
    unsigned char a4e2 = 0;
    int has_a4e2 = 0;
    dec4c_o2r(c, o2i, r);
    if (a4 != 0xFFFFFFFFUL) {
        unsigned char ae[4];
        dec4c_de(c, a4, ae);
        a4e2 = ae[2];
        has_a4e2 = 1;
    }
    for (k = 0; k < 16UL; k++) mask[k] = (k < 8UL) ? 1UL : 0UL;
    tot = r[0x12];
    for (k = 0; k < 8UL; k++) ex[k] = 0;
    for (k = 0; k < (unsigned long)c->nex; k++) {
        unsigned s = c->excl[k];
        ex[(s >> 5) & 0x07UL] |= 1UL << (s & 31UL);
    }
    k = 0;
    for (;;) {
        unsigned s = r[9UL + k];
        cnt += 1UL;
        if ((unsigned)s == l50) f8 = k;
        if (((ex[(s >> 5) & 0x07UL] >> (s & 31UL)) & 1UL) != 0UL) {
            tot = (tot - r[k + 1UL]) & 0xFFFFFFFFUL;
            mask[k] = 0;
        }
        k += 1UL;
        if (r[k + 1UL] == 0 || k >= 8UL) break;
    }
    c->g45c6a8 = tot;
    c->g_e3a5 = 0xFFU;
    if (((tot << 2) & 0xFFFFFFFFUL) <= (unsigned long)r[0] * 5UL)
        return;
    if ((((unsigned long)r[0x11] << 2) & 0xFFFFFFFFUL) + c->g45c6ac >= 0x10UL)
        return;
    c->g_e39a = (unsigned char)l52;
    if (l4e)
        return;
    g6a8 = (unsigned)tot;
    if (r[2] == 0) {
        unsigned s9 = r[9];
        unsigned long acc = (unsigned)r[1] >> 1;
        unsigned long cntv = 0;
        unsigned long pa, v, p;
        if ((unsigned)s9 == l50) {
            acc = (acc + c->mru_n[l10c & 0x3FFUL]) & 0xFFFFFFFFUL;
            cntv = 1;
        }
        if (acc < 0x10UL) {
            unsigned long ctx_idx;
            if (bm)
                ctx_idx = ((c6a4 & 0xC0UL) >> 5);
            else {
                ctx_idx = (unsigned long)dec4c_rom8(0x42738dUL + ((c6a4 >> 8) & 0xFFUL)) * 2UL +
                      (unsigned long)dec4c_rom8(0x42738dUL + (c6a4 & 0xFFUL)) * 4UL;
            }
            cntv |= ctx_idx;
        }
        if (bm && dec4c_recent(c, (unsigned char)s9))
            cntv |= 8UL;
        if (dec4c_marked(c, (unsigned char)s9))
            cntv |= 0x10UL;
        pa = D4C_S_O2BIN + ((((acc & 0x1FFUL) < 288UL ? dec4c_rom32(0x423df0UL + 4UL * (acc & 0x1FFUL)) : 0UL) << 5) + cntv);
        v = dec4c_rc_div(c, 0x2000UL) & 0xFFFFUL;
        p = c->see[pa & (DEC4C_SEE_LEN - 1UL)];
        if (v < p) {
            *esc = 0;
            c->g_e3a5 = 0;
            c->g_e3a4 = 1;
            *sym = (unsigned char)s9;
            dec4c_rc_dec(c, 0, p);
        } else {
            c->g_e3a5 = 8;
            dec4c_excl_push(c, (unsigned char)s9);
            dec4c_rc_dec(c, p, 0x2000UL);
        }
        see = pa;
    } else {
        unsigned k7c = (c->g45d104 < c->g45d138) ? (r[0x13] & 0x0FUL) : 8UL;
        unsigned long t = g6a8;
        if (cnt < 8UL) {
            unsigned long e = (g6a8 - r[0]) & 0xFFFFFFFFUL;
            if (e < 0x10UL) {
                unsigned long idx = dec4c_rom32(0x4239d0UL + 4UL * cnt) +
                    (e & 0x0CUL) + ((r[0x12] == g6a8) ? 1UL : 0UL);
                unsigned long pa = D4C_S_O2ESC + idx;
                t = (dec4c_dv((e << 13) & 0xFFFFFFFFUL, c->see[pa & (DEC4C_SEE_LEN - 1UL)]) + 1UL +
                     ((unsigned)r[0] >> 1)) & 0xFFFFFFFFUL;
                see = pa;
            }
        }
        if (mask[k7c & 0x0FUL]) {
            unsigned long cc = r[(k7c & 0x0FUL) + 1UL];
            unsigned long bc = (g6a8 - cc) & 0xFFFFFFFFUL;
            unsigned long b8 = 0, e, x, q;
            l80 = cc;
            e = (cc + l104) & 0xFFFFFFFFUL;
            if ((unsigned)l3c == l50) { b8 = 0x80UL; e = (e + l100) & 0xFFFFFFFFUL; }
            x = dec4c_dv((e << 4) & 0xFFFFFFFFUL, (bc + e) & 0xFFFFFFFFUL);
            if (k7c != 0) x |= 0x100UL;
            if ((l3c & 0xC0UL) == 0xC0UL) x |= 0x20UL;
            if (has_a4e2 && a4e2 == (unsigned char)l3c) x |= 0x40UL;
            if (dec4c_recent(c, (unsigned char)l3c)) x |= 0x200UL;
            if (dec4c_marked(c, (unsigned char)l3c)) x |= 0x400UL;
            x |= b8 | lfc;
            pedi = D4C_S_O2O2 + x;
            if (dec4c_see_freq(c, pedi, bc, cc, &q)) {
                t = (t + (q - cc)) & 0xFFFFFFFFUL;
                r[(k7c & 0x0FUL) + 1UL] = (unsigned char)q;
            }
        }
        if (mask[f8 & 0x0FUL] && k7c != f8) {
            unsigned long d8 = r[(f8 & 0x0FUL) + 1UL];
            unsigned long b4 = (g6a8 - d8) & 0xFFFFFFFFUL;
            unsigned long e = (d8 + l100) & 0xFFFFFFFFUL;
            unsigned long x, q;
            l88 = d8;
            x = dec4c_dv((e << 4) & 0xFFFFFFFFUL, (b4 + e) & 0xFFFFFFFFUL);
            if ((l50 & 0xC0UL) == 0xC0UL) x |= 0x20UL;
            if (dec4c_recent(c, (unsigned char)l50)) x |= 0x40UL;
            if (dec4c_marked(c, (unsigned char)l50)) x |= 0x80UL;
            x |= lfc;
            p16 = D4C_S_O2MRU + x;
            if (dec4c_see_freq(c, p16, b4, d8, &q)) {
                t = (t + (q - d8)) & 0xFFFFFFFFUL;
                r[(f8 & 0x0FUL) + 1UL] = (unsigned char)q;
            }
        }
        if (mask[0] && k7c != 0 && f8 != 0) {
            unsigned long e0 = r[1];
            unsigned long x = dec4c_dv((e0 << 4) & 0xFFFFFFFFUL, g6a8);
            unsigned long q;
            unsigned s9;
            l90 = e0;
            s9 = r[9];
            if ((s9 & 0xC0UL) == 0xC0UL) x |= 0x20UL;
            if (has_a4e2 && a4e2 == (unsigned char)s9) x |= 0x40UL;
            if (dec4c_recent(c, (unsigned char)s9)) x |= 0x80UL;
            if (dec4c_marked(c, (unsigned char)s9)) x |= 0x100UL;
            x |= lfc;
            pe = D4C_S_O2FST + x;
            q = ((g6a8 * c->see[pe & (DEC4C_SEE_LEN - 1UL)]) >> 13) + 1UL;
            if (q >= 0x100UL) q = 0xFFUL;
            t = (t + (q - e0)) & 0xFFFFFFFFUL;
            r[1] = (unsigned char)q;
        }
        {
            unsigned long v = dec4c_rc_div(c, t) & 0xFFFFUL;
            unsigned long cum = 0, e2 = 0;
            unsigned kk = 0;
            while (kk < 8UL) {
                if (mask[kk]) {
                    unsigned long f = r[kk + 1UL];
                    if (f == 0) { kk = 8UL; break; }
                    e2 = (cum + f) & 0xFFFFFFFFUL;
                    if (v < e2) {
                        *esc = 0;
                        *sym = r[kk + 9UL];
                        break;
                    }
                    dec4c_excl_push(c, r[kk + 9UL]);
                    cum = e2;
                }
                kk += 1UL;
            }
            c->g_e3a5 = (unsigned char)kk;
            {
                unsigned long hi = (kk == 8UL) ? c->rc_t : e2;
                dec4c_rc_dec(c, cum, hi);
            }
            {
                unsigned long got = kk;
                if (pe != 0) {
                    r[1] = (unsigned char)l90;
                    dec4c_see_bump(c, pe, got == 0);
                }
                if (p16 != 0) {
                    r[f8 + 1UL] = (unsigned char)l88;
                    dec4c_see_bump(c, p16, got == f8);
                }
                if (pedi != 0) {
                    r[k7c + 1UL] = (unsigned char)l80;
                    dec4c_see_bump(c, pedi, got == k7c);
                }
                if (!*esc && ((unsigned)r[(got + 1UL) & 0x0FUL] << 2) >= g6a8)
                    c->g_e3a4 = (g6a8 > 0x10UL) ? 2U : 1U;
            }
        }
    }
    if (see != 0)
        dec4c_see_bump(c, see, *esc == 0);
    /* Rust order2 does not do the light update here; decode_symbol does
     * it for order-4 hits (f<7) and the full o2_update in the tail. */
}

/* 0x40e672 part: order-4 update (update.rs o4_update). */
static void dec4c_o4_update(dec4c_ctx *c, unsigned char sym)
{
    unsigned long bi;
    unsigned char *b;
    unsigned long k;
    int halve = 0;
    unsigned ahv;
    if (c->g45c694 >= 0x40000UL) {
        /* New bucket: pick the least-used slot of the probe chain. */
        unsigned long h = c->g45c694 & 0x7FFFUL;
        unsigned long base = h;
        unsigned long dstv = 0, datv = 0;
        c->g45c694 = h;
        for (;;) {
            unsigned char *e;
            unsigned long slot = base + datv;
            unsigned long sb = ((slot & 0x7FFFUL) << 3) | ((slot >> 15) & 7UL);
            e = c->o4 + sb * 32UL;
            if (e[0] == 0) {
                dstv = datv;
                break;
            }
            {
                unsigned long cslot = base + dstv;
                unsigned long scbi = ((cslot & 0x7FFFUL) << 3) | ((cslot >> 15) & 7UL);
                unsigned char *cur = c->o4 + scbi * 32UL;
                long bb = (long)e[1] + 1L;
                long tt = ((long)cur[1] > bb ? 1L : 0L) + (long)(cur[0x19] & 0x3FUL);
                if ((long)(e[0x19] & 0x3FUL) < tt)
                    dstv = datv;
            }
            datv += 0x8000UL;
            if (datv >= 0x40000UL)
                break;
        }
        if (datv < 0x38000UL) {
            unsigned long slot = base + datv + 0x8000UL;
            unsigned long sb = ((slot & 0x7FFFUL) << 3) | ((slot >> 15) & 7UL);
            c->o4[sb * 32UL] = (unsigned char)(c->o4[sb * 32UL] & 0U);
        }
        {
            unsigned long sbi = (((base + dstv) & 0x7FFFUL) << 3) | (((base + dstv) >> 15) & 7UL);
            unsigned char *nb = c->o4 + sbi * 32UL;
            memset(nb, 0, 32);
            nb[0] = 1;
            nb[0x1A] = 0x20U;
            nb[1] = (unsigned char)(c->g_e3a4 + 1U);
            nb[0x0DUL] = sym;
            nb[28] = (unsigned char)(c->g45c6a4 & 0xFFUL);
            nb[29] = (unsigned char)((c->g45c6a4 >> 8) & 0xFFUL);
            nb[30] = (unsigned char)((c->g45c6a4 >> 16) & 0xFFUL);
            nb[31] = (unsigned char)((c->g45c6a4 >> 24) & 0xFFUL);
        }
        if (c->mchk_p != 0xFFFFFFFFUL)
            c->mchk[c->mchk_p & 0x3FFFFUL] = 0x20U;
        else
            c->bad = 1;
        return;
    }
    bi = ((c->g45c694 & 0x7FFFUL) << 3) | ((c->g45c694 >> 15) & 7UL);
    b = c->o4 + bi * 32UL;
    k = c->g_e3a3;
    if (k == 0xFFUL) {
        k = 0;
        while (b[k + 0x0DUL] != sym) {
            k += 1UL;
            if (k >= 0x0CUL) break;
        }
    }
    if (c->g_e394 != 0 && k < 0x0CUL) {
        long f = (long)b[k + 1UL] >> 6;
        if (f > (long)b[0])
            c->g45d104 = (c->g45d104 + 4UL) & 0xFFFFFFFFUL;
    }
    if (c->g_e397 != 0) {
        unsigned long x, q;
        if (k < 0x0CUL) {
            c->g_e39a = 0;
            c->g_e399 = 0;
            c->g_e39f = 0;
            x = b[k + 1UL];
        } else {
            x = b[0];
        }
        q = dec4c_dv((x << 14) & 0xFFFFFFFFUL, c->g45c698);
        c->g45c658 = (c->g45c658 + dec4c_cost(c, q)) & 0xFFFFFFFFUL;
    }
    ahv = (dec4c_rom8(0x4244a0UL + b[0]) < c->g45c678) ? 1U : 0U;
    if (k < 0x0CUL) {
        unsigned alv = b[0x19] & 0xC0UL;
        long fk = (long)b[k + 1UL] + 1L;
        long half = (long)b[1] >> 1;
        if (fk < half) {
            alv = (alv + 0x40UL) & 0xFFUL;
            if (alv == 0)
                ahv = 1;
        } else {
            alv = (unsigned char)(((long)alv >> 1) & 0xC0L);
        }
        b[0x19] = (unsigned char)((b[0x19] & 0x3FUL) | alv);
        if (k != 0) {
            long hi = (long)b[0x1A] >> 4;
            if (hi >= 4L && (b[0x1A] & 0x0FUL) == k)
                halve = 1;
        }
    }
    if (halve || ahv != 0) {
        int x;
        for (x = 0; x <= 12; x++)
            b[x] = (unsigned char)(((unsigned)b[x] + 1U) >> 1);
    }
    if (k < 0x0CUL) {
        b[k + 1UL] = (unsigned char)(b[k + 1UL] + 2U);
        if ((b[0x19] & 0x3FUL) < 0x3FUL)
            b[0x19] = (unsigned char)(b[0x19] + 1U);
        if ((b[0x1A] & 0x0FUL) == k) {
            if (b[0x1A] < 0xF0U)
                b[0x1A] = (unsigned char)(b[0x1A] + 0x10U);
            if (b[0x1B] < 0xF8U)
                b[0x1B] = (unsigned char)(b[0x1B] + 8U);
        } else {
            b[0x1B] >>= 1;
            b[0x1A] = (unsigned char)k;
        }
        c->g45d120 = (c->g45d120 + 5UL) & 0xFFFFFFFFUL;
    } else {
        b[0] = (unsigned char)(b[0] + 2U);
        k = 1;
        b[0x1B] >>= 1;
        while (b[k + 1UL] != 0) {
            k += 1UL;
            if (k == 0x0BUL) break;
        }
        if (b[k + 1UL] >= 6U) {
            k = 0;
        } else {
            b[k + 1UL] = (unsigned char)(c->g_e3a4 + 2U);
            b[k + 0x0DUL] = sym;
            b[0x1A] = (unsigned char)k;
        }
        {
            long f = ((long)(b[0x19] & 0x3FUL) >> 1);
            b[0x19] = (unsigned char)((b[0x19] & 0xC0UL) + f);
        }
    }
    while (k != 0) {
        unsigned dlv = b[k + 1UL];
        unsigned dhv;
        if (dlv < b[k])
            break;
        dhv = b[k + 0x0CUL];
        b[k + 1UL] = b[k];
        b[k] = (unsigned char)dlv;
        b[k + 0x0CUL] = b[k + 0x0DUL];
        b[k + 0x0DUL] = (unsigned char)dhv;
        k -= 1UL;
        b[0x1A] = (unsigned char)(b[0x1A] - 1U);
    }
    c->g_e3a3 = (unsigned char)k;
    c->g45d138 = (c->g45d138 + 2UL) & 0xFFFFFFFFUL;
    if (c->g45d138 > 0x800UL) {
        c->g45d138 >>= 1;
        c->g45d120 >>= 1;
        c->g45d104 >>= 1;
    }
    if (c->mchk_p != 0xFFFFFFFFUL)
        c->mchk[c->mchk_p & 0x3FFFFUL] = b[0x1A];
    else
        c->bad = 1;
}

/* Part of 0x40d6d1: locate the order-4 bucket of C0 for the rebuild. */
static void dec4c_o4_find_for_update(dec4c_ctx *c)
{
    unsigned long c0 = c->c0;
    c->g45c6a4 = c0;
    c->g45c694 = ((c0 >> 13) ^ c0) & 0x7FFFUL;
    for (;;) {
        unsigned long bi = ((c->g45c694 & 0x7FFFUL) << 3) | ((c->g45c694 >> 15) & 7UL);
        unsigned char *b = c->o4 + bi * 32UL;
        unsigned long ctx = ((unsigned long)b[28]) | ((unsigned long)b[29] << 8) |
                            ((unsigned long)b[30] << 16) | ((unsigned long)b[31] << 24);
        if (b[0] == 0) {
            unsigned hi = (unsigned)(((c->g45c694 >> 16) & 0xFFFFUL) + 4U);
            c->g45c694 = (c->g45c694 & 0xFFFFUL) | ((unsigned long)(hi & 0xFFFFUL) << 16);
            return;
        }
        if (ctx == c->g45c6a4) {
            unsigned long sum = b[0];
            unsigned long blo = b[1];
            unsigned long p = 0;
            c->g_e3a3 = 0xFFU;
            for (;;) {
                p += 1UL;
                sum = (sum + blo) & 0xFFFFFFFFUL;
                if (p >= 0x0CUL) break;
                blo = b[p + 1UL];
                if (blo == 0) break;
            }
            c->g45c678 = sum;
            return;
        }
        c->g45c694 = (c->g45c694 + 0x8000UL) & 0xFFFFFFFFUL;
        if (c->g45c694 >= 0x40000UL)
            return;
    }
}

/* 0x40d6d1: switch from text mode to binary mode. */
static void dec4c_to_binary_mode(dec4c_ctx *c)
{
    unsigned long sv138 = c->g45d138;
    unsigned long sv120 = c->g45d120;
    unsigned long sv104 = c->g45d104;
    unsigned long mask = c->dictmask;
    unsigned char save[256];
    unsigned long dpos, wpos, half, start;
    int i;
    dpos = c->pos;
    for (i = 0; i < 256; i++) {
        save[i] = dec4c_tb(c, dpos);
        dpos = (dpos + 1UL) & mask;
    }
    half = (c->dictsize - 0x140UL) >> 1;
    start = (c->pos + 0x140UL) & mask;
    dpos = (start + half) & mask;
    if (half > c->nbytes)
        dpos = 0;
    c->g_e39b = 0;
    wpos = start;
    while (dpos != c->pos) {
        unsigned blv = (unsigned char)(~dec4c_tb(c, dpos));
        unsigned long srcv = (wpos + 1UL) & mask;
        unsigned st = c->g_e39b;
        if (st == 1) {
            unsigned long ii = (unsigned long)(((unsigned char)(c->g_e39e - 0xA6U) * 0x0BUL) & 0xFFFFFFFFUL);
            c->g_e39b = 0;
            c->dict[wpos] = (unsigned char)(~dec4c_rom8(0x427179UL + ii + blv));
            wpos = srcv;
        } else if (blv < 0x80UL && st == 0) {
            c->dict[wpos] = (unsigned char)(~blv);
            wpos = srcv;
        } else {
            unsigned long dstv = (wpos + 1UL) & mask;
            unsigned bhv = dec4c_rom8(0x42711fUL + blv);
            if (bhv >= 0xC8UL) {
                blv = (blv - c->g_e39b) & 0xFFUL;
                c->g_e39b = 0;
                c->dict[wpos] = (unsigned char)(~blv);
                wpos = dstv;
            } else if (bhv == 0x64UL) {
                c->g_e39b = 1;
                c->g_e39e = (unsigned char)blv;
            } else if (bhv == 0x63UL) {
                c->g_e39b = 0x20U;
            } else {
                unsigned long src = dec4c_rom32(0x426ac3UL + 4UL * bhv);
                unsigned cnt = dec4c_rom8(0x426c53UL + bhv);
                if (c->g_e39b != 0) {
                    c->g_e39b = 0;
                    c->dict[wpos] = (unsigned char)(dec4c_rom8(src) + 0x20UL);
                    wpos = dstv;
                    src = (src + 1UL) & 0xFFFFFFFFUL;
                    cnt = (cnt - 1UL) & 0xFFUL;
                }
                for (;;) {
                    c->dict[wpos] = dec4c_rom8(src);
                    wpos = (wpos + 1UL) & mask;
                    src = (src + 1UL) & 0xFFFFFFFFUL;
                    if (dpos == wpos) break;
                    cnt = (cnt - 1UL) & 0xFFUL;
                    if (cnt == 0) break;
                }
            }
        }
        if (dpos == wpos) break;
        dpos = (dpos + 1UL) & mask;
    }
    c->g45d140 = (c->g45d140 + wpos - c->pos) & mask;
    c->g_e39b = 0;
    c->pos = wpos;
    {
        unsigned long e = wpos;
        for (i = 0; i < 256; i++) {
            c->dict[e] = save[i];
            e = (e + 1UL) & mask;
        }
    }
    {
        unsigned long e = start;
        for (i = 0; i < 0x40; i++) {
            e = (e - 1UL) & mask;
            c->dict[e] = 0;
        }
    }
    if (c->dictsize >= 0x100UL)
        memcpy(c->dict + c->dictsize, c->dict, 0x100UL);
    dec4c_rescale_o4(c);
    memset(c->o2, 0, 0x10000UL * 5UL * sizeof(unsigned long));
    memset(c->mh1, 0xFF, 0x80000UL * sizeof(unsigned long));
    memset(c->mh2, 0xFF, 0x80000UL * sizeof(unsigned long));
    memset(c->mlen, 0, 0x40000UL);
    memset(c->mchk, 0, 0x40000UL);
    c->c0 = c->c1 = c->c2 = 0;
    c->g_e394 = 0;
    c->g_e397 = 0;
    {
        unsigned long pos = c->pos;
        unsigned long srcv;
        unsigned long edx2 = start;
        if (((pos + c->dictsize - start) & mask) > 0x8000UL)
            srcv = (pos + 0xFFFF8000UL) & mask;
        else
            srcv = 0xFFFFFFFFUL;
        while (edx2 != c->pos) {
            unsigned long c0 = c->c0, c1 = c->c1, c2 = c->c2;
            unsigned long h1 = ((c0 >> 13) ^ c0) & 0x3FFFFUL;
            unsigned long h2 = ((((c1 >> 15) ^ c1) << 6) ^ (((c0 >> 15) ^ c0) << 4) ^ ((c2 >> 15) ^ c2)) & 0x3FFFFUL;
            c->mh2[2UL * h2 + 1UL] = c->mh2[2UL * h2];
            c->mh2[2UL * h2] = edx2;
            c->mh1[2UL * h1] = edx2;
            c->mh1[2UL * h1 + 1UL] = c0;
            {
                int learn = 0;
                if (edx2 == srcv) {
                    srcv = 0xFFFFFFFFUL;
                    learn = 1;
                } else if (srcv == 0xFFFFFFFFUL) {
                    learn = 1;
                }
                if (learn) {
                    unsigned char b;
                    dec4c_o4_find_for_update(c);
                    b = dec4c_tb(c, edx2);
                    dec4c_o4_update(c, b);
                }
            }
            {
                unsigned long b = dec4c_tb(c, edx2);
                c->c2 = ((c2 << 8) + (c1 >> 24)) & 0xFFFFFFFFUL;
                c->c1 = ((c1 << 8) + (c0 >> 24)) & 0xFFFFFFFFUL;
                edx2 = (edx2 + 1UL) & mask;
                c->c0 = ((c0 << 8) + b) & 0xFFFFFFFFUL;
            }
            (void)pos;
        }
    }
    c->g45d138 = sv138;
    c->g45d120 = sv120;
    c->g45d104 = sv104;
    dec4c_init_see(c);
    c->text_end = 0xFFFFFFFFUL;
    c->binmode = 1;
    c->seg_end = c->out_pos;
}

/* Cost table (0x429ed4 prefix sum, 0x419ac0) for add_cost. */
static unsigned dec4c_cost(dec4c_ctx *c, unsigned idx)
{
    static unsigned short cost[0x8000];
    static int cost_init = 0;
    unsigned acc = 0;
    unsigned k;
    (void)c;
    if (!cost_init) {
        for (k = 0; k < 0x4000U; k++) {
            unsigned raw = (unsigned)DEC4C_DG_429ED2[2 + 2 * k] |
                           ((unsigned)DEC4C_DG_429ED2[3 + 2 * k] << 8);
            acc = (k == 0) ? raw : ((acc + raw) & 0xFFFFU);
            cost[k] = (unsigned short)acc;
        }
        cost[0x4000] = (unsigned short)(((unsigned)DEC4C_DG_429ED2[0x8003] << 8) & 0xFFFFU);
        cost_init = 1;
    }
    return cost[idx & 0x7FFFUL];
}

static unsigned dec4c_cx(unsigned long c_)
{
    return (c_ >= 0x310UL) ? 0x30FUL : (unsigned)c_;
}

/* order-1 helpers (o1.rs) */
static void dec4c_see_up(dec4c_ctx *c, unsigned long a)
{
    unsigned long *p = &c->see[a & (DEC4C_SEE_LEN - 1UL)];
    *p = (*p + ((0x2000UL - *p) >> 6)) & 0xFFFFFFFFUL;
}

static void dec4c_see_down(dec4c_ctx *c, unsigned long a)
{
    unsigned long *p = &c->see[a & (DEC4C_SEE_LEN - 1UL)];
    *p = (*p - (*p >> 6)) & 0xFFFFFFFFUL;
}

static void dec4c_add_cost(dec4c_ctx *c, unsigned long lo, unsigned long hi)
{
    unsigned long t = c->rc_t;
    unsigned long q = dec4c_dv(((hi - lo) << 14) & 0xFFFFFFFFUL, t);
    c->g45c648 = (c->g45c648 + dec4c_cost(c, (unsigned)q)) & 0xFFFFFFFFUL;
}

/* sum over f[k] where seen[k]!=0 or bm[k] (masked_sum, o1.rs) */
static unsigned dec4c_masked_sum(dec4c_ctx *c, unsigned fo, unsigned seen, const unsigned char bm[256])
{
    unsigned s = 0;
    int k;
    (void)c;
    for (k = 0; k < 256; k++) {
        if (c->o1t[seen][1][k] != 0 || bm[k])
            s = (s + (unsigned)c->o1t[fo][1][k]) & 0xFFFFU;
    }
    return s;
}

/* map the exclusion list to/from delta space around base (excl_map). */
static void dec4c_excl_map(dec4c_ctx *c, unsigned n, unsigned char base, int neg, int back)
{
    unsigned i;
    for (i = 0; i < n; i++) {
        unsigned char x = c->excl[i];
        if (neg)
            x = (unsigned char)(base - x);
        else if (back)
            x = (unsigned char)(x + base);
        else
            x = (unsigned char)(x - base);
        c->excl[i] = x;
    }
}

static unsigned dec4c_excl_total(dec4c_ctx *c, unsigned tree, unsigned frq, unsigned n)
{
    unsigned tot = c->o1t[tree][0][0];
    unsigned i;
    for (i = 0; i < n; i++)
        tot = (tot - c->o1t[frq][1][c->excl[i]]) & 0xFFFFU;
    return tot;
}

static unsigned long dec4c_mru_see(dec4c_ctx *c, unsigned long ctx, unsigned char s,
                                   unsigned long f, unsigned siv, unsigned long escc)
{
    unsigned long t = dec4c_rom32(0x42748dUL + 4UL * (c->mru_n[dec4c_cx(ctx)] & 0x0FUL));
    unsigned long x = dec4c_dv((f + t) << 4, (t + escc + siv) & 0xFFFFFFFFUL);
    unsigned s_ = s;
    if (f <= 0x600UL) {
        if (f > 0x100UL) x |= 0x30UL;
        else if (f > 0x40UL) x |= 0x20UL;
        else x |= 0x10UL;
    }
    if ((ctx & 0xC0UL) == 0xC0UL) x |= 0x40UL;
    if ((s_ & 0xC0UL) == 0xC0UL) x |= (0x80UL << ((s_ == 0xFFUL) ? 1UL : 0UL));
    {
        unsigned long cntv = x | 0x200UL;
        if (c->binmode != 0) {
            /* arena 0x412b8x: (unsigned)(dict_pos - last_pos[s]) is compared
             * WITHOUT the dictionary mask.  After the dictionary wraps the
             * stale entry must read as a huge distance (no recent hit), not
             * as a small masked one. */
            if ((unsigned long)(c->pos - c->lastpos[s_]) <= 0x40UL)
                x = cntv;
        } else if (dec4c_rom8(0x42738dUL + s_) != 0) {
            x = cntv;
        }
        if (c->see[D4C_MARKS + s_] == c->nbytes)
            x |= 0x400UL;
    }
    return x;
}

static unsigned long dec4c_see_total(dec4c_ctx *c, unsigned long siv, unsigned long escc, unsigned long *out_a)
{
    long q = (siv + escc) ? (long)((escc << 5) / (siv + escc)) : 0;
    unsigned long a = D4C_S_O1TOT + (unsigned long)q;
    unsigned long t = (dec4c_dv(siv << 13, c->see[a & (DEC4C_SEE_LEN - 1UL)]) + 1UL) & 0xFFFFFFFFUL;
    if (t >= 0x4000UL) t = 0x3FFFUL;
    *out_a = a;
    return t;
}

/* 0x412257: order-1 symbol (falling back to g45c690 context). */
static unsigned char dec4c_o1_symbol(dec4c_ctx *c, unsigned long ctx)
{
    unsigned n = c->nex;
    unsigned cc = dec4c_cx(ctx);
    unsigned tree = cc;
    unsigned siv = c->o1t[cc][0][0];
    unsigned escc = c->esc[cc];
    unsigned lim = (escc * c->ba3c[cc]) & 0xFFFFFFFFUL;
    unsigned frq = cc;
    unsigned dstv = 0;
    unsigned long v = 0;
    /* 'search */
    if (siv <= lim) {
        unsigned f = dec4c_cx(c->g45c690);
        tree = f;
        frq = f;
        siv = dec4c_excl_total(c, f, f, n);
        v = dec4c_rc_div(c, siv);
        goto descent;
    }
    frq = cc;
    siv = dec4c_excl_total(c, cc, cc, n);
    if (ctx < (0x200UL >> (unsigned)c->binmode)) {
        unsigned char s = c->mru_s[cc];
        int excluded = 0;
        unsigned i;
        for (i = 0; i < n; i++)
            if (c->excl[i] == s) { excluded = 1; break; }
        if (!excluded) {
            unsigned long f = c->o1t[cc][1][s];
            unsigned long x = dec4c_mru_see(c, ctx, s, f, siv, escc);
            unsigned long pa = D4C_S_O1MRU + x;
            unsigned long p = c->see[pa & (DEC4C_SEE_LEN - 1UL)];
            unsigned long vv = dec4c_rc_div(c, 0x2000UL) & 0xFFFFUL;
            int hit = (vv < p) ? 1 : 0;
            unsigned long lo = hit ? 0UL : p;
            unsigned long hi = hit ? p : 0x2000UL;
            unsigned long cst = dec4c_cost(c, 2UL * (hi - lo));
            if (!hit) {
                if (n < 256U) {
                    c->excl[n] = s;
                    n += 1U;
                }
                siv = (siv - (unsigned short)f) & 0xFFFFU;
            }
            c->g45c648 = (c->g45c648 + cst) & 0xFFFFFFFFUL;
            dec4c_rc_dec(c, lo, hi);
            if (hit) {
                dec4c_see_up(c, pa);
                return s;
            }
            dec4c_see_down(c, pa);
        }
    }
    if (siv == 0) {
        unsigned f = dec4c_cx(c->g45c690);
        tree = f;
        frq = f;
        siv = dec4c_excl_total(c, f, f, n);
        v = dec4c_rc_div(c, siv);
        goto descent;
    }
    {
        unsigned long t;
        if (ctx < 0x200UL && siv < 0x2000UL) {
            unsigned long a;
            t = dec4c_see_total(c, siv, escc, &a);
            dstv = (unsigned)a;
        } else {
            t = (siv + escc) & 0xFFFFFFFFUL;
        }
        {
            unsigned long vv = dec4c_rc_div(c, t);
            if ((unsigned short)vv < siv) {
                v = vv;
                goto descent;
            }
            /* escape to the fallback context */
            {
                unsigned char bm[256];
                unsigned i;
                unsigned long lo = siv;
                unsigned long hi = c->rc_t;
                unsigned fo_i = dec4c_cx(c->g45c690);
                unsigned s16;
                unsigned long t2;
                unsigned long esc_total;
                unsigned k;
                int fail = 0;
                memset(bm, 0, sizeof(bm));
                for (i = 0; i < n; i++)
                    bm[c->excl[i]] = 1;
                dec4c_add_cost(c, lo, hi);
                dec4c_rc_dec(c, lo, hi);
                s16 = (unsigned)dec4c_masked_sum(c, fo_i, cc, bm);
                t2 = ((unsigned long)c->o1t[fo_i][0][0] - s16) & 0xFFFFFFFFUL;
                esc_total = dec4c_rc_div(c, t2) & 0xFFFFUL;
                s16 = 0;
                k = 0x100U;
                for (;;) {
                    k -= 1U;
                    if (c->o1t[cc][1][k] == 0 && !bm[k])
                        s16 = (s16 + (unsigned)c->o1t[fo_i][1][k]) & 0xFFFFU;
                    if (s16 > esc_total)
                        break;
                    if (k == 0) {
                        fail = 1;
                        break;
                    }
                }
                hi = s16;
                lo = (hi - (unsigned)c->o1t[fo_i][1][k]) & 0xFFFFFFFFUL;
                if (fail)
                    c->bad = 1;
                dec4c_add_cost(c, lo, hi);
                dec4c_rc_dec(c, lo, hi);
                if (dstv != 0)
                    dec4c_see_down(c, dstv);
                return (unsigned char)k;
            }
        }
    }
descent:
    {
        unsigned rem = (siv - (unsigned short)v) & 0xFFFFU;
        unsigned cur = 0;
        unsigned char sym;
        unsigned long hi, lo;
        if (n != 0) {
            unsigned ptrs[32];
            unsigned fr[32];
            unsigned len = (n < 32U) ? n : 32U;
            unsigned i;
            unsigned step = 0x80U;
            for (i = 0; i < len; i++) {
                ptrs[i] = c->excl[i];
                fr[i] = c->o1t[frq][1][ptrs[i]];
            }
            while (step != 0) {
                unsigned node = cur + step;
                unsigned long sum = 0;
                unsigned tn;
                for (i = 0; i < len; i++)
                    if (node > ptrs[i])
                        sum = (sum + fr[i]) & 0xFFFFFFFFUL;
                tn = c->o1t[tree][0][node & 0xFFUL];
                if ((long)tn < (long)rem + (long)(unsigned short)sum) {
                    i = 0;
                    while (i < len) {
                        if (node > ptrs[i]) {
                            len -= 1U;
                            ptrs[i] = ptrs[len];
                            fr[i] = fr[len];
                            continue;
                        }
                        i += 1U;
                    }
                    cur = node;
                    rem = (rem - ((tn - (unsigned short)sum) & 0xFFFFU)) & 0xFFFFU;
                }
                step >>= 1;
            }
        } else {
            unsigned dlv = 0x80U;
            while (dlv != 0) {
                unsigned node = cur + dlv;
                unsigned tn = c->o1t[tree][0][node & 0xFFUL];
                if (rem > tn) {
                    cur = node;
                    rem = (rem - tn) & 0xFFFFU;
                }
                dlv >>= 1;
            }
        }
        sym = (unsigned char)(cur & 0xFFUL);
        hi = ((rem + (unsigned short)v) & 0xFFFFU);
        lo = (hi - (unsigned)c->o1t[frq][1][sym]) & 0xFFFFFFFFUL;
        dec4c_add_cost(c, lo, hi);
        dec4c_rc_dec(c, lo, hi);
        if (dstv != 0)
            dec4c_see_up(c, dstv);
        return sym;
    }
}

/* 0x412a59: code length of sym in ctx (encoder-side simulation). */
static void dec4c_o1_cost(dec4c_ctx *c, unsigned long ctx, unsigned char sym)
{
    unsigned cc = dec4c_cx(ctx);
    unsigned escc = c->esc[cc];
    unsigned lim = (c->ba3c[cc] * escc) & 0xFFFFFFFFUL;
    int has_esc_from = 0;
    unsigned esc_from = 0;
    unsigned tot = c->o1t[cc][0][0];
    unsigned n = c->nex;
    unsigned basv = (unsigned)ctx;
    unsigned f14 = cc;
    unsigned s0c = 0xFFFFU;
    /* 'tail */
    if (tot <= lim) {
        basv = (unsigned)c->g45c690;
        f14 = dec4c_cx(basv);
    } else {
        int done = 0;
        f14 = cc;
        if (ctx < (0x200UL >> (unsigned)c->binmode)) {
            unsigned char s = c->mru_s[cc];
            int excluded = 0;
            unsigned i;
            for (i = 0; i < n; i++)
                if (c->excl[i] == s) { excluded = 1; break; }
            if (!excluded) {
                unsigned long f = c->o1t[cc][1][s];
                unsigned long x;
                unsigned long pa, p;
                int hit;
                unsigned cntv;
                unsigned long cst;
                s0c = dec4c_excl_total(c, cc, cc, n);
                x = dec4c_mru_see(c, ctx, s, f, s0c, escc);
                pa = D4C_S_O1MRU + x;
                p = c->see[pa & (DEC4C_SEE_LEN - 1UL)];
                hit = (s == sym) ? 1 : 0;
                if (hit) {
                    cntv = (unsigned)p;
                } else {
                    cntv = (unsigned)((0x2000UL - p) & 0xFFFFFFFFUL);
                    if (n < 256U) {
                        c->excl[n] = s;
                        n += 1U;
                    }
                    s0c = (s0c - (unsigned short)f) & 0xFFFFU;
                }
                cst = dec4c_cost(c, 2UL * cntv);
                c->g45c648 = (c->g45c648 + cst) & 0xFFFFFFFFUL;
                if (hit) {
                    dec4c_see_up(c, pa);
                    return;
                }
                dec4c_see_down(c, pa);
            }
        }
        if (c->o1t[cc][1][sym] != 0) {
            done = 1;
        } else {
            if (s0c == 0xFFFFU)
                s0c = dec4c_excl_total(c, cc, cc, n);
            if (s0c != 0) {
                unsigned siv = s0c;
                unsigned long t;
                if (ctx < 0x200UL && s0c < 0x2000UL) {
                    unsigned long a;
                    t = dec4c_see_total(c, siv, escc, &a);
                } else {
                    t = (escc + siv) & 0xFFFFFFFFUL;
                }
                {
                    unsigned long q = dec4c_dv(((t - siv) << 14) & 0xFFFFFFFFUL, t);
                    c->g45c648 = (c->g45c648 + dec4c_cost(c, (unsigned)q)) & 0xFFFFFFFFUL;
                }
                has_esc_from = 1;
                esc_from = f14;
            }
            basv = (unsigned)c->g45c690;
            f14 = dec4c_cx(basv);
        }
        if (done) {
            /* fall through to 0x412d69 with basv=ctx, f14=cc */
        }
    }
    /* 0x412d69 */
    {
        unsigned long t;
        if (basv < 0x301UL) {
            unsigned cx16 = dec4c_excl_total(c, dec4c_cx(basv), f14, n);
            unsigned e = c->esc[dec4c_cx(basv)];
            if (basv < 0x200UL && cx16 < 0x2000UL) {
                unsigned long a;
                t = dec4c_see_total(c, cx16, e, &a);
            } else {
                t = (e + cx16) & 0xFFFFFFFFUL;
            }
        } else {
            unsigned s = c->o1t[dec4c_cx(basv)][0][0];
            if (has_esc_from) {
                unsigned char bm[256];
                unsigned i;
                memset(bm, 0, sizeof(bm));
                for (i = 0; i < n; i++)
                    bm[c->excl[i]] = 1;
                s = (s - dec4c_masked_sum(c, f14, esc_from, bm)) & 0xFFFFU;
            } else {
                unsigned i;
                for (i = 0; i < n; i++)
                    s = (s - (unsigned)c->o1t[f14][1][c->excl[i]]) & 0xFFFFU;
            }
            t = s;
        }
        t &= 0xFFFFUL;
        {
            unsigned f = c->o1t[f14][1][sym];
            if (t == 0) { c->bad = 1; return; }
            {
                unsigned long q = ((unsigned long)f << 14) / t;
                c->g45c648 = (c->g45c648 + dec4c_cost(c, (unsigned)q)) & 0xFFFFFFFFUL;
            }
        }
    }
}

/* 0x411f8b: order-1 stage. */
static unsigned char dec4c_o1_decode(dec4c_ctx *c, unsigned long ctx, unsigned long fb)
{
    int single = 1;
    unsigned flag = (fb == 0x301UL) ? 1U : 0U;
    unsigned ring = (unsigned)((c->g45c650 + 1UL) & 0xFFUL);
    unsigned char sym;
    c->g45c648 = 0;
    c->g45c650 = ring;
    {
        unsigned old = (unsigned)c->ring[ring];
        unsigned long s = (c->g45c644 - old) & 0xFFFFFFFFUL;
        c->g45c690 = fb;
        c->g45c644 = s;
        if (!(s >= 0x6D4250UL && s < 0xFEFF01UL)) {
            sym = dec4c_o1_symbol(c, ctx);
            if (c->g_e39f != 0)
                c->g45c668 = (c->g45c668 + c->g45c648) & 0xFFFFFFFFUL;
        } else if (s >= 0x91ADC0UL) {
            unsigned long v = dec4c_rc_div(c, 0x100UL);
            sym = (unsigned char)v;
            dec4c_rc_dec(c, v & 0xFFUL, (v & 0xFFUL) + 1UL);
            if (c->g_e39f != 0)
                c->g45c668 = (c->g45c668 + 0x9240UL) & 0xFFFFFFFFUL;
            dec4c_o1_cost(c, ctx, sym);
        } else {
            unsigned o0 = 0x302UL + flag;
            unsigned long saved_cost;
            unsigned long c10;
            if (c->avg_a[flag] >= c->avg_b[flag]) {
                sym = dec4c_o1_symbol(c, ctx);
                saved_cost = c->g45c648;
                c->g45c690 = o0;
                c->g45c648 = 0;
                dec4c_o1_cost(c, 0x200UL, sym);
                c10 = c->g45c648;
                c->g45c648 = saved_cost;
            } else {
                c->g45c690 = o0;
                sym = dec4c_o1_symbol(c, 0x200UL);
                saved_cost = c->g45c648;
                c->g45c690 = fb;
                c->g45c648 = 0;
                c10 = saved_cost;
                dec4c_o1_cost(c, ctx, sym);
            }
            if (c->g_e39f != 0)
                c->g45c668 = (c->g45c668 + saved_cost) & 0xFFFFFFFFUL;
            {
                unsigned i = c->g_e3a1 & 3U;
                c->qsym[i] = sym;
                c->ring[c->g45c650 & 0xFFUL] = saved_cost;
                c->qctx[i] = 0x200UL;
                c->qfb[i] = o0;
                c->avg_a[flag] = ((c->avg_a[flag] * 0x1FUL) >> 5) + c10;
                single = 0;
                c->g45c644 = (c->g45c644 + saved_cost) & 0xFFFFFFFFUL;
                c->g_e3a1 = (unsigned char)(c->g_e3a1 + 1U);
            }
        }
    }
    {
        unsigned i = c->g_e3a1 & 3U;
        unsigned long cc;
        c->g_e3a1 = (unsigned char)(c->g_e3a1 + 1U);
        c->qfb[i] = fb;
        c->qctx[i] = ctx;
        c->qsym[i] = sym;
        cc = c->g45c648;
        if (single) {
            c->ring[c->g45c650 & 0xFFUL] = cc;
            c->g45c644 = (c->g45c644 + cc) & 0xFFFFFFFFUL;
            c->avg_a[flag] = ((c->avg_a[flag] * 0x1FUL) >> 5) + cc;
        }
        c->avg_b[flag] = ((c->avg_b[flag] * 0x1FUL) >> 5) + cc;
    }
    return sym;
}

/* 0x412ed2: queue the update for sym in ctx. */
static void dec4c_o1_account(dec4c_ctx *c, unsigned long ctx, unsigned long fb, unsigned char sym)
{
    unsigned i;
    unsigned f;
    unsigned long cc;
    c->g45c690 = fb;
    c->g45c648 = 0;
    dec4c_o1_cost(c, ctx, sym);
    i = c->g_e3a1 & 3U;
    c->g_e3a1 = (unsigned char)(c->g_e3a1 + 1U);
    c->qfb[i] = c->g45c690;
    c->qctx[i] = ctx;
    c->qsym[i] = sym;
    f = (fb == 0x301UL) ? 1U : 0U;
    cc = c->g45c648;
    c->avg_a[f] = cc + ((c->avg_a[f] * 0x1FUL) >> 5);
    c->avg_b[f] = cc + ((c->avg_b[f] * 0x1FUL) >> 5);
}

/* 0x412f80: cost into the average 0x45c640. */
static void dec4c_o1_cost_avg(dec4c_ctx *c, unsigned long ctx, unsigned long fb, unsigned char sym)
{
    c->g45c690 = fb;
    c->g45c648 = 0;
    dec4c_o1_cost(c, ctx, sym);
    c->g45c640 = ((c->g45c640 * 0x1FUL) >> 5) + c->g45c648;
}

/* Fenwick add (o1.rs fen_add). */
static unsigned dec4c_fen_add(dec4c_ctx *c, unsigned ctx, unsigned char sym, unsigned inc)
{
    unsigned short *f = c->o1t[ctx][1];
    unsigned short *t = c->o1t[ctx][0];
    unsigned b;
    int k;
    f[sym] = (unsigned short)(f[sym] + inc);
    b = (unsigned)(sym + 1U) & 0xFFU;
    for (k = 0; k < 8; k++) {
        unsigned d = (b != 0) ? inc : 0U;
        t[b] = (unsigned short)(t[b] + d);
        b = (b + (b & (~b + 1U))) & 0xFFU;
    }
    t[0] = (unsigned short)(t[0] + inc);
    return t[0];
}

/* 0x40e3eb: halve frequencies (pair-wise). */
static void dec4c_o1_halve(dec4c_ctx *c, unsigned long ctx_)
{
    unsigned cidx = dec4c_cx(ctx_);
    unsigned long sum = 0;
    int k;
    for (k = 0; k < 0x80; k++) {
        unsigned long w = (unsigned)c->o1t[cidx][1][2 * k] |
                          ((unsigned long)c->o1t[cidx][1][2 * k + 1] << 16);
        unsigned long v = ((w + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
        c->o1t[cidx][1][2 * k] = (unsigned short)(v & 0xFFFFUL);
        c->o1t[cidx][1][2 * k + 1] = (unsigned short)((v >> 16) & 0xFFFFUL);
        sum = (sum + v) & 0xFFFFFFFFUL;
    }
    for (k = 0; k < 255; k++)
        c->o1t[cidx][0][k + 1] = c->o1t[cidx][1][k];
    c->o1t[cidx][0][0] = (unsigned short)(((sum & 0xFFFFUL) + (sum >> 16)) & 0xFFFFUL);
    dec4c_fenwick_build(c->o1t[cidx][0]);
}

/* 0x40e672: bump symbol in fallback context. */
static void dec4c_o1_bump(dec4c_ctx *c, unsigned char sym, unsigned long idx)
{
    unsigned cc = dec4c_cx(c->g45c690);
    unsigned inc = c->inc_b[dec4c_cx(idx)];
    if (dec4c_fen_add(c, cc, sym, inc) >= 0x4000U)
        dec4c_o1_halve(c, cc);
}

/* 0x40e6d5: update context ctx with symbol sym. */
static void dec4c_o1_update(dec4c_ctx *c, unsigned char sym, unsigned long ctx)
{
    unsigned cc = dec4c_cx(ctx);
    unsigned div2 = c->inc_u[cc];
    unsigned f = c->o1t[cc][1][sym];
    unsigned esc = c->esc[cc];
    unsigned long acc = ((unsigned long)esc + f) & 0xFFFFFFFFUL;
    unsigned long ae;
    if (c->mru_s[cc] == sym) {
        acc = (acc << 5) & 0xFFFFFFFFUL;
        if (c->mru_n[cc] < 0x0FU)
            c->mru_n[cc] = (unsigned char)(c->mru_n[cc] + 1U);
    } else {
        c->mru_n[cc] = 0;
        c->mru_s[cc] = sym;
    }
    ae = (c->ae38[cc] - acc) & 0xFFFFFFFFUL;
    c->ae38[cc] = ae;
    if (f == 0) {
        dec4c_o1_bump(c, sym, ctx);
        c->ba3c[cc] = (c->ba3c[cc] + c->binmode) & 0xFFFFFFFFUL;
        esc = (esc + div2) & 0xFFFFU;
    } else {
        unsigned k = c->ba3c[cc];
        if ((unsigned)c->o1t[cc][0][0] <= (k * (unsigned long)esc & 0xFFFFFFFFUL))
            dec4c_o1_bump(c, sym, ctx);
        c->ba3c[cc] = k >> 4;
    }
    c->esc[cc] = (unsigned short)esc;
    {
        unsigned tot = dec4c_fen_add(c, cc, sym, div2);
        if ((long)esc + (long)tot >= 0x4000L || (long)(int)ae < 0)
            dec4c_o1_rescale(c, ctx);
    }
}

/* 0x41030f: decode one symbol (order-4 -> order-2/delta -> order-1).
 * Orchestration matches Rust lit.rs decode_symbol. */
static void dec4c_decode_symbol(dec4c_ctx *c)
{
    unsigned long l10c, c6a4, o2i, lfc;
    unsigned char l50, rec[20], b[32], sym = 0;
    int esc = 1;
    unsigned l3c;
    int l52, l4e, l40, l76;
    unsigned long l104, l100;
    int bm = (c->binmode != 0) ? 1 : 0;
    int mru_upd = 1;
    unsigned long a4 = 0xFFFFFFFFUL; /* delta entry or none */
    int l54 = 1, l2c = 0;
    /* Preamble */
    dec4c_decode_symbol_pre(c, &l10c, &l50, rec);
    c6a4 = c->g45c6a4;
    o2i = c->g45c69c & 0x1FFFFUL;
    lfc = ((c6a4 & 0xC0UL) == 0xC0UL) ? 0x10UL : 0UL;
    l3c = rec[9UL + (rec[0x13] & 0x0FUL)];
    l104 = dec4c_rom32(0x42750dUL + 4UL * ((rec[0x13] >> 4) & 0x0FUL));
    l100 = dec4c_rom32(0x4274cdUL + 4UL * (c->mru_n[l10c & 0x3FFUL] & 0x0FUL));
    l40 = bm;
    l76 = (c->mmtype != 0 && !(c->g45c674 > 4UL && dec4c_g68c(c) > 0x3FEUL)) ? 1 : 0;
    if (l40) {
        unsigned long a = c->avg_b[0], bb = c->avg_b[1];
        if (c->g_e38d != 0)
            l40 = ((a * 0x1FUL) < ((bb << 5) & 0xFFFFFFFFUL)) ? 1 : 0;
        else
            l40 = (a < bb) ? 1 : 0;
    }
    l52 = (l40 && l76) ? 1 : 0;
    l4e = (l52 && c->g45c640 < c->g45c654) ? 1 : 0;
    /* Order-4 probe */
    if (dec4c_o4_scan(c, c6a4, rec, b)) {
        unsigned long see_esc = 0;
        int e2 = 1;
        unsigned char s2 = 0;
        c->g_e394 = 1;
        c->g45c6b0 = (c->g45c6b0 + 1UL) & 0xFFFFFFFFUL;
        c->g_e397 = (unsigned char)l52;
        c->g_e394 = (unsigned char)(l4e ? 0 : 1);
        if (!l4e) {
            if (b[2] == 0) {
                dec4c_o4_single(c, b, rec, c6a4, l10c, l3c, l50, bm, &e2, &s2, &see_esc);
            } else {
                unsigned long c698 = c->g45c698;
                dec4c_o4_multi(c, b, rec, c6a4, l10c, l3c, l50, bm, c698, lfc, &e2, &s2, &see_esc);
            }
            esc = e2;
            sym = s2;
            if (see_esc != 0)
                dec4c_see_bump(c, see_esc, esc == 0);
        } else {
            esc = 1;
            sym = 0;
        }
    }
    /* 'done: order-4 success -> light update; Rust reads the persistent
     * bucket: o4_bytes(&self.o4[bucket])[(g_e3a3+1)&0x1f]. The local b[]
     * has been SEE-mutated by o4_multi, so it must not gate this. */
    if (!esc) {
        unsigned long bk = ((c->g45c694 & 0x7FFFUL) << 3) | ((c->g45c694 >> 15) & 7UL);
        unsigned f = c->o4[bk * 32UL + ((c->g_e3a3 + 1UL) & 0x1FUL)];
        if (f < 7U)
            dec4c_o2_update_light(c, sym);
    } else {
        /* Order-2 / delta stage */
        if (bm)
            a4 = dec4c_delta_lookup(c);
        c->g45c6b0 >>= 3;
        if (a4 != 0xFFFFFFFFUL) {
            unsigned char e[4];
            dec4c_de(c, a4, e);
            c->g45c6a0 = (c->g45c6a0 + 2UL) & 0xFFFFFFFFUL;
            {
                unsigned e0 = e[0], e1 = e[1], e3 = e[3] & 0x7FUL;
                int v = (rec[0x13] >= 0x90UL || (e0 * 5UL > e1 * 3UL && e3 <= 2UL)) ? 1 : 0;
                c->g_e39d = (unsigned char)v;
            }
        } else {
            c->g_e39d = 1;
        }
        l54 = 1;
        l2c = 0;
        for (;;) {
            if (c->g_e39d != 0 || l2c) {
                unsigned char r11;
                int v;
                l54 = 0;
                dec4c_o2r(c, o2i, rec);
                r11 = rec[0x11];
                v = (c->g45c6ac == 0 || r11 == 0 || (c->g45c6b4 < 0xFFFFUL && dec4c_g68c(c) <= 0x3FEUL)) ? 1 : 0;
                c->g_e39d = (unsigned char)v;
                if (v && rec[0] != 0)
                    dec4c_order2(c, o2i, l50, l3c, l10c, c6a4, lfc, l104, l100, l52, l4e, bm, a4, &esc, &sym);
            }
            if (l2c)
                break;
            c->g_e393 = (unsigned char)esc;
            if (esc) {
                int clo = 1;
                l2c = 1;
                if (!bm)
                    c->g_e393 = 0;
                if (a4 != 0xFFFFFFFFUL) {
                    unsigned char e[4];
                    unsigned char e2;
                    dec4c_de(c, a4, e);
                    c->g45c6a0 >>= 2;
                    e2 = e[2];
                    /* excl_contains(e2) */
                    {
                        unsigned i;
                        int contains = 0;
                        for (i = 0; i < c->nex; i++)
                            if (c->excl[i] == e2) { contains = 1; break; }
                        if (contains) {
                            clo = 0;
                            if (e[0] > 3U || ((e[1] >> 6) > e[0]))
                                c->g_e393 = 0;
                        }
                    }
                    if (clo) {
                        c->g_e399 = (unsigned char)l52;
                        if (!l4e) {
                            unsigned long arr0, arr1, t, v;
                            if (c->pair_p != 0xFFFFFFFFUL) {
                                unsigned long p = c->pair_p & 0x7EUL;
                                arr0 = c->pair[p];
                                arr1 = c->pair[p + 1UL];
                            } else {
                                arr0 = e[0];
                                arr1 = e[1];
                            }
                            arr1 = (arr1 + dec4c_rom32(0x4274cdUL + 4UL * ((e[3] >> 3) & 0x0FUL))) & 0xFFFFFFFFUL;
                            /* Rust: arr[(e2==l28)] += l108 etc. (add to arr[1] if match, else arr[0]).
                             * l28/l108 come from the order-4 bucket and stay valid even
                             * when the order-4 coder was skipped (c->g_l28/c->g_l108). */
                            if (e2 == c->g_l28)
                                arr1 = (arr1 + c->g_l108) & 0xFFFFFFFFUL;
                            else
                                arr0 = (arr0 + c->g_l108) & 0xFFFFFFFFUL;
                            if (e2 == l3c)
                                arr1 = (arr1 + l104) & 0xFFFFFFFFUL;
                            else
                                arr0 = (arr0 + l104) & 0xFFFFFFFFUL;
                            if (e2 == l50)
                                arr1 = (arr1 + l100) & 0xFFFFFFFFUL;
                            else
                                arr0 = (arr0 + l100) & 0xFFFFFFFFUL;
                            t = (arr0 + arr1) & 0xFFFFFFFFUL;
                            v = dec4c_rc_div(c, t) & 0xFFFFUL;
                            if (v < arr0) {
                                if (c->g_e398 != 0) {
                                    unsigned ga = c->g45c68c & 0x3FFUL;
                                    unsigned gb = c->g45c688 & 0x3FFUL;
                                    c->g6b8[ga] >>= 1;
                                    c->g6b8[gb] >>= 1;
                                    c->g45c6b4 = (c->g45c6b4 >> 1) + 0xFFUL;
                                }
                                dec4c_excl_push(c, e2);
                                dec4c_rc_dec(c, 0, arr0);
                            } else {
                                if (c->g_e398 != 0) {
                                    unsigned ga = c->g45c68c & 0x3FFUL;
                                    unsigned gb = c->g45c688 & 0x3FFUL;
                                    unsigned long old = c->g45c6b4;
                                    c->g6b8[ga] = (c->g6b8[ga] + old) & 0xFFFFFFFFUL;
                                    c->g45c6b4 = (old << 1) & 0xFFFFFFFFUL;
                                    c->g6b8[gb] = (c->g6b8[gb] + old) & 0xFFFFFFFFUL;
                                    if (c->g45c6b4 > 0xFFFFUL)
                                        c->g45c6b4 = 0xFFFFUL;
                                }
                                esc = 0;
                                sym = e2;
                                dec4c_rc_dec(c, arr0, t);
                            }
                        }
                    }
                }
            }
            if (!esc || !l54)
                break;
        }
        /* Order-1 */
        if (esc) {
            unsigned long cntv = l10c;
            unsigned char l1c = 0;
            unsigned long l60 = 0, l5c = 0;
            unsigned nex = c->nex;
            mru_upd = 0;
            c->g_e39f = (unsigned char)l52;
            if (bm) {
                l1c = dec4c_hist(c, c->g45c674);
                if (c->g45c674 > 1UL && dec4c_g68c(c) != 0) {
                    cntv = (unsigned long)l1c + 0x100UL;
                    mru_upd = 1;
                }
                if (l76) {
                    l76 = c->g_e38e;
                    l1c = c->g_e38b;
                    l60 = c->mmctx;
                    l5c = (unsigned long)c->mmtype + 0x304UL;
                } else {
                    unsigned char h = dec4c_hist(c, (c->g45c674 * 2UL) & 0xFFFFFFFFUL);
                    l60 = dec4c_rom32(0x42472cUL + 4UL * ((unsigned char)(l1c - h))) + 0x201UL;
                    l5c = 0x304UL;
                }
            }
            if (l40) {
                unsigned char s;
                dec4c_excl_map(c, nex, l1c, l76, 0);
                s = dec4c_o1_decode(c, l60, l5c);
                sym = l76 ? (unsigned char)(l1c - s) : (unsigned char)(l1c + s);
                dec4c_excl_map(c, nex, l1c, l76, 1);
                dec4c_o1_account(c, cntv, 0x301UL, sym);
            } else {
                unsigned char s = dec4c_o1_decode(c, cntv, 0x301UL);
                sym = s;
                if (bm) {
                    unsigned char d = l76 ? (unsigned char)(l1c - s) : (unsigned char)(s - l1c);
                    dec4c_excl_map(c, nex, l1c, l76, 0);
                    dec4c_o1_account(c, l60, l5c, d);
                }
            }
        }
    }
    /* Updates (0x411e02) */
    dec4c_o4_update(c, sym);
    if (c->g_e39d != 0)
        dec4c_o2_update(c, sym);
    if (c->g_e393 != 0)
        dec4c_delta_update(c, sym);
    if (l52) {
        unsigned char alo = (unsigned char)(sym - c->g_e38b);
        unsigned long fb_ctx, mm_ctx;
        if (c->g_e38e != 0)
            alo = (unsigned char)(0U - alo);
        fb_ctx = (unsigned long)c->mmtype + 0x304UL;
        mm_ctx = c->mmctx;
        c->nex = 0;
        dec4c_o1_cost_avg(c, mm_ctx, fb_ctx, alo);
        if (c->g_e39f != 0)
            c->g45c658 = (c->g45c658 + c->g45c668) & 0xFFFFFFFFUL;
        c->g45c654 = (((c->g45c654 * 0x1FUL) >> 5) + c->g45c658) & 0xFFFFFFFFUL;
    }
    for (;;) {
        unsigned long i = (c->g_e3a1 - 1U) & 0xFFUL;
        c->g_e3a1 = (unsigned char)i;
        if (i == 0xFFUL)
            break;
        i &= 3UL;
        c->g45c690 = c->qfb[i];
        {
            unsigned long ctx = c->qctx[i];
            unsigned char s = c->qsym[i];
            if (ctx == 0x200UL)
                dec4c_o1_bump(c, s, ctx);
            else
                dec4c_o1_update(c, s, ctx);
        }
    }
    dec4c_output(c, sym);
    if (mru_upd) {
        unsigned cc = dec4c_cx(l10c);
        if (c->mru_s[cc] == sym) {
            if (c->mru_n[cc] < 0x0FUL)
                c->mru_n[cc] = (unsigned char)(c->mru_n[cc] + 1U);
        } else {
            c->mru_n[cc] = 0;
            c->mru_s[cc] = sym;
        }
    }
    dec4c_after_symbol(c, sym, 1);
    dec4c_push_hist(c, sym);
}

int UhaDec4_06(void *ctx, const unsigned long *cum, unsigned long ncum,
                         uharc_dec_read_cb rcb, void *rctx,
                         uharc_dec_write_cb wcb, void *wctx,
                         unsigned long out_len)
{
    dec4c_ctx *c = (dec4c_ctx *)ctx;
    unsigned char tmp[4096];
    unsigned long got, i;
    /* Drain the packed stream into memory (decoder reads it whole). */
    unsigned long cap, len;
    if (!c || !rcb || !wcb)
        return 1;
    if (out_len == 0)
        return 0;
    /* File-boundary scan for kennedy (flags2 bit6): first file with
     * size 0xFB670 sets kennedy_end = predecessor end (0 if first). */
    if ((g_c_flags2 & 0x40) && cum && ncum) {
        unsigned long prev = 0;
        for (i = 0; i < ncum; i++) {
            unsigned e = (unsigned)cum[i];
            if (e - prev == 0xFB670U) break;
            prev = e;
        }
        c->kennedy_end = prev;
        c->g_e395 = (prev == 0) ? 10 : 0;
    }
    c->out_pos = 0;
    c->out_len = out_len;
    c->wcb = wcb;
    c->wctx = wctx;
    c->out_err = 0;
    c->out_fill = 0;
    c->out_cap = 0x10000U;
    c->outflush = (unsigned char *)malloc((size_t)c->out_cap);
    if (!c->outflush) return 2;
    cap = 65536UL;
    c->inbuf = (unsigned char *)malloc((size_t)cap);
    if (!c->inbuf) { free(c->outflush); c->outflush = 0; return 2; }
    len = 0;
    for (;;) {
        got = (unsigned)rcb(rctx, tmp, (unsigned)sizeof(tmp));
        if (got == 0) break;
        if (len + got > cap) {
            unsigned char *nb;
            nb = (unsigned char *)realloc(c->inbuf, (size_t)(len + got + 8192UL));
            if (!nb) { free(c->outflush); c->outflush = 0; return 2; }
            c->inbuf = nb;
            cap = len + got + 8192UL;
        }
        memcpy(c->inbuf + len, tmp, got);
        len += got;
        if (len >= c->out_len * 2UL + 4096UL) break;
    }
    c->in_len = len;
    c->in_pos = 0;
    /* File-end table for new_segment (padded with zeros like orig). */
    if (cum && ncum) {
        unsigned long capn = ((4UL * ncum + 16UL + 0xFFFUL) & ~0xFFFUL) / 4UL;
        c->fe = (unsigned long *)calloc((size_t)capn, sizeof(unsigned long));
        if (!c->fe) return 2;
        for (i = 0; i < ncum; i++)
            c->fe[i] = (unsigned long)cum[i];
        c->fe_n = capn;
        c->fe_idx = 0;
        c->fe_on = 1;
    }
    c->g45d124 = c->kennedy_end;
    c->text_end = c->sw_pos;
    if (c->binmode != 0)
        c->text_end = 0xFFFFFFFFUL; /* never switches */
    c->seg_end = 0;
    dec4c_init_model(c);
    dec4c_rc_init(c);
    dec4c_read_switch_pos(c);
    /* Driver 0x40f962 */
    while (c->out_pos < c->out_len && !c->bad) {
        if (c->out_pos >= c->text_end && c->binmode == 0)
            dec4c_to_binary_mode(c);
        if (c->out_pos >= c->seg_end)
            dec4c_new_segment(c);
        if (!dec4c_match_model(c))
            dec4c_decode_symbol(c);
        if (c->bad)
            break;
    }
    if (c->out_err) {
        free(c->outflush);
        c->outflush = 0;
        return 4;
    }
    if (c->out_pos >= c->out_len) {
        if (!dec4c_flush(c)) {
            free(c->outflush);
            c->outflush = 0;
            return 4;
        }
        free(c->outflush);
        c->outflush = 0;
        return 0;
    }
    /* On a data error the arena has already streamed the bytes it decoded,
     * so hand over the partial output too instead of dropping it. */
    dec4c_flush(c);
    free(c->outflush);
    c->outflush = 0;
    if (c->out_err) return 4;
    return 3;
}
