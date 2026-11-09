/* Core 3 for 0.6 (methods 8..23, 319 contexts). Clean-room rewrite.
 * Range coder: code/range init 5 bytes BE, range 0xFFFFFFFF,
 * renormalize while range < 0x1000000. Output bytes are the
 * complement of the decoded symbol (replicated exactly as the
 * original stores ~byte; the dictionary keeps raw bytes).
 */

#include <stdlib.h>
#include <string.h>
#include "UhaDec3.h"

#define AZ_M32 0xFFFFFFFFUL
#define AZ_NCTX 319
#define AZ_CTXMAX 318

typedef struct Az3
{
	Uha3ReadCb rcb;
	void* rctx;
	Uha3WriteCb wcb;
	void* wctx;
	unsigned char* inb;
	unsigned in_pos;
	unsigned in_avail;
	unsigned char* outb;
	unsigned out_fill;
	unsigned long produced;
	unsigned long target;
	int err;
	unsigned long code;
	unsigned long rng;
	unsigned long* htab;
	unsigned char* hval;
	unsigned char* mval;
	unsigned long* ord;
	unsigned char* dict;
	unsigned short* freq;
	unsigned short* raw;
	unsigned short* escw;
	unsigned short litm[50][4];
	unsigned long prob[5121];
	unsigned long psz;
	unsigned long pmask;
	unsigned long dpos;
	unsigned long ctx;
	long excl;
	long mpos;
	unsigned long run;
	unsigned long lastd;
	unsigned long symlo;
	unsigned long symhi;
	unsigned char state;
	unsigned long thresh;
	const unsigned long* cum;
	unsigned long ncum;
	unsigned long cumidx;
} Az3;

static const unsigned short az_litw1[50] = {
	81, 89, 97, 105, 105, 105, 105, 105,
	81, 89, 97, 105, 105, 105, 105, 105,
	81, 89, 97, 105, 105, 105, 105, 105,
	41, 49, 57, 65, 65, 65, 65, 65,
	41, 49, 57, 65, 65, 65, 65, 65,
	41, 49, 57, 65, 65, 65, 65, 65,
	385, 625
};

static const unsigned char az_st0[16] = {
	6, 0, 6, 0, 6, 1, 6, 2, 6, 3, 6, 3, 7, 4, 7, 5
};

static const unsigned char az_pair[512] = {
	0, 0, 0, 0, 0, 4, 0, 0, 3, 5, 0, 0, 2, 14, 0, 0,
	7, 9, 0, 0, 8, 12, 0, 0, 11, 13, 0, 0, 10, 30, 0, 0,
	15, 17, 0, 0, 16, 20, 0, 0, 19, 21, 0, 0, 18, 26, 0, 0,
	23, 25, 0, 0, 24, 28, 0, 0, 27, 29, 0, 0, 22, 62, 0, 0,
	31, 33, 0, 0, 32, 36, 0, 0, 35, 37, 0, 0, 34, 42, 0, 0,
	39, 41, 0, 0, 40, 44, 0, 0, 43, 45, 0, 0, 38, 54, 0, 0,
	47, 49, 0, 0, 48, 52, 0, 0, 51, 53, 0, 0, 50, 58, 0, 0,
	55, 57, 0, 0, 56, 60, 0, 0, 59, 61, 0, 0, 46, 254, 0, 0,
	63, 65, 0, 0, 64, 68, 0, 0, 67, 69, 0, 0, 66, 74, 0, 0,
	71, 73, 0, 0, 72, 76, 0, 0, 75, 77, 0, 0, 70, 86, 0, 0,
	79, 81, 0, 0, 80, 84, 0, 0, 83, 85, 0, 0, 82, 90, 0, 0,
	87, 89, 0, 0, 88, 92, 0, 0, 91, 93, 0, 0, 78, 110, 0, 0,
	95, 97, 0, 0, 96, 100, 0, 0, 99, 101, 0, 0, 98, 106, 0, 0,
	103, 105, 0, 0, 104, 108, 0, 0, 107, 109, 0, 0, 102, 118, 0, 0,
	111, 113, 0, 0, 112, 116, 0, 0, 115, 117, 0, 0, 114, 122, 0, 0,
	119, 121, 0, 0, 120, 124, 0, 0, 123, 125, 0, 0, 94, 190, 0, 0,
	127, 129, 0, 0, 128, 132, 0, 0, 131, 133, 0, 0, 130, 138, 0, 0,
	135, 137, 0, 0, 136, 140, 0, 0, 139, 141, 0, 0, 134, 150, 0, 0,
	143, 145, 0, 0, 144, 148, 0, 0, 147, 149, 0, 0, 146, 154, 0, 0,
	151, 153, 0, 0, 152, 156, 0, 0, 155, 157, 0, 0, 142, 174, 0, 0,
	159, 161, 0, 0, 160, 164, 0, 0, 163, 165, 0, 0, 162, 170, 0, 0,
	167, 169, 0, 0, 168, 172, 0, 0, 171, 173, 0, 0, 166, 182, 0, 0,
	175, 177, 0, 0, 176, 180, 0, 0, 179, 181, 0, 0, 178, 186, 0, 0,
	183, 185, 0, 0, 184, 188, 0, 0, 187, 189, 0, 0, 158, 222, 0, 0,
	191, 193, 0, 0, 192, 196, 0, 0, 195, 197, 0, 0, 194, 202, 0, 0,
	199, 201, 0, 0, 200, 204, 0, 0, 203, 205, 0, 0, 198, 214, 0, 0,
	207, 209, 0, 0, 208, 212, 0, 0, 211, 213, 0, 0, 210, 218, 0, 0,
	215, 217, 0, 0, 216, 220, 0, 0, 219, 221, 0, 0, 206, 238, 0, 0,
	223, 225, 0, 0, 224, 228, 0, 0, 227, 229, 0, 0, 226, 234, 0, 0,
	231, 233, 0, 0, 232, 236, 0, 0, 235, 237, 0, 0, 230, 246, 0, 0,
	239, 241, 0, 0, 240, 244, 0, 0, 243, 245, 0, 0, 242, 250, 0, 0,
	247, 249, 0, 0, 248, 252, 0, 0, 251, 253, 0, 0, 126, 0, 0, 0
};

static const unsigned char az_grp[12] = {0, 0, 1, 2, 3, 4, 5, 5, 6, 6, 6, 6};

static int az_ord(unsigned c)
{
	if((c >= 133 && c <= 158) || (c >= 165 && c <= 190)) return 0;
	return 1;
}

int UhaDec3_06Init(void** pctx, int method)
{
	Az3* d;
	unsigned long psz;
	unsigned k, j;
	if(!pctx || method < 8 || method > 23) return 1;
	psz = 1UL << (((method - 8) % 16) + 10);
	d = (Az3*)malloc(sizeof(*d));
	if(!d) return 2;
	memset(d, 0, sizeof(*d));
	d->htab = (unsigned long*)malloc(0x200000UL);
	d->hval = (unsigned char*)malloc(0x40000UL);
	d->mval = (unsigned char*)malloc(0x10000UL);
	d->ord = (unsigned long*)malloc(0x40000UL);
	d->dict = (unsigned char*)calloc(psz + 256UL, 1);
	d->freq = (unsigned short*)malloc(163328UL);
	d->raw = (unsigned short*)malloc(163328UL);
	d->escw = (unsigned short*)malloc(636UL);
	if(!d->htab || !d->hval || !d->mval || !d->ord ||
	   !d->dict || !d->freq || !d->raw || !d->escw) {
		free(d->htab); free(d->hval); free(d->mval); free(d->ord);
		free(d->dict); free(d->freq); free(d->raw); free(d->escw);
		free(d);
		return 2;
	}
	memset(d->htab, 0xFF, 0x200000UL);
	memset(d->hval, 0, 0x40000UL);
	memset(d->mval, 0, 0x10000UL);
	memset(d->ord, 0xFF, 0x40000UL);
	for(k = 0; k < 50; k++) {
		d->litm[k][0] = 0;
		d->litm[k][1] = az_litw1[k];
		d->litm[k][2] = 777;
		d->litm[k][3] = 0;
	}
	d->prob[0] = 0;
	for(k = 0; k < 10; k++)
		for(j = 1; j <= 512; j++) d->prob[k * 512 + j] = 2560UL - 64UL * k;
	d->psz = psz;
	d->pmask = psz - 1UL;
	d->dpos = 0;
	d->ctx = 0;
	d->excl = -1;
	d->mpos = -1;
	d->run = 0;
	d->lastd = AZ_M32;
	d->symlo = 0;
	d->symhi = 0;
	d->state = 0;
	d->thresh = 0;
	*pctx = d;
	return 0;
}

void UhaDec3_06Free(void* ctx)
{
	Az3* d = (Az3*)ctx;
	if(!d) return;
	free(d->htab); free(d->hval); free(d->mval); free(d->ord);
	free(d->dict); free(d->freq); free(d->raw); free(d->escw);
	free(d);
}

static int az_get(Az3* d)
{
	int n;
	if(d->err) return 0;
	if(d->in_pos >= d->in_avail) {
		n = d->rcb(d->rctx, d->inb, 65536);
		if(n <= 0) { d->err = 3; return 0; }
		d->in_avail = (unsigned)n;
		d->in_pos = 0;
	}
	return d->inb[d->in_pos++];
}

static void az_emit(Az3* d, unsigned b)
{
	d->outb[d->out_fill++] = (unsigned char)b;
	d->produced++;
	if(d->out_fill == 65536) {
		if(d->wcb(d->wctx, d->outb, d->out_fill) != 0) d->err = 4;
		d->out_fill = 0;
	}
}

static void az_rc_init(Az3* d)
{
	int i;
	d->code = 0;
	d->rng = AZ_M32;
	for(i = 0; i < 5; i++)
		d->code = ((d->code << 8) | (unsigned long)az_get(d)) & AZ_M32;
}

static int az_renorm(Az3* d, unsigned long lo, unsigned long hi)
{
	unsigned long rng, code;
	if(hi <= lo) { d->err = 7; return 0; }
	rng = (d->rng * ((hi - lo) & AZ_M32)) & AZ_M32;
	code = (d->code - ((d->rng * lo) & AZ_M32)) & AZ_M32;
	if(rng == 0) { d->err = 7; return 0; }
	while(rng < 0x1000000UL) {
		code = ((code << 8) | (unsigned long)az_get(d)) & AZ_M32;
		rng = (rng << 8) & AZ_M32;
		if(d->err) return 0;
	}
	d->code = code;
	d->rng = rng;
	return 1;
}

static void az_model_reset(Az3* d)
{
	unsigned i;
	memset(d->freq, 0, 162816UL);
	memset(d->raw, 0, 162816UL);
	for(i = 0; i < 256; i++) {
		unsigned v = (i == 0) ? 256u : (i & (0u - i));
		d->freq[318u * 256u + i] = (unsigned short)(v * 8u);
	}
	for(i = 0; i < 256; i++) d->raw[318u * 256u + i] = 8;
	for(i = 0; i < 318; i++) d->escw[i] = 1;
	for(i = 0; i < 50; i++) {
		d->litm[i][1] = (unsigned short)((d->litm[i][1] + 7) >> 3);
		d->litm[i][2] = (unsigned short)((d->litm[i][2] + 15) >> 3);
	}
	if(d->cum && d->ncum) {
		unsigned long t = (d->thresh + 0x10000UL) & AZ_M32;
		unsigned long idx, v;
		if(t > d->target) t = d->target;
		idx = d->cumidx;
		for(;;) {
			if(idx >= d->ncum - 1UL) { idx = d->ncum - 1UL; break; }
			v = d->cum[idx];
			if(v >= t) break;
			idx++;
		}
		d->cumidx = idx;
		d->thresh = d->cum[idx];
		if(d->cumidx && d->thresh < d->target) {
			unsigned long cur = d->cum[idx] - d->cum[idx - 1];
			unsigned long nxt = d->cum[idx + 1] - d->cum[idx];
			if(cur < 0x4000UL) {
				while(nxt < cur) {
					idx++;
					d->cumidx = idx;
					d->thresh = d->cum[idx];
					if(d->thresh >= d->target) break;
					cur = nxt;
					nxt = d->cum[idx + 1] - d->cum[idx];
				}
			}
		}
	} else {
		d->thresh = d->target;
	}
}

static void az_rescale(Az3* d, unsigned cidx)
{
	unsigned long total = 0;
	unsigned i, k, j;
	unsigned short* raw = d->raw + cidx * 256u;
	unsigned short* fr = d->freq + cidx * 256u;
	if(cidx < 0x13E) d->escw[cidx] = (unsigned short)((d->escw[cidx] >> 2) + 1);
	for(i = 0; i < 256; i += 8) {
		unsigned long d0 = (unsigned long)raw[i] | ((unsigned long)raw[i+1] << 16);
		unsigned long d1 = (unsigned long)raw[i+2] | ((unsigned long)raw[i+3] << 16);
		unsigned long d2 = (unsigned long)raw[i+4] | ((unsigned long)raw[i+5] << 16);
		unsigned long d3 = (unsigned long)raw[i+6] | ((unsigned long)raw[i+7] << 16);
		unsigned long h0 = ((d0 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h1 = ((d1 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h2 = ((d2 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h3 = ((d3 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		raw[i] = (unsigned short)(h0 & 0xFFFFUL);
		raw[i+1] = (unsigned short)((h0 >> 16) & 0xFFFFUL);
		raw[i+2] = (unsigned short)(h1 & 0xFFFFUL);
		raw[i+3] = (unsigned short)((h1 >> 16) & 0xFFFFUL);
		raw[i+4] = (unsigned short)(h2 & 0xFFFFUL);
		raw[i+5] = (unsigned short)((h2 >> 16) & 0xFFFFUL);
		raw[i+6] = (unsigned short)(h3 & 0xFFFFUL);
		raw[i+7] = (unsigned short)((h3 >> 16) & 0xFFFFUL);
		total = (total + h0 + h1) & AZ_M32;
		total = (total + h2 + h3) & AZ_M32;
	}
	for(i = 0; i < 255; i++) fr[i+1] = raw[i];
	fr[0] = (unsigned short)((total + (total >> 16)) & 0xFFFFUL);
	for(k = 1; k <= 64; k *= 2) {
		for(j = 256 - 2 * k; ; j -= 2 * k) {
			fr[j] = (unsigned short)(fr[j] + fr[j-k]);
			if(j == 2 * k) break;
		}
	}
}

static void az_update(Az3* d, unsigned cidx, unsigned sym)
{
	unsigned short* r = d->raw + cidx * 256u;
	unsigned short* f = d->freq + cidx * 256u;
	unsigned idx;
	unsigned t;
	if(r[sym] == 0) {
		unsigned short* r318 = d->raw + 318u * 256u;
		unsigned short* f318 = d->freq + 318u * 256u;
		d->escw[cidx] = (unsigned short)(d->escw[cidx] + 32);
		r318[sym] = (unsigned short)(r318[sym] + 16);
		idx = (sym + 1) & 0xFFu;
		while(idx != 0) {
			f318[idx] = (unsigned short)(f318[idx] + 16);
			idx = (idx + (idx & (0u - idx))) & 0xFFu;
		}
		f318[0] = (unsigned short)(f318[0] + 16);
		if(f318[0] >= 0x4000) az_rescale(d, 0x13E);
	}
	r[sym] = (unsigned short)(r[sym] + 32);
	idx = (sym + 1) & 0xFFu;
	while(idx != 0) {
		f[idx] = (unsigned short)(f[idx] + 32);
		idx = (idx + (idx & (0u - idx))) & 0xFFu;
	}
	f[0] = (unsigned short)(f[0] + 32);
	t = d->escw[cidx] + f[0];
	if(t >= 0x4000) az_rescale(d, cidx);
}

static unsigned az_dec_sym(Az3* d, unsigned cidx)
{
	unsigned short* f = d->freq + cidx * 256u;
	unsigned short* r = d->raw + cidx * 256u;
	unsigned long total, scaled, esc_total, v42;
	int have_eb, eb_on;
	unsigned eb, p, rem, step, sym, q;
	unsigned short *r318, *f318, xsum, acc;
	unsigned i;
	unsigned long v17, s2;
	unsigned v40, target;
	int is_fb;
	total = f[0];
	have_eb = (d->excl >= 0);
	eb = have_eb ? (unsigned)(unsigned char)d->dict[d->excl] : 0;
	if(have_eb) total = (total - r[eb]) & 0xFFFFUL;
	is_fb = 0;
	if(total == 0) {
		cidx = 0x13E;
		f = d->freq + cidx * 256u;
		r = d->raw + cidx * 256u;
		total = f[0];
		if(have_eb) total = (total - r[eb]) & 0xFFFFUL;
		is_fb = 1;
	}
	if(total == 0) { d->err = 7; return 0; }
	if(is_fb) d->rng /= total;
	else {
		esc_total = total + (unsigned long)d->escw[cidx];
		d->rng /= esc_total;
	}
	if(d->rng == 0) { d->err = 7; return 0; }
	scaled = d->code / d->rng;
	if((scaled & 0xFFFFUL) < total) {
		p = 0;
		rem = (unsigned)(total - scaled);
		if(have_eb) {
			v42 = r[eb];
			eb_on = 1;
			for(step = 128; step != 0; step >>= 1) {
				q = p + step;
				if(!eb_on || q <= eb) {
					if(rem > f[q]) { p = q; rem -= f[q]; }
				} else if(f[q] < v42 + rem) {
					p = q;
					eb_on = 0;
					rem = (rem - f[q] + (unsigned)v42) & 0xFFFFU;
				}
			}
		} else {
			for(step = 128; step != 0; step >>= 1) {
				q = p + step;
				if(rem > f[q]) { p = q; rem -= f[q]; }
			}
		}
		sym = p;
		if(sym > 255) { d->err = 7; return 0; }
		d->symhi = (scaled + rem) & 0xFFFFUL;
		if(r[sym] > d->symhi) { d->err = 7; return 0; }
		d->symlo = d->symhi - r[sym];
		if(!az_renorm(d, d->symlo, d->symhi)) return 0;
		return sym;
	} else {
		r318 = d->raw + 318u * 256u;
		f318 = d->freq + 318u * 256u;
		xsum = 0; acc = 0; v40 = 256;
		d->symlo = total;
		d->symhi = (unsigned long)d->escw[cidx] + total;
		if(!az_renorm(d, d->symlo, d->symhi)) return 0;
		for(i = 0; i < 256; i++)
			if(r[i] != 0) xsum = (unsigned short)(xsum + r318[i]);
		if(have_eb && r[eb] == 0) xsum = (unsigned short)(xsum + r318[eb]);
		v17 = (f318[0] - xsum) & AZ_M32;
		if(v17 == 0) { d->err = 7; return 0; }
		d->rng /= v17;
		if(d->rng == 0) { d->err = 7; return 0; }
		s2 = d->code / d->rng;
		if(have_eb) {
			target = eb + 1;
			while(v40 != target) {
				--v40;
				if(r[v40] == 0) {
					acc = (unsigned short)(acc + r318[v40]);
					if(acc > (unsigned short)(s2 & 0xFFFFUL)) break;
				}
			}
			if(acc <= (unsigned short)(s2 & 0xFFFFUL)) {
				if(v40 == 0) { d->err = 7; return 0; }
				--v40;
			}
		}
		if(acc <= (unsigned short)(s2 & 0xFFFFUL)) {
			for(;;) {
				if(v40 == 0) { d->err = 7; return 0; }
				--v40;
				if(r[v40] == 0) break;
			}
			for(;;) {
				acc = (unsigned short)(acc + r318[v40]);
				if(acc > (unsigned short)(s2 & 0xFFFFUL)) break;
				for(;;) {
					if(v40 == 0) { d->err = 7; return 0; }
					--v40;
					if(r[v40] == 0) break;
				}
			}
		}
		if(v40 > 255) { d->err = 7; return 0; }
		d->symhi = acc;
		d->symlo = (acc - r318[v40]) & 0xFFFFUL;
		sym = v40;
		if(!az_renorm(d, d->symlo, d->symhi)) return 0;
		return sym;
	}
}

static int az_match(Az3* d)
{
	unsigned long h, mpos;
	unsigned long* he;
	unsigned v1 = 0, v45 = 0;
	unsigned char* mp;
	if(d->err) return 0;
	d->mpos = -1;
	h = ((d->ctx ^ (d->ctx >> 13)) & 0x3FFFFUL);
	he = d->htab + h * 2;
	mpos = he[0];
	if(d->run) {
		mpos = (d->dpos - d->lastd) & d->pmask;
		v1 = (d->run > 2) + 48;
	}
	if(v1 == 0) {
		if(mpos == AZ_M32 || he[1] != d->ctx) {
			unsigned long dist;
			v45 = 0;
			mpos = (d->dpos - d->lastd) & d->pmask;
			dist = (mpos - d->dpos) & d->pmask;
			if(dist < 0x104UL) {
				unsigned long o = d->ord[d->ctx & 0xFFFFUL];
				if(o != AZ_M32) { mpos = o; v1 = 17; }
			} else {
				unsigned long mp4 = (mpos - 4) & d->pmask;
				unsigned long a0 = ((unsigned long)d->dict[mp4] << 24)
					| ((unsigned long)d->dict[mp4+1] << 16)
					| ((unsigned long)d->dict[mp4+2] << 8)
					| (unsigned long)d->dict[mp4+3];
				unsigned long c = d->ctx;
				if((unsigned short)a0 != (unsigned short)c
				   && (a0 & 0xFFFF00FFUL) != (c & 0xFFFF00FFUL)
				   && (a0 & 0xFFFFFF00UL) != (c & 0xFFFFFF00UL)) {
					unsigned long o = d->ord[d->ctx & 0xFFFFUL];
					if(o != AZ_M32) { mpos = o; v1 = 17; }
				} else {
					v1 = 9;
				}
			}
		} else {
			v45 = 1;
			v1 = 1;
		}
	}
	he[0] = d->dpos;
	d->ord[d->ctx & 0xFFFFUL] = d->dpos;
	he[1] = d->ctx;
	if(v1 == 0 || ((mpos - d->dpos) & d->pmask) < 0x100UL) {
		d->run = 0;
		return 0;
	}
	if(!d->run) {
		if(d->hval[h]) v1 += d->state - 1;
		else v1 += d->state + 23;
	}
	mp = d->dict + (mpos & d->pmask);
	if(d->excl >= 0 && d->dict[d->excl] == *mp) {
		unsigned short* e = d->litm[v1];
		e[2] = (unsigned short)(e[2] + 64);
		if(e[2] >= 0x4000) {
			e[1] = (unsigned short)((e[1] + 1) >> 1);
			e[2] = (unsigned short)((e[2] + 2) >> 1);
		}
		d->state = az_st0[2 * d->state + 1];
		d->run = 0;
		return 0;
	}
	{
		unsigned v42, nxt, bit;
		unsigned short* e;
		unsigned long scaled, lo, hi;
		v42 = ((unsigned)d->dict[mpos & d->pmask]) << 8;
		nxt = (mpos + 1) & d->pmask;
		if(nxt != d->dpos) v42 |= d->dict[nxt];
		d->mpos = (long)(mpos & d->pmask);
		e = d->litm[v1];
		if(e[2] == 0) { d->err = 7; return 0; }
		d->rng /= e[2];
		if(d->rng == 0) { d->err = 7; return 0; }
		scaled = d->code / d->rng;
		bit = ((scaled & 0xFFFFUL) >= e[1]) ? 1 : 0;
		if(bit == 0) {
			lo = e[0]; hi = e[1];
			e[1] = (unsigned short)(e[1] + 64);
		} else {
			lo = e[1]; hi = e[2];
		}
		e[2] = (unsigned short)(e[2] + 64);
		if(e[2] >= 0x4000) {
			e[1] = (unsigned short)((e[1] + 1) >> 1);
			e[2] = (unsigned short)((e[2] + 2) >> 1);
		}
		d->state = az_st0[2 * d->state + bit];
		if(!az_renorm(d, lo, hi)) return 0;
		if(bit) {
			d->hval[h] = 0;
			d->mval[v42] = 0;
			d->run = 0;
			return 0;
		}
		{
			unsigned v46 = 6, v51 = 0xFF, v23;
			unsigned v22 = d->mval[v42];
			unsigned long* cell, pv;
			unsigned n;
			unsigned long mcx, mdp, mmp;
			if(d->run) v23 = 8 + (d->run > 2 ? 1 : 0);
			else v23 = (v45 ? d->hval[h] : d->mval[v42]) < 12
				? az_grp[v45 ? d->hval[h] : d->mval[v42]] : 7;
			for(;;) {
				cell = d->prob + 1 + 512u * v23 + ((v22 <= v46) ? 256u : 0u) + v46;
				pv = *cell;
				d->rng >>= 12;
				if(d->rng == 0) { d->err = 7; return 0; }
				scaled = d->code / d->rng;
				if((scaled & 0xFFFFUL) >= pv) {
					d->symlo = pv; d->symhi = 4096;
					*cell -= (pv >> 6);
				} else {
					v51 = v46;
					d->symhi = pv; d->symlo = 0;
					*cell += ((4096UL - pv) >> 6);
				}
				if(!az_renorm(d, d->symlo, d->symhi)) return 0;
				v46 = az_pair[2 * v46 + (v51 > v46 ? 1 : 0)];
				if(v46 == 0) break;
			}
			d->hval[h] = (unsigned char)v51;
			d->mval[v42] = (unsigned char)v51;
			d->lastd = (d->dpos - (mpos & d->pmask)) & d->pmask;
			if(v51 == 0xFF) {
				d->excl = -1;
				d->run++;
			} else {
				d->run = 0;
				d->excl = (long)((mpos + v51) & d->pmask);
			}
			n = v51 == 0 ? 256 : v51;
			mcx = d->ctx;
			mdp = d->dpos;
			mmp = mpos & d->pmask;
			while(n--) {
				unsigned b = d->dict[mmp];
				d->dict[mdp] = (unsigned char)b;
				if(mdp < 0x100) d->dict[d->psz + mdp] = (unsigned char)b;
				mcx = ((mcx << 8) + b) & AZ_M32;
				az_emit(d, b ^ 0xFFu);
				mdp = (mdp + 1) & d->pmask;
				mmp = (mmp + 1) & d->pmask;
			}
			d->ctx = mcx;
			d->dpos = mdp;
			return 1;
		}
	}
}

static void az_literal(Az3* d)
{
	unsigned long c16 = d->ctx & 0xFFFFUL;
	unsigned lo = (unsigned)(c16 & 0xFFUL);
	unsigned hi = (unsigned)((c16 >> 8) & 0xFFUL);
	unsigned midx, b;
	if(c16 == 0xFFFFUL) midx = 314;
	else if(c16 == 0xD1DFUL) midx = 315;
	else if(c16 == 0xD3DFUL) midx = 316;
	else if(lo == 0xDF) midx = az_ord(hi) ? 317 : 223;
	else if(!az_ord(lo) && az_ord(hi)) midx = lo + 123;
	else midx = lo;
	b = az_dec_sym(d, midx);
	if(d->err) return;
	az_update(d, midx, b);
	d->excl = -1;
	if(d->mpos >= 0 && d->dict[d->mpos] == (unsigned char)b) {
		d->excl = (long)(((unsigned long)d->mpos + 1UL) & d->pmask);
	}
	az_emit(d, b ^ 0xFFu);
	d->ctx = ((d->ctx << 8) + b) & AZ_M32;
	d->dict[d->dpos] = (unsigned char)b;
	if(d->dpos < 0x100) d->dict[d->psz + d->dpos] = (unsigned char)b;
	d->dpos = (d->dpos + 1) & d->pmask;
}

int UhaDec3_06(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha3ReadCb rcb, void* rctx, Uha3WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	Az3* d = (Az3*)ctx;
	int err;
	if(!d || !rcb || !wcb) return 1;
	if(out_len == 0) return 0;
	d->inb = (unsigned char*)malloc(65536UL);
	d->outb = (unsigned char*)malloc(65536UL);
	if(!d->inb || !d->outb) {
		free(d->inb); free(d->outb);
		d->inb = 0; d->outb = 0;
		return 2;
	}
	d->rcb = rcb; d->rctx = rctx;
	d->wcb = wcb; d->wctx = wctx;
	d->in_pos = 0; d->in_avail = 0;
	d->out_fill = 0; d->produced = 0;
	d->target = out_len;
	d->err = 0;
	d->thresh = 0;
	d->cumidx = 0;
	if(cum && ncum && cum[ncum - 1] == out_len) {
		d->cum = cum;
		d->ncum = ncum;
	} else {
		d->cum = 0;
		d->ncum = 0;
	}
	az_rc_init(d);
	while(d->produced < d->target && !d->err) {
		if(d->produced >= d->thresh) az_model_reset(d);
		if(d->err) break;
		if(!az_match(d)) {
			if(d->err) break;
			az_literal(d);
		}
	}
	err = d->err;
	if(!err && d->out_fill) {
		if(wcb(wctx, d->outb, d->out_fill) != 0) err = 4;
	}
	free(d->inb);
	free(d->outb);
	d->inb = 0; d->outb = 0;
	return err;
}


/* ------------------------------------------------------------------ */
/* 0.2 core 3 (methods 8..43, PPM/LZ + range coder). Clean-room.
 * Group = 8..19 / 20..31 / 32..43; dict 1 << (((m-8) % 12) + 10).
 * 16-bit init range coder, token (control/run/match/literal) model.
 * Verified against 02_M1 (9), 02_M2 (21), 02_M3 (33) and uha02.UHA.
 */

static const unsigned short w02_ctl[13] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
};

static const unsigned short w02_state0[5] = {0, 1, 2, 0x0C00, 0x2000};

static const unsigned short w02_len0[23] = {
	0, 0x10, 0x18, 0x28, 0x38, 0x58, 0x7C, 0xCC, 0x130, 0x18C, 0x244, 0x2EF,
	0x401, 0x4C9, 0x5CD, 0x71F, 0x8E1, 0xB39, 0xE59, 0x1241, 0x17B9, 0x2119, 0x2710
};

static const unsigned short w02_dmod0[45] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x200, 0x400, 0x600,
	0x800, 0xA00, 0xC00, 0xE00, 0x1000
};

static const unsigned short w02_lcls0[4] = {0, 0x780, 0xC80, 0xF00};

static const unsigned short w02_small0[4] = {0x1000, 0, 0x1194, 0x238C};

static const unsigned short w02_d20[14] = {
	0, 0x190, 0x352, 0x546, 0x76C, 0x9C4, 0xC4E, 0xF0A, 0x11F8, 0x1518, 0x186A, 0x1BEE,
	0x1FA4, 0x238C
};

static const unsigned char w02_lbase[22] = {
	0xFF, 0xBF, 0x7F, 0x5F, 0x3F, 0x2F, 0x1F, 0x17, 0x13, 0x0F, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
	0x07, 0x06, 0x05, 0x04, 0x03, 0x02
};

static const unsigned char w02_lbits[22] = {
	0x00, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10, 0x08, 0x04, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const unsigned char w02_de8[16] = {
	0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0
};

static const unsigned char w02_df8[256] = {
	0x00, 0x01, 0x01, 0x02, 0x02, 0x02, 0x03, 0x03,
	0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04,
	0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
	0x06, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x09, 0x09, 0x09, 0x09, 0x0A, 0x0A, 0x0A, 0x0A,
	0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
	0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
	0x0A, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
	0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C,
	0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D,
	0x0D, 0x0D, 0x0D, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F
};

static const unsigned char w02_ef8[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x07, 0x08, 0x08, 0x09, 0x09, 0x0A, 0x0A, 0x0A,
	0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0C, 0x0C, 0x0C,
	0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D,
	0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D, 0x0D,
	0x0D, 0x0D, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
	0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E,
	0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0E, 0x0F,
	0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
	0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
	0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
	0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
	0x0F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
	0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12,
	0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x12, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,
	0x13, 0x13, 0x13, 0x13, 0x14, 0x14, 0x14, 0x14,
	0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x16, 0x16, 0x16, 0x17, 0x17, 0x18, 0x18,
	0x19, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

static const unsigned char w02_excl[16] = {
	0x00, 0x00, 0x01, 0x02, 0x02, 0x00, 0x00, 0x01, 0x04, 0x02, 0x00, 0x00, 0x06, 0x04, 0x02, 0x00
};

static const unsigned char w02_db[4] = {0x00, 0x05, 0x08, 0x10};

static const unsigned long w02_distbase[44] = {
	0x001E0000UL, 0x001C0000UL, 0x001A0000UL, 0x00180000UL, 0x00160000UL, 0x00140000UL,
	0x00120000UL, 0x00100000UL, 0x000E0000UL, 0x000C0000UL, 0x000A0000UL, 0x00080000UL,
	0x00060000UL, 0x00040000UL, 0x00030000UL, 0x00020000UL, 0x00018000UL, 0x00010000UL,
	0x0000C000UL, 0x00008000UL, 0x00006000UL, 0x00004000UL, 0x00003000UL, 0x00002000UL,
	0x00001800UL, 0x00001000UL, 0x00000C00UL, 0x00000800UL, 0x00000600UL, 0x00000400UL,
	0x00000300UL, 0x00000200UL, 0x00000100UL, 0x00000080UL, 0x00000040UL, 0x00000020UL,
	0x00000010UL, 0x00000008UL, 0x00000004UL, 0x00000002UL, 0x00000001UL, 0x00000003UL,
	0x00000001UL, 0x00000000UL
};

static const unsigned long w02_distspan[40] = {
	0x00020000UL, 0x00020000UL, 0x00020000UL, 0x00020000UL, 0x00020000UL, 0x00020000UL,
	0x00020000UL, 0x00020000UL, 0x00020000UL, 0x00020000UL, 0x00020000UL, 0x00020000UL,
	0x00020000UL, 0x00020000UL, 0x00010000UL, 0x00010000UL, 0x00008000UL, 0x00008000UL,
	0x00004000UL, 0x00004000UL, 0x00002000UL, 0x00002000UL, 0x00001000UL, 0x00001000UL,
	0x00000800UL, 0x00000800UL, 0x00000400UL, 0x00000400UL, 0x00000200UL, 0x00000200UL,
	0x00000100UL, 0x00000100UL, 0x00000100UL, 0x00000080UL, 0x00000040UL, 0x00000020UL,
	0x00000010UL, 0x00000008UL, 0x00000004UL, 0x00000002UL
};

static const unsigned long w02_fillthr[37] = {
	0xFFFFFFFFUL, 0x001E0000UL, 0x001C0000UL, 0x001A0000UL, 0x00180000UL, 0x00160000UL,
	0x00140000UL, 0x00120000UL, 0x00100000UL, 0x000E0000UL, 0x000C0000UL, 0x000A0000UL,
	0x00080000UL, 0x00060000UL, 0x00040000UL, 0x00030000UL, 0x00020000UL, 0x00018000UL,
	0x00010000UL, 0x0000C000UL, 0x00008000UL, 0x00006000UL, 0x00004000UL, 0x00003000UL,
	0x00002000UL, 0x00001800UL, 0x00001000UL, 0x00000C00UL, 0x00000800UL, 0x00000600UL,
	0x00000400UL, 0x00000300UL, 0x00000200UL, 0x00000100UL, 0x00000080UL, 0x00000040UL,
	0x00000020UL
};

typedef struct W02
{
	Uha3ReadCb rcb;
	void* rctx;
	Uha3WriteCb wcb;
	void* wctx;
	unsigned char inbuf[8192];
	unsigned in_pos;
	unsigned in_len;
	int in_eof;
	unsigned char cur_byte;
	unsigned bit_mask;
	unsigned char outbuf[65536];
	unsigned out_fill;
	unsigned long out_total;
	unsigned long out_expect;
	int err;
	unsigned long code, rng;
	unsigned long rtotal, rlow, rhigh;
	unsigned short* tree;
	unsigned short* freq;
	unsigned short state[8][5];
	unsigned short lenA[23];
	unsigned short lenB[23];
	unsigned short dmodel[45];
	unsigned short smallm[4];
	unsigned short d2[14];
	unsigned short lenclass[4];
	unsigned long* distlist;
	unsigned char* dict;
	unsigned long dsize;
	unsigned long dwrap;
	unsigned long dmask;
	unsigned long ring_pos;
	unsigned long ring_fill;
	unsigned long hash;
	unsigned long counter;
	unsigned long mpos;
	unsigned long list_max;
	unsigned long class_max;
	unsigned char group;
	unsigned char state_idx;
	unsigned char prev_ctx;
	unsigned char last_out;
	unsigned char last_sym;
	unsigned char mode;
	unsigned char db;
	unsigned char order;
	unsigned char prev_order;
	unsigned char flag7;
	unsigned char raw_lit;
	unsigned char exc_lit;
	unsigned char len_sel;
	unsigned char len_val;
	unsigned char fill_idx;
} W02;

static int w02_in_byte(W02* c)
{
	if(c->in_pos >= c->in_len) {
		int n;
		if(c->in_eof) { c->err = 3; return 0; }
		n = c->rcb(c->rctx, c->inbuf, (int)sizeof(c->inbuf));
		if(n <= 0) { c->in_eof = 1; c->err = 3; return 0; }
		c->in_len = (unsigned)n;
		c->in_pos = 0;
	}
	return c->inbuf[c->in_pos++];
}

static int w02_bit(W02* c)
{
	int b;
	if(c->bit_mask == 0) {
		c->cur_byte = (unsigned char)w02_in_byte(c);
		c->bit_mask = 0x80;
	}
	b = (c->cur_byte & c->bit_mask) ? 1 : 0;
	c->bit_mask >>= 1;
	return b;
}

static void w02_flush(W02* c)
{
	if(c->out_fill) {
		unsigned long n = c->out_fill;
		if(c->out_expect && c->out_total >= c->out_expect &&
		   c->out_total - n >= c->out_expect) {
			c->out_fill = 0;
			return;
		}
		if(c->out_expect && c->out_total - n < c->out_expect &&
		   c->out_total > c->out_expect) {
			n = c->out_expect - (c->out_total - n);
		}
		if(c->wcb(c->wctx, c->outbuf, n) != 0)
			c->err = 4;
		c->out_fill = 0;
	}
}

static void w02_out_byte(W02* c, unsigned char b)
{
	if(c->out_fill >= sizeof(c->outbuf)) w02_flush(c);
	c->outbuf[c->out_fill++] = b;
	c->out_total++;
}

static void w02_fill_out(W02* c, unsigned char b, unsigned long n)
{
	while(n) {
		unsigned room = (unsigned)(sizeof(c->outbuf) - c->out_fill);
		unsigned take;
		if(room == 0) { w02_flush(c); room = (unsigned)sizeof(c->outbuf); }
		take = (n < room) ? (unsigned)n : room;
		memset(c->outbuf + c->out_fill, b, take);
		c->out_fill += take;
		c->out_total += take;
		n -= take;
	}
}

static unsigned long w02_rget(W02* c, unsigned long total)
{
	unsigned long num;
	if(total == 0) { c->err = 7; return 0; }
	num = (((c->code + 1) * total) - 1) & 0xFFFFFFFFUL;
	if(c->rng == 0) { c->err = 7; return 0; }
	return (num / c->rng) & 0xFFFFFFFFUL;
}

static void w02_rupd(W02* c)
{
	unsigned long r = c->rng;
	unsigned long lo, hi, span;
	if(c->rtotal == 0 || r == 0) { c->err = 7; return; }
	lo = ((c->rlow * r) & 0xFFFFFFFFUL) / c->rtotal;
	c->code = (c->code - lo) & 0xFFFFFFFFUL;
	hi = ((c->rhigh * r) & 0xFFFFFFFFUL) / c->rtotal;
	span = (hi - lo) & 0xFFFFFFFFUL;
	while(span <= 0x8000UL) {
		int b = w02_bit(c);
		span = (span << 1) & 0xFFFFFFFFUL;
		c->code = ((c->code << 1) + (unsigned long)b) & 0xFFFFFFFFUL;
		if(c->err) return;
	}
	c->rng = span;
}

static unsigned w02_dec_tree(W02* c, const unsigned short* w)
{
	unsigned long v;
	unsigned edx, idx, step, mask, i;
	unsigned char cl = 0;
	unsigned short total = w[0];
	edx = total;
	v = w02_rget(c, total);
	if(v < (unsigned long)((unsigned short)(edx - w[128]))) {
		idx = 128; cl = 0x80; edx = (unsigned)(edx - w[128]);
	} else {
		idx = 0;
	}
	step = 64; mask = 0x40;
	for(i = 0; i < 7; i++) {
		unsigned di = (unsigned)((edx - w[idx + step]) & 0xFFFF);
		if(v < (unsigned long)di) {
			idx += step;
			cl = (unsigned char)(cl | mask);
			edx = di;
		}
		step >>= 1;
		mask >>= 1;
	}
	c->rhigh = edx & 0xFFFF;
	if(cl == 0xFF) {
		c->rlow = 0;
	} else if(!(cl & 1)) {
		c->rlow = (unsigned)((edx - w[idx + 1]) & 0xFFFF);
	} else {
		unsigned c2 = (unsigned)((cl + 1) & 0xFF);
		unsigned long p = 0;
		unsigned val = total;
		mask = 0x80;
		for(i = 0; i < 8; i++) {
			if(c2 & mask) {
				p += mask * 2UL;
				val = (unsigned)((val - w[p >> 1]) & 0xFFFF);
			}
			mask >>= 1;
		}
		c->rlow = val & 0xFFFF;
	}
	c->rtotal = total;
	return (unsigned)cl;
}

static unsigned w02_dec_tree2(W02* c, const unsigned short* w)
{
	unsigned long v;
	unsigned edx, idx, step, mask, i;
	unsigned char cl = 0x80;
	unsigned short total;
	edx = (unsigned)((w[0] - w[128]) & 0xFFFF);
	total = (unsigned short)edx;
	v = w02_rget(c, edx);
	idx = 128;
	{
		unsigned di = (unsigned)((edx - w[192]) & 0xFFFF);
		if(v < (unsigned long)di) {
			cl = 0xC0; idx += 64; edx = di;
		}
	}
	step = 32; mask = 0x20;
	for(i = 0; i < 6; i++) {
		unsigned di = (unsigned)((edx - w[idx + step]) & 0xFFFF);
		if(v < (unsigned long)di) {
			idx += step;
			cl = (unsigned char)(cl | mask);
			edx = di;
		}
		step >>= 1;
		mask >>= 1;
	}
	c->rhigh = edx & 0xFFFF;
	if(cl == 0xFF) {
		c->rlow = 0;
	} else if(!(cl & 1)) {
		c->rlow = (unsigned)((edx - w[idx + 1]) & 0xFFFF);
	} else {
		unsigned c2 = (unsigned)((cl + 1) & 0xFF);
		unsigned long p = 0;
		unsigned val = w[0];
		mask = 0x80;
		for(i = 0; i < 8; i++) {
			if(c2 & mask) {
				p += mask * 2UL;
				val = (unsigned)((val - w[p >> 1]) & 0xFFFF);
			}
			mask >>= 1;
		}
		c->rlow = val & 0xFFFF;
	}
	c->rtotal = total;
	return (unsigned)cl;
}

static void w02_halve_pairs(unsigned short* p, unsigned n)
{
	unsigned i;
	for(i = 0; i + 1 < n; i += 2) {
		unsigned long d = (unsigned long)p[i] | ((unsigned long)p[i+1] << 16);
		d = (d + 0x10001UL) & 0xFFFEFFFEUL;
		d >>= 1;
		p[i] = (unsigned short)(d & 0xFFFF);
		p[i+1] = (unsigned short)((d >> 16) & 0xFFFF);
	}
	if(n & 1) p[n-1] = (unsigned short)((p[n-1] + 1) >> 1);
}

static void w02_rebuild(W02* c, unsigned ctx)
{
	unsigned short* tr = c->tree + (unsigned long)ctx * 256;
	unsigned short* fr = c->freq + (unsigned long)ctx * 256;
	unsigned long total = 0;
	unsigned i;
	w02_halve_pairs(fr, 256);
	for(i = 0; i < 256; i++) total += fr[i];
	total &= 0xFFFF;
	tr[0] = (unsigned short)total;
	for(i = 1; i < 256; i++) tr[i] = fr[i-1];
	for(i = 1; i < 256; i++) {
		unsigned j = i + (i & (0u - i));
		if(j < 256) tr[j] = (unsigned short)(tr[j] + tr[i]);
	}
}

static void w02_halve(W02* c, unsigned ctx, unsigned shift)
{
	unsigned short* fr = c->freq + (unsigned long)ctx * 256;
	unsigned n;
	for(n = 1; n < shift; n++) w02_halve_pairs(fr, 256);
	w02_rebuild(c, ctx);
}

static void w02_update(W02* c, unsigned char sym)
{
	static const unsigned short inc_tab[4] = {2, 8, 32, 128};
	unsigned inc = inc_tab[c->order & 3];
	unsigned long base = (unsigned long)c->prev_ctx * 256;
	unsigned short* tr = c->tree + base;
	unsigned short* fr = c->freq + base;
	fr[sym] = (unsigned short)(fr[sym] + inc);
	if(sym != 0xFF) {
		unsigned i = (unsigned)(sym + 1) & 0xFF;
		unsigned bit = 1;
		while(i) {
			if(i & bit) {
				tr[i] = (unsigned short)(tr[i] + inc);
				i = (i + bit) & 0xFF;
			}
			bit = (bit << 1) & 0xFF;
			if(bit == 0) break;
		}
	}
	tr[0] = (unsigned short)(tr[0] + inc);
	if(tr[0] >= 0x4000) w02_rebuild(c, c->prev_ctx);
	switch(c->mode) {
	case 0:
	case 5:
		c->prev_ctx = sym;
		break;
	case 1:
		c->prev_ctx = (unsigned char)((w02_de8[c->prev_ctx & 15] << 4) + w02_df8[sym]);
		break;
	case 3:
		c->prev_ctx = (unsigned char)(w02_ef8[sym] + ((c->counter % 3) << 5));
		break;
	default:
		c->prev_ctx = (unsigned char)(((c->counter & c->db) << 4) + w02_df8[sym]);
		break;
	}
	c->counter++;
}

static unsigned char w02_hash_byte(W02* c)
{
	return (unsigned char)((c->hash >> (8 * (c->db & 3))) & 0xFF);
}

static void w02_put_orig(W02* c, unsigned char sym)
{
	unsigned char d = (unsigned char)(~sym);
	if(c->mode) {
		if(!c->flag7) d = (unsigned char)(0 - d);
		if((unsigned char)(~sym) >= 0x80) c->flag7 = (unsigned char)!c->flag7;
		d = (unsigned char)(d + w02_hash_byte(c));
	}
	c->hash = ((c->hash << 8) + d) & 0xFFFFFFFFUL;
	c->last_out = d;
	w02_out_byte(c, d);
}

static void w02_put_trans(W02* c, unsigned char b)
{
	unsigned char d;
	if(c->mode) {
		d = (unsigned char)(b - w02_hash_byte(c));
		if(!c->flag7) d = (unsigned char)(0 - d);
		if(d >= 0x80) c->flag7 = (unsigned char)!c->flag7;
	} else {
		d = b;
	}
	d = (unsigned char)(~d);
	c->hash = ((c->hash << 8) + b) & 0xFFFFFFFFUL;
	c->last_sym = d;
	w02_out_byte(c, b);
}

static void w02_upd_state(W02* c, unsigned v)
{
	unsigned short* m = c->state[c->state_idx];
	if(v == 2) m[3] = (unsigned short)(m[3] + 64);
	m[4] = (unsigned short)(m[4] + 64);
	if(m[4] >= 0x4000) {
		unsigned long f2 = (unsigned)(m[3] - m[2]);
		unsigned long f3 = (unsigned)(m[4] - m[3]);
		unsigned long h2 = (f2 + 1) >> 1;
		unsigned long h3 = (f3 + 1) >> 1;
		m[3] = (unsigned short)(m[2] + h2);
		m[4] = (unsigned short)(m[3] + h3);
	}
	c->state_idx = (unsigned char)((v + 2 * c->state_idx - 2) & 7);
}

static void w02_upd_lenclass(W02* c, unsigned v)
{
	unsigned short* m = c->lenclass;
	if(v == 0) m[1] = (unsigned short)(m[1] + 64);
	if(v <= 1) m[2] = (unsigned short)(m[2] + 64);
	m[3] = (unsigned short)(m[3] + 64);
	if(m[3] >= 0x4000) {
		unsigned long f0 = m[1] - m[0];
		unsigned long f1 = m[2] - m[1];
		unsigned long f2 = m[3] - m[2];
		unsigned long h0 = (f0 + 1) >> 1;
		unsigned long h1 = (f1 + 1) >> 1;
		unsigned long h2 = (f2 + 1) >> 1;
		m[1] = (unsigned short)(m[0] + h0);
		m[2] = (unsigned short)(m[1] + h1);
		m[3] = (unsigned short)(m[2] + h2);
	}
}

static void w02_rescale_cum(unsigned short* m, unsigned last)
{
	unsigned d;
	for(d = last; d >= 1; d--) {
		unsigned long diff = (unsigned)(m[d] - m[d-1]);
		m[d] = (unsigned short)((diff + 1) >> 1);
		if(d == 0) break;
	}
	{
		unsigned a;
		for(a = 2; a <= last; a++)
			m[a] = (unsigned short)(m[a] + m[a-1]);
	}
}

static void w02_upd_dmodel(W02* c, unsigned v)
{
	unsigned a;
	for(a = (unsigned)v + 1; a < 44; a++)
		c->dmodel[a] = (unsigned short)(c->dmodel[a] + 0x20);
	c->dmodel[44] = (unsigned short)(c->dmodel[44] + 0x20);
	if(c->dmodel[44] >= 0x4000) w02_rescale_cum(c->dmodel, 44);
}

static void w02_fill_step(W02* c, unsigned idx)
{
	unsigned inc = (unsigned)(c->dmodel[idx+1] + 1) >> 1;
	unsigned a;
	for(a = idx; a < 45; a++)
		c->dmodel[a] = (unsigned short)(c->dmodel[a] + inc);
	w02_rescale_cum(c->dmodel, 44);
}

static void w02_upd_lenmodel(W02* c, unsigned v)
{
	unsigned short* m = c->len_sel ? c->lenB : c->lenA;
	unsigned last = c->len_sel ? 22 : 21;
	unsigned a;
	for(a = (unsigned)v + 1; a < last; a++)
		m[a] = (unsigned short)(m[a] + 0x40);
	m[last] = (unsigned short)(m[last] + 0x40);
	if(m[last] >= 0x4000) w02_rescale_cum(m, last);
}

static void w02_upd_distlist(W02* c, unsigned v)
{
	unsigned char cl = (unsigned char)c->class_max;
	if(v < 3) {
		if(v == 2) c->smallm[2] = (unsigned short)(c->smallm[2] + 0x20);
		c->smallm[3] = (unsigned short)(c->smallm[3] + 0x20);
		if(c->smallm[3] >= 0x4000) {
			unsigned long h2 = ((unsigned)(c->smallm[2] - c->smallm[1]) + 1) >> 1;
			unsigned long h3 = ((unsigned)(c->smallm[3] - c->smallm[2]) + 1) >> 1;
			if(h2 < 0x400) h2 = 0x400;
			if(h3 < 0x400) h3 = 0x400;
			c->smallm[2] = (unsigned short)(c->smallm[1] + h2);
			c->smallm[3] = (unsigned short)(c->smallm[2] + h3);
		}
	} else {
		unsigned a = (unsigned)c->list_max - 1 - v;
		for(;;) {
			a++;
			if(a >= cl) break;
			c->d2[a] = (unsigned short)(c->d2[a] + 0x20);
		}
		c->d2[cl] = (unsigned short)(c->d2[cl] + 0x20);
		if(c->d2[cl] >= 0x4000) {
			unsigned d;
			for(d = cl; d >= 1; d--) {
				unsigned long diff = (unsigned)(c->d2[d] - c->d2[d-1]);
				unsigned long h = (diff + 1) >> 1;
				if(h < 0x100) h = 0x100;
				c->d2[d] = (unsigned short)h;
				if(d == 0) break;
			}
			{
				unsigned x;
				for(x = 2; x <= cl; x++)
					c->d2[x] = (unsigned short)(c->d2[x] + c->d2[x-1]);
			}
		}
	}
	c->class_max = cl;
}

static unsigned w02_dec_len(W02* c)
{
	unsigned short* m = c->len_sel ? c->lenB : c->lenA;
	unsigned start = c->len_sel ? 22 : 21;
	unsigned long v;
	unsigned d = start;
	unsigned bits, extra = 0;
	c->rtotal = m[start];
	v = w02_rget(c, c->rtotal);
	do {
		d--;
	} while(v < (unsigned long)m[d] && d);
	c->rlow = m[d];
	c->rhigh = m[d+1];
	w02_rupd(c);
	bits = w02_lbits[d];
	if(bits) {
		c->rtotal = bits;
		extra = (unsigned)w02_rget(c, bits);
		c->rlow = extra & 0xFF;
		c->rhigh = (extra + 1) & 0xFFFF;
		w02_rupd(c);
	}
	c->len_val = (unsigned char)(w02_lbase[d] + (extra & 0xFF));
	return d;
}

static unsigned w02_dec_dist(W02* c)
{
	unsigned long v;
	unsigned d, ebx, ex, low, high;
	unsigned short* m = c->dmodel;
	c->rtotal = m[44];
	v = w02_rget(c, c->rtotal);
	d = 44;
	do {
		d--;
	} while(v < (unsigned long)m[d] && d);
	c->rlow = m[d];
	c->rhigh = m[d+1];
	w02_rupd(c);
	c->mpos = w02_distbase[d];
	if(d == 40) {
		c->len_sel = 0;
		return d;
	}
	if(d == 41) {
		unsigned short* m2 = c->d2;
		ebx = c->class_max;
		ex = ebx * 2;
		c->rtotal = m2[ebx];
		v = w02_rget(c, c->rtotal);
		do {
			high = m2[ebx-1];
			ex -= 2;
			ebx--;
			if(v >= (unsigned long)high) break;
		} while(ebx);
		c->rlow = m2[ebx];
		c->rhigh = m2[ebx+1];
		w02_rupd(c);
		c->mpos += (unsigned long)(c->class_max - 1 - ebx);
		c->len_sel = 1;
		return d;
	}
	if(d == 42) {
		unsigned short* sm = c->smallm;
		ebx = 2;
		c->rtotal = sm[3];
		v = w02_rget(c, c->rtotal);
		do {
			high = sm[ebx];
			ebx--;
			if(v >= (unsigned long)high) break;
		} while(ebx);
		c->rlow = sm[ebx+1];
		c->rhigh = sm[ebx+2];
		w02_rupd(c);
		c->mpos += (unsigned long)(1 - ebx);
		c->len_sel = 1;
		return d;
	}
	if(d == 43) {
		c->len_sel = 1;
		return d;
	}
	ebx = (unsigned)w02_distspan[d];
	ex = (unsigned)(c->mpos + ebx - 1);
	if(ex > c->ring_fill) ebx = (unsigned)(c->ring_fill - c->mpos + 1);
	if(ebx >= 2) {
		if(ebx > 0x1000) {
			unsigned long q;
			c->rtotal = ((ebx - 1) >> 12) + 1;
			q = w02_rget(c, c->rtotal) & 0xFFFF;
			c->rlow = (unsigned)q;
			c->rhigh = (unsigned)(q + 1);
			w02_rupd(c);
			c->mpos = (c->mpos + (q << 12)) & 0xFFFFFFFFUL;
			if(c->rhigh == c->rtotal) {
				ebx -= (unsigned)(q << 12);
			} else {
				ebx = 0x1000;
			}
		}
		if(ebx >= 2) {
			c->rtotal = ebx;
			low = (unsigned)w02_rget(c, ebx) & 0xFFFF;
			c->rlow = low;
			c->rhigh = (unsigned)(low + 1);
			w02_rupd(c);
			c->mpos = (c->mpos + low) & 0xFFFFFFFFUL;
		}
	}
	c->len_sel = 0;
	return d;
}

static unsigned w02_token(W02* c)
{
	unsigned short* m = c->state[c->state_idx];
	unsigned long v;
	unsigned d = 4;
	c->rtotal = m[4];
	v = w02_rget(c, c->rtotal);
	do {
		d--;
		if(v >= (unsigned long)m[d]) break;
	} while(d);
	c->rlow = m[d];
	c->rhigh = m[d+1];
	w02_rupd(c);
	if(d <= 1) return d;
	w02_upd_state(c, d);
	return d;
}

static void w02_reset_models(W02* c)
{
	unsigned i;
	for(i = 0; i < 256; i++) {
		unsigned j;
		unsigned short* tr = c->tree + (unsigned long)i * 256;
		unsigned short* fr = c->freq + (unsigned long)i * 256;
		for(j = 0; j < 256; j++) fr[j] = 1;
		tr[0] = 256;
		for(j = 1; j < 256; j++) tr[j] = 1;
		for(j = 1; j < 256; j++) {
			unsigned k = j + (j & (0u - j));
			if(k < 256) tr[k] = (unsigned short)(tr[k] + tr[j]);
		}
	}
}

static void w02_control(W02* c)
{
	unsigned long v;
	unsigned d = 12;
	int v15 = 0, v14 = 0;
	c->rtotal = 12;
	v = w02_rget(c, 12);
	do {
		d--;
	} while(v < (unsigned long)w02_ctl[d] && d);
	c->rlow = w02_ctl[d];
	c->rhigh = w02_ctl[d+1];
	w02_rupd(c);
	if(d > 11) { c->err = 7; return; }
	if(d <= 5) v15 = 1;
	else if(d <= 9) v14 = 1;
	else if(d == 10) c->raw_lit = (unsigned char)!c->raw_lit;
	else if(d == 11) c->exc_lit = (unsigned char)!c->exc_lit;
	if(v15) {
		c->mode = (unsigned char)d;
		c->db = (unsigned char)((d - 1) & 3);
		if(c->out_total) w02_reset_models(c);
	}
	if(v14) {
		unsigned shift;
		c->prev_order = c->order;
		c->order = (unsigned char)(d - 6);
		if(c->out_total && !v15) {
			shift = w02_excl[4 * c->prev_order + c->order];
			if(shift) {
				unsigned i;
				for(i = 0; i < 256; i++)
					w02_halve(c, i, shift);
			}
		}
	}
}

static void w02_literal(W02* c)
{
	unsigned sym;
	if(c->raw_lit) {
		c->rtotal = 256;
		sym = (unsigned)w02_rget(c, 256) & 0xFF;
		c->rlow = sym;
		c->rhigh = (sym + 1) & 0xFFFF;
		w02_rupd(c);
	} else if(c->exc_lit) {
		sym = w02_dec_tree2(c, c->tree + (unsigned long)c->prev_ctx * 256);
		w02_rupd(c);
	} else {
		sym = w02_dec_tree(c, c->tree + (unsigned long)c->prev_ctx * 256);
		w02_rupd(c);
	}
	w02_update(c, (unsigned char)sym);
	w02_put_orig(c, (unsigned char)sym);
	c->dict[c->ring_pos] = c->last_out;
	if(c->ring_fill < c->dwrap) c->ring_fill++;
	c->ring_pos = (c->ring_pos + 1) & c->dmask;
}

static void w02_match(W02* c)
{
	unsigned dist_idx, sym, len_sym;
	unsigned long dist, src;
	unsigned bl;
	for(;;) {
		unsigned idx = c->fill_idx;
		if(c->ring_fill >= w02_fillthr[idx]) {
			c->fill_idx = (unsigned char)(idx - 1);
			w02_fill_step(c, idx);
		} else break;
	}
	sym = w02_dec_dist(c);
	w02_upd_dmodel(c, sym);
	if(c->len_sel) {
		if(c->len_sel != 1) { c->err = 7; return; }
		dist_idx = (unsigned)(c->mpos & 0xFF);
		if(dist_idx >= 16) { c->err = 7; return; }
		dist = c->distlist[dist_idx];
		if(dist_idx) w02_upd_distlist(c, dist_idx);
		bl = dist_idx;
	} else {
		dist = c->mpos;
		bl = (unsigned)c->list_max - 1;
	}
	if(bl && dist > 1) {
		unsigned k = bl;
		while(k) {
			c->distlist[k] = c->distlist[k-1];
			k--;
		}
		c->distlist[0] = dist;
	}
	src = (c->ring_pos - dist) & c->dmask;
	len_sym = w02_dec_len(c);
	w02_upd_lenmodel(c, len_sym);
	bl = c->len_val;
	if(bl < 2) { c->err = 7; return; }
	for(;;) {
		unsigned char b = c->dict[src];
		c->dict[c->ring_pos] = b;
		if(c->ring_fill < c->dwrap) c->ring_fill++;
		c->ring_pos = (c->ring_pos + 1) & c->dmask;
		src = (src + 1) & c->dmask;
		w02_put_trans(c, b);
		w02_update(c, c->last_sym);
		bl--;
		if(!bl) break;
	}
}

static void w02_run(W02* c)
{
	unsigned long v;
	unsigned d = 3;
	unsigned long len;
	unsigned char b;
	unsigned i;
	c->rtotal = c->lenclass[3];
	v = w02_rget(c, c->lenclass[3]);
	do {
		d--;
	} while(v < (unsigned long)c->lenclass[d] && d);
	c->rlow = c->lenclass[d];
	c->rhigh = c->lenclass[d+1];
	w02_rupd(c);
	w02_upd_lenclass(c, d);
	len = 0x100UL;
	if(d == 2) {
		c->rtotal = 256;
		v = w02_rget(c, 256) & 0xFF;
		c->rlow = (unsigned)v;
		c->rhigh = (unsigned)(v + 1);
		w02_rupd(c);
		len += (v & 0xFF) << 16;
	}
	if(d == 1 || d == 2) {
		c->rtotal = 256;
		v = w02_rget(c, 256) & 0xFF;
		c->rlow = (unsigned)v;
		c->rhigh = (unsigned)(v + 1);
		w02_rupd(c);
		len += (v & 0xFF) << 8;
	}
	c->rtotal = 256;
	v = w02_rget(c, 256) & 0xFF;
	c->rlow = (unsigned)v;
	c->rhigh = (unsigned)(v + 1);
	w02_rupd(c);
	len += v & 0xFF;
	c->rtotal = 256;
	v = w02_rget(c, 256) & 0xFF;
	c->rlow = (unsigned)v;
	c->rhigh = (unsigned)(v + 1);
	w02_rupd(c);
	b = (unsigned char)v;
	if(c->ring_pos + 255 < c->dsize) {
		memset(c->dict + c->ring_pos, b, 255);
		{
			unsigned long nf = c->ring_fill + 255;
			if(nf > c->dwrap) nf = c->dwrap;
			c->ring_fill = nf;
		}
		c->ring_pos += 255;
	} else {
		for(i = 0; i < 255; i++) {
			c->dict[c->ring_pos] = b;
			if(c->ring_fill < c->dwrap) c->ring_fill++;
			c->ring_pos = (c->ring_pos + 1) & c->dmask;
		}
	}
	for(i = 0; i < 5; i++) {
		w02_put_trans(c, b);
		w02_update(c, c->last_sym);
	}
	len -= 5;
	w02_fill_out(c, b, len);
	while(len) {
		w02_update(c, c->last_sym);
		len--;
	}
}

static void w02_free(W02* c)
{
	if(!c) return;
	free(c->tree);
	free(c->freq);
	free(c->distlist);
	free(c->dict);
	free(c);
}

int UhaDec3_02Init(void** pctx, int method)
{
	W02* c;
	unsigned group, i;
	unsigned long dict;
	if(!pctx) return 1;
	*pctx = 0;
	if(method < 8 || method > 43) return 1;
	group = (method < 20) ? 1u : (method < 32 ? 2u : 3u);
	c = (W02*)calloc(1, sizeof(*c));
	if(!c) return 2;
	c->tree = (unsigned short*)malloc(256UL * 256UL * sizeof(unsigned short));
	c->freq = (unsigned short*)malloc(256UL * 256UL * sizeof(unsigned short));
	c->distlist = (unsigned long*)malloc(16 * sizeof(unsigned long));
	dict = 1UL << (((unsigned)(method - 8) % 12u) + 10u);
	c->dict = (unsigned char*)malloc(dict + 256);
	if(!c->tree || !c->freq || !c->distlist || !c->dict) {
		w02_free(c);
		return 2;
	}
	c->dsize = dict;
	c->dwrap = dict - 255;
	c->dmask = dict - 1;
	c->group = (unsigned char)group;
	c->list_max = w02_db[group];
	c->class_max = (unsigned long)(w02_db[group] - 3);
	for(i = 0; i < 256; i++) {
		unsigned j;
		unsigned short* tr = c->tree + (unsigned long)i * 256;
		unsigned short* fr = c->freq + (unsigned long)i * 256;
		for(j = 0; j < 256; j++) fr[j] = 1;
		tr[0] = 256;
		for(j = 1; j < 256; j++) tr[j] = 1;
		for(j = 1; j < 256; j++) {
			unsigned k = j + (j & (0u - j));
			if(k < 256) tr[k] = (unsigned short)(tr[k] + tr[j]);
		}
	}
	for(i = 0; i < 8; i++) memcpy(c->state[i], w02_state0, 10);
	memcpy(c->lenA, w02_len0, sizeof(c->lenA));
	memcpy(c->lenB, w02_len0, sizeof(c->lenB));
	memcpy(c->dmodel, w02_dmod0, sizeof(c->dmodel));
	memcpy(c->smallm, w02_small0, sizeof(c->smallm));
	memcpy(c->d2, w02_d20, sizeof(c->d2));
	memcpy(c->lenclass, w02_lcls0, sizeof(c->lenclass));
	for(i = 0; i < 16; i++) c->distlist[i] = 0xFFFFFFFFUL;
	c->state_idx = 7;
	c->prev_ctx = 0xFF;
	c->last_out = 0;
	c->last_sym = 0;
	c->mode = 0;
	c->db = 0;
	c->order = 3;
	c->prev_order = 3;
	c->flag7 = 1;
	c->raw_lit = 0;
	c->exc_lit = 0;
	c->len_sel = 0;
	c->len_val = 0;
	c->fill_idx = (unsigned char)(sizeof(w02_fillthr) / sizeof(w02_fillthr[0]) - 1);
	c->hash = 0xFFFFFFFFUL;
	c->counter = 0;
	c->mpos = 0xFFFFFFFFUL;
	c->ring_pos = 0;
	c->ring_fill = 31;
	c->code = 0;
	c->rng = 0xFFFFUL;
	*pctx = c;
	return 0;
}

void UhaDec3_02Free(void* ctx)
{
	w02_free((W02*)ctx);
}

int UhaDec3_02(void* ctx, Uha3ReadCb rcb, void* rctx,
	Uha3WriteCb wcb, void* wctx, unsigned long out_len)
{
	W02* c = (W02*)ctx;
	unsigned i;
	if(!c || !rcb || !wcb) return 1;
	c->rcb = rcb; c->rctx = rctx;
	c->wcb = wcb; c->wctx = wctx;
	c->out_expect = out_len;
	c->out_total = 0;
	c->out_fill = 0;
	c->in_pos = c->in_len = 0;
	c->in_eof = 0;
	c->err = 0;
	c->bit_mask = 0;
	c->code = 0;
	for(i = 0; i < 16; i++)
		c->code = ((c->code << 1) | (unsigned long)w02_bit(c)) & 0xFFFFFFFFUL;
	c->rng = 0x10000UL;
	if(c->err) return c->err;
	while(c->out_total < c->out_expect && !c->err) {
		unsigned t = w02_token(c);
		switch(t) {
		case 0: w02_control(c); break;
		case 1: w02_run(c); break;
		case 2: w02_match(c); break;
		case 3: w02_literal(c); break;
		default: c->err = 7; break;
		}
	}
	w02_flush(c);
	if(c->err) return c->err;
	if(c->out_total < c->out_expect) return 4;
	return 0;
}

/* ------------------------------------------------------------------ */
/* 0.3 core 3 (methods 8..21). Clean-room rewrite.
 * A 24-bit range/offset coder with an 8-bit renormalisation step and
 * a one-byte bit reservoir; the stream starts with a discarded byte
 * (the ~out_len marker). 326 symbol contexts plus two order-0
 * contexts (326 for ordinary contexts, 327 for the length contexts
 * and second-level escapes). The escape from a symbol context
 * excludes all symbols already present in that context from the
 * order-0 model (multi-exclusion) and decodes that model from
 * symbol 255 downwards. The match/literal decision uses a 48-entry
 * table of {c0,c1,c2} entries indexed by (hist*8 + class), and the
 * match length is a symbol in contexts 318..325 chosen by the
 * previous length. No run-length/state stage beyond that.
 * Dictionary: 1 << (((method-8) % 14) + 10).
 */

#define V3_M32 0xFFFFFFFFUL
#define V3_M16 0xFFFFUL
#define V3_NCTX 328u
#define V3_NFB 326u
#define V3_NLITM 48u

typedef struct Zc3
{
	Uha3ReadCb rcb;
	void* rctx;
	Uha3WriteCb wcb;
	void* wctx;
	unsigned char* inb;
	unsigned in_pos;
	unsigned in_avail;
	unsigned char* outb;
	unsigned out_fill;
	unsigned long produced;
	unsigned long target;
	int err;
	unsigned long unit;
	unsigned long code;
	unsigned long rng;
	unsigned cur;
	unsigned char* htab;
	unsigned char* hval;
	unsigned long* ord;
	unsigned char* dict;
	unsigned short* freq;
	unsigned short* raw;
	unsigned short* escw;
	unsigned short litm[48][3];
	unsigned char hist;
	unsigned long dsz;
	unsigned long dmask;
	unsigned long dpos;
	unsigned long ctx;
	long excl;
	long mpos;
	unsigned long lastd;
	unsigned long thresh;
	const unsigned long* cum;
	unsigned long ncum;
	unsigned long cumidx;
} Zc3;

static const unsigned short v3_litw1[8] = {
	0x0800, 0x0400, 0x0500, 0x0400, 0x0600, 0x0400, 0x0500, 0x0400
};

static int v3_ord(unsigned c)
{
	if((c >= 133 && c <= 158) || (c >= 165 && c <= 190)) return 0;
	return 1;
}

static unsigned v3_lenctx(unsigned v)
{
	if(v < 2) return 0x13Eu;
	if(v < 6) return 0x13Fu + (v - 2);
	if(v < 8) return 0x143u;
	if(v < 12) return 0x144u;
	return 0x145u;
}

static void v3_reset(Zc3* d);

int UhaDec3_03Init(void** pctx, int method)
{
	Zc3* d;
	unsigned long dsz;
	if(!pctx) return 1;
	*pctx = 0;
	if(method < 8 || method > 21) return 1;
	dsz = 1UL << (((method - 8) % 14) + 10);
	d = (Zc3*)malloc(sizeof(*d));
	if(!d) return 2;
	memset(d, 0, sizeof(*d));
	d->htab = (unsigned char*)malloc(0x200000UL);
	d->hval = (unsigned char*)malloc(0x40000UL);
	d->ord = (unsigned long*)malloc(0x40000UL);
	d->dict = (unsigned char*)calloc((size_t)(dsz + 256UL), 1);
	d->freq = (unsigned short*)malloc(V3_NCTX * 256u * sizeof(unsigned short));
	d->raw = (unsigned short*)malloc(V3_NCTX * 256u * sizeof(unsigned short));
	d->escw = (unsigned short*)malloc(V3_NCTX * sizeof(unsigned short));
	if(!d->htab || !d->hval || !d->ord || !d->dict || !d->freq || !d->raw || !d->escw) {
		free(d->htab); free(d->hval); free(d->ord); free(d->dict);
		free(d->freq); free(d->raw); free(d->escw); free(d);
		return 2;
	}
	memset(d->htab, 0xFF, 0x200000UL);
	memset(d->hval, 0, 0x40000UL);
	memset(d->ord, 0xFF, 0x40000UL);
	d->dsz = dsz;
	d->dmask = dsz - 1UL;
	d->dpos = 0;
	d->ctx = 0;
	d->excl = -1;
	d->mpos = -1;
	d->lastd = V3_M32;
	d->hist = 7;
	v3_reset(d);
	*pctx = d;
	return 0;
}

void UhaDec3_03Free(void* ctx)
{
	Zc3* d = (Zc3*)ctx;
	if(!d) return;
	free(d->htab); free(d->hval); free(d->ord); free(d->dict);
	free(d->freq); free(d->raw); free(d->escw); free(d);
}

static int v3_get(Zc3* d)
{
	int n;
	if(d->err) return 0;
	if(d->in_pos >= d->in_avail) {
		n = d->rcb(d->rctx, d->inb, 65536);
		if(n <= 0) { d->err = 3; return 0; }
		d->in_avail = (unsigned)n;
		d->in_pos = 0;
	}
	return d->inb[d->in_pos++];
}

static void v3_emit(Zc3* d, unsigned b)
{
	if(d->produced < d->target) {
		d->outb[d->out_fill++] = (unsigned char)b;
		if(d->out_fill == 65536) {
			if(d->wcb(d->wctx, d->outb, d->out_fill) != 0) d->err = 4;
			d->out_fill = 0;
		}
	}
	d->produced++;
}

static void v3_rc_init(Zc3* d)
{
	int b;
	v3_get(d);	/* discarded ~out_len marker byte */
	b = v3_get(d);
	d->cur = (unsigned)b;
	d->rng = 0x80UL;
	d->code = (unsigned long)((unsigned)b >> 1);
}

static void v3_renorm(Zc3* d)
{
	while(d->rng <= 0x800000UL) {
		unsigned b = (unsigned)v3_get(d);
		d->code = ((d->code << 8) |
			(unsigned long)((d->cur & 1u) << 7) |
			(unsigned long)(b >> 1)) & V3_M32;
		d->rng = (d->rng << 8) & V3_M32;
		d->cur = b;
		if(d->err) return;
	}
}

static unsigned v3_point(Zc3* d, unsigned total)
{
	unsigned long unit;
	unsigned long p;
	if(d->err) return 0;
	if(d->rng <= 0x800000UL) v3_renorm(d);
	if(d->err) return 0;
	total &= V3_M16;
	if(total == 0) { d->err = 7; return 0; }
	unit = d->rng / total;
	if(unit == 0) { d->err = 7; return 0; }
	d->unit = unit;
	p = d->code / unit;
	if(p >= (unsigned long)total) p = (unsigned long)total - 1;
	return (unsigned)p;
}

static void v3_narrow(Zc3* d, unsigned total, unsigned lo, unsigned hi)
{
	unsigned long u = d->unit;
	unsigned long lp = (u * (unsigned long)lo) & V3_M32;
	d->code = (d->code - lp) & V3_M32;
	if((unsigned long)hi < (unsigned long)total)
		d->rng = (u * (unsigned long)(hi - lo)) & V3_M32;
	else
		d->rng = (d->rng - lp) & V3_M32;
}

static void v3_rescale(Zc3* d, unsigned cidx)
{
	unsigned short* raw = d->raw + cidx * 256u;
	unsigned short* fr = d->freq + cidx * 256u;
	unsigned long total = 0;
	unsigned i, k, j;
	for(i = 0; i < 256; i += 8) {
		unsigned long d0 = (unsigned long)raw[i] | ((unsigned long)raw[i+1] << 16);
		unsigned long d1 = (unsigned long)raw[i+2] | ((unsigned long)raw[i+3] << 16);
		unsigned long d2 = (unsigned long)raw[i+4] | ((unsigned long)raw[i+5] << 16);
		unsigned long d3 = (unsigned long)raw[i+6] | ((unsigned long)raw[i+7] << 16);
		unsigned long h0 = ((d0 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h1 = ((d1 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h2 = ((d2 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h3 = ((d3 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		raw[i]     = (unsigned short)(h0 & V3_M16);
		raw[i+1]   = (unsigned short)((h0 >> 16) & V3_M16);
		raw[i+2]   = (unsigned short)(h1 & V3_M16);
		raw[i+3]   = (unsigned short)((h1 >> 16) & V3_M16);
		raw[i+4]   = (unsigned short)(h2 & V3_M16);
		raw[i+5]   = (unsigned short)((h2 >> 16) & V3_M16);
		raw[i+6]   = (unsigned short)(h3 & V3_M16);
		raw[i+7]   = (unsigned short)((h3 >> 16) & V3_M16);
		total = (total + h0 + h1) & V3_M32;
		total = (total + h2 + h3) & V3_M32;
	}
	/* freq[i] = raw[i-1] (1-based Fenwick fill) */
	for(i = 1; i < 256; i++) fr[i] = raw[i-1];
	fr[0] = (unsigned short)(((total & V3_M16) + ((total >> 16) & V3_M16)) & V3_M16);
	for(k = 1; k <= 64; k *= 2) {
		j = 256 - 2 * k;
		for(;;) {
			fr[j] = (unsigned short)(fr[j] + fr[j-k]);
			if(j == 2 * k) break;
			j -= 2 * k;
		}
	}
	if(cidx < 0x146u) d->escw[cidx] = (unsigned short)((d->escw[cidx] >> 2) + 1);
}

static void v3_update(Zc3* d, unsigned cidx, unsigned sym)
{
	unsigned short* f = d->freq;
	unsigned short* r = d->raw;
	unsigned base = cidx * 256u;
	unsigned fb, i, t;
	if(r[base + sym] == 0) {
		fb = V3_NFB + ((cidx >= 0x13Eu) ? 1u : 0u);
		d->escw[cidx] = (unsigned short)(d->escw[cidx] + 32);
		r[fb * 256u + sym] = (unsigned short)(r[fb * 256u + sym] + 16);
		i = (sym + 1) & 0xFFu;
		while(i) {
			f[fb * 256u + i] = (unsigned short)(f[fb * 256u + i] + 16);
			i = (i + (i & (0u - i))) & 0xFFu;
		}
		f[fb * 256u] = (unsigned short)(f[fb * 256u] + 16);
		if(f[fb * 256u] >= 0x4000u) v3_rescale(d, fb);
	}
	r[base + sym] = (unsigned short)(r[base + sym] + 32);
	i = (sym + 1) & 0xFFu;
	while(i) {
		f[base + i] = (unsigned short)(f[base + i] + 32);
		i = (i + (i & (0u - i))) & 0xFFu;
	}
	f[base] = (unsigned short)(f[base] + 32);
	t = (unsigned)d->escw[cidx] + (unsigned)f[base];
	if(t >= 0x4000u) v3_rescale(d, cidx);
}

static void v3_reset(Zc3* d)
{
	unsigned i, c;
	for(i = 0; i < V3_NFB * 256u; i++) {
		d->freq[i] = 0;
		d->raw[i] = 0;
	}
	for(c = V3_NFB; c < V3_NCTX; c++) {
		for(i = 0; i < 256; i++) {
			unsigned v = (i == 0) ? 256u : (i & (0u - i));
			d->freq[c * 256u + i] = (unsigned short)(v * 8u);
			d->raw[c * 256u + i] = 8;
		}
	}
	for(i = 0; i < V3_NFB; i++) d->escw[i] = 1;
	for(c = 0; c < V3_NLITM; c++) {
		d->litm[c][0] = 0;
		d->litm[c][1] = v3_litw1[c & 7u];
		d->litm[c][2] = 0x2000;
	}
}

static void v3_progress(Zc3* d)
{
	if(!d->cum || !d->ncum) return;
	while(d->cumidx < d->ncum - 1UL && d->produced > d->cum[d->cumidx]) {
		unsigned long v = d->cum[d->cumidx];
		d->cumidx++;
		if(v > d->thresh) {
			v3_reset(d);
			d->thresh = d->produced + 0x10000UL;
		}
	}
}

static unsigned v3_search(Zc3* d, unsigned cidx, unsigned di,
	unsigned point, unsigned ntotal)
{
	unsigned short* f = d->freq + cidx * 256u;
	unsigned short* r = d->raw + cidx * 256u;
	unsigned rem, p, step, q, hi, lo, sym;
	rem = (di - point) & V3_M16;
	p = 0;
	if(d->excl >= 0) {
		unsigned e = d->dict[d->excl];
		unsigned re = r[e];
		int ebon = 1;
		for(step = 0x80; step; step >>= 1) {
			q = p + step;
			if(ebon && q > e) {
				if((unsigned long)f[q] < (unsigned long)re + (unsigned long)rem) {
					p = q;
					ebon = 0;
					rem = (rem - f[q] + re) & V3_M16;
				}
			} else {
				if(rem > (unsigned)f[q]) {
					p = q;
					rem = (rem - f[q]) & V3_M16;
				}
			}
		}
	} else {
		for(step = 0x80; step; step >>= 1) {
			q = p + step;
			if(rem > (unsigned)f[q]) {
				p = q;
				rem = (rem - f[q]) & V3_M16;
			}
		}
	}
	if(p > 255) { d->err = 7; return 0; }
	sym = p;
	hi = (point + rem) & V3_M16;
	lo = (hi - r[sym]) & V3_M16;
	v3_narrow(d, ntotal, lo, hi);
	return sym;
}

static unsigned v3_bit(Zc3* d, unsigned al, int have, unsigned bitin)
{
	unsigned idx = ((unsigned)d->hist + al) & 0xFFu;
	unsigned short* e;
	unsigned bit, total, point, lo, hi;
	if(idx >= V3_NLITM) idx &= (V3_NLITM - 1u);
	e = d->litm[idx];
	if(!have) {
		total = e[2];
		if(total == 0) { d->err = 7; return 0; }
		point = v3_point(d, total);
		if(d->err) return 0;
		if(point < (unsigned)e[1]) {
			bit = 0; lo = e[0]; hi = e[1];
		} else {
			bit = 1; lo = e[1]; hi = e[2];
		}
		v3_narrow(d, total, lo, hi);
		if(d->err) return 0;
	} else {
		bit = bitin & 1u;
	}
	if(bit == 0) e[1] = (unsigned short)(e[1] + 64);
	e[2] = (unsigned short)(e[2] + 64);
	if(e[2] >= 0x4000u) {
		unsigned c0 = e[0], c1 = e[1], c2 = e[2];
		unsigned n1 = c0 + (((c1 - c0) + 1) >> 1);
		unsigned n2 = n1 + (((c2 - c1) + 1) >> 1);
		e[1] = (unsigned short)(n1 & V3_M16);
		e[2] = (unsigned short)(n2 & V3_M16);
	}
	d->hist = (unsigned char)(((unsigned)d->hist * 2u + bit) & 7u);
	return bit;
}

static unsigned v3_sym(Zc3* d, unsigned cidx)
{
	unsigned di, esc, total, point, fb, e, total0, point2, bx, sym, i;
	if(d->err) return 0;
	di = d->freq[cidx * 256u];
	if(d->excl >= 0) {
		e = d->dict[d->excl];
		di = (di - d->raw[cidx * 256u + e]) & V3_M16;
	}
	if(di == 0) {
		fb = V3_NFB + ((cidx >= 0x13Eu) ? 1u : 0u);
		di = d->freq[fb * 256u];
		if(d->excl >= 0) {
			e = d->dict[d->excl];
			di = (di - d->raw[fb * 256u + e]) & V3_M16;
		}
		if(di == 0) { d->err = 7; return 0; }
		point = v3_point(d, di);
		if(d->err) return 0;
		return v3_search(d, fb, di, point, di);
	}
	esc = d->escw[cidx];
	total = (di + esc) & V3_M16;
	point = v3_point(d, total);
	if(d->err) return 0;
	if(point < di) return v3_search(d, cidx, di, point, total);
	/* escape symbol: consume [di,total), then order-0 with
	 * multi-exclusion */
	v3_narrow(d, total, di, total);
	if(d->err) return 0;
	fb = V3_NFB + ((cidx >= 0x13Eu) ? 1u : 0u);
	total0 = d->freq[fb * 256u];
	e = (d->excl >= 0) ? (unsigned)d->dict[d->excl] : 256u;
	for(i = 0; i < 256; i++) {
		if(d->raw[cidx * 256u + i] != 0 || i == e)
			total0 = (total0 - d->raw[fb * 256u + i]) & V3_M16;
	}
	if(total0 == 0) { d->err = 7; return 0; }
	point2 = v3_point(d, total0);
	if(d->err) return 0;
	bx = 0;
	sym = 255;
	for(;;) {
		if(!(d->raw[cidx * 256u + sym] != 0 || sym == e))
			bx = (bx + d->raw[fb * 256u + sym]) & V3_M16;
		if(bx > point2) break;
		if(sym == 0) { d->err = 7; return 0; }
		sym--;
	}
	v3_narrow(d, total0, (bx - d->raw[fb * 256u + sym]) & V3_M16, bx);
	if(d->err) return 0;
	return sym;
}

static void v3_literal(Zc3* d)
{
	unsigned long c16 = d->ctx & V3_M16;
	unsigned lo8 = (unsigned)(c16 & 0xFFUL);
	unsigned hi8 = (unsigned)((c16 >> 8) & 0xFFUL);
	unsigned idx, b;
	if(c16 == 0xFFFFUL) idx = 314;
	else if(c16 == 0xD1DFUL) idx = 315;
	else if(c16 == 0xD3DFUL) idx = 316;
	else if(lo8 == 0xDFu) idx = v3_ord(hi8) ? 317u : 223u;
	else if(!v3_ord(lo8) && v3_ord(hi8)) idx = lo8 + 123u;
	else idx = lo8;
	b = v3_sym(d, idx);
	if(d->err) return;
	v3_update(d, idx, b);
	d->excl = -1;
	if(d->mpos >= 0 && (unsigned)d->dict[d->mpos] == b) {
		d->excl = (long)(((unsigned long)d->mpos + 1UL) & d->dmask);
	}
	v3_emit(d, b ^ 0xFFu);
	d->ctx = ((d->ctx << 8) + (unsigned long)b) & V3_M32;
	d->dict[d->dpos] = (unsigned char)b;
	if(d->dpos < 0x100UL)
		d->dict[d->dsz + d->dpos] = (unsigned char)b;
	d->dpos = (d->dpos + 1UL) & d->dmask;
}

static int v3_match(Zc3* d)
{
	unsigned long h, mpos, mpos0, dist, pos;
	unsigned long* he;
	unsigned al, idx, bit, ln, n;
	unsigned long mcx, mdp, mmp;
	unsigned b;
	if(d->err) return 0;
	pos = d->dpos;
	h = (d->ctx ^ (d->ctx >> 13)) & 0x3FFFFUL;
	he = (unsigned long*)(void*)(d->htab + h * 8UL);
	mpos = he[0];
	d->mpos = -1;
	if(mpos != V3_M32 && d->ctx == he[1]) {
		al = 1;
	} else {
		al = 0;
		mpos = (pos - d->lastd) & d->dmask;
		dist = (mpos - pos) & d->dmask;
		if(dist >= 0x104UL) {
			unsigned long mp4 = (mpos - 4UL) & d->dmask;
			unsigned long a0 = ((unsigned long)d->dict[mp4] << 24)
				| ((unsigned long)d->dict[mp4 + 1] << 16)
				| ((unsigned long)d->dict[mp4 + 2] << 8)
				| (unsigned long)d->dict[mp4 + 3];
			unsigned long c = d->ctx;
			if((a0 & 0xFFFFUL) == (c & 0xFFFFUL)
				|| (a0 & 0xFFFF00FFUL) == (c & 0xFFFF00FFUL)
				|| (a0 & 0xFFFFFF00UL) == (c & 0xFFFFFF00UL))
				al = 9;
		}
		if(al == 0) {
			unsigned long o = d->ord[d->ctx & 0xFFFFUL];
			if(o != V3_M32) {
				mpos = o;
				al = 0x11u;
			}
		}
	}
	he[0] = pos;
	he[1] = d->ctx;
	d->ord[d->ctx & 0xFFFFUL] = pos;
	if(al == 0) return 0;
	if(((mpos - pos) & d->dmask) < 0x100UL) return 0;
	al = al - 1u;
	mpos0 = mpos & d->dmask;
	if(d->hval[h] == 0) al += 0x18u;
	if(d->excl >= 0 && d->dict[d->excl] == d->dict[mpos0]) {
		v3_bit(d, al, 1, 1);
		return 0;
	}
	d->mpos = (long)mpos0;
	bit = v3_bit(d, al, 0, 0);
	if(d->err) return 0;
	if(bit != 0) {
		d->hval[h] = 0;
		return 0;
	}
	idx = v3_lenctx(d->hval[h]);
	/* the length decode must not inherit the literal exclusion */
	d->excl = -1;
	ln = v3_sym(d, idx);
	if(d->err) return 0;
	v3_update(d, idx, ln);
	d->hval[h] = (unsigned char)ln;
	d->lastd = (d->dpos - mpos0) & d->dmask;
	d->excl = -1;
	if(ln < 0xFFu) d->excl = (long)((mpos0 + ln) & d->dmask);
	n = (ln == 0) ? 256u : ln;
	mcx = d->ctx;
	mdp = d->dpos;
	mmp = mpos0;
	while(n--) {
		b = (unsigned)d->dict[mmp];
		v3_emit(d, b ^ 0xFFu);
		mcx = ((mcx << 8) + (unsigned long)b) & V3_M32;
		d->dict[mdp] = (unsigned char)b;
		if(mdp < 0x100UL) d->dict[d->dsz + mdp] = (unsigned char)b;
		mdp = (mdp + 1UL) & d->dmask;
		mmp = (mmp + 1UL) & d->dmask;
	}
	d->ctx = mcx;
	d->dpos = mdp;
	return 1;
}

int UhaDec3_03(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha3ReadCb rcb, void* rctx, Uha3WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	Zc3* d = (Zc3*)ctx;
	int err;
	if(!d || !rcb || !wcb) return 1;
	if(out_len == 0) return 0;
	d->inb = (unsigned char*)malloc(65536UL);
	d->outb = (unsigned char*)malloc(65536UL);
	if(!d->inb || !d->outb) {
		free(d->inb); free(d->outb);
		d->inb = 0; d->outb = 0;
		return 2;
	}
	d->rcb = rcb; d->rctx = rctx;
	d->wcb = wcb; d->wctx = wctx;
	d->in_pos = 0; d->in_avail = 0;
	d->out_fill = 0; d->produced = 0;
	d->target = out_len;
	d->err = 0;
	d->thresh = 0x10000UL;
	d->cumidx = 0;
	if(cum && ncum && cum[ncum - 1] == out_len) {
		d->cum = cum;
		d->ncum = ncum;
	} else {
		d->cum = 0;
		d->ncum = 0;
	}
	v3_rc_init(d);
	while(d->produced < d->target && !d->err) {
		if(!v3_match(d)) {
			if(d->err) break;
			v3_literal(d);
		}
		if(!d->err) v3_progress(d);
	}
	err = d->err;
	if(!err && d->out_fill) {
		if(wcb(wctx, d->outb, d->out_fill) != 0) err = 4;
	}
	free(d->inb);
	free(d->outb);
	d->inb = 0; d->outb = 0;
	return err;
}

/* ------------------------------------------------------------------ */
/* 0.4 core 3 (methods 8..23). Clean-room rewrite.
 * 330 model contexts: 328 ordinary/length contexts (escw 0..327)
 * plus two order-0 contexts (328 for contexts < 0x13E, 329 for the
 * length contexts >= 0x13E). The range coder is the 5-byte init
 * form. Match lengths are ordinary symbols in a context selected
 * by the previous length at the hash slot (0x4372B0 table, or the
 * 326/327 run contexts). The literal entries are 50 {e0,e1,e2} with
 * seeds 11..14 / 6..9 / 49 / 79 and e2 = 99, rescaled as
 * e1' = e0 + ((e1-e0+1)>>1), e2' = e1' + ((e2-e1+1)>>1).
 * Dictionary: 1 << (((method-8) % 16) + 10).
 */

#define V4_M32 0xFFFFFFFFUL
#define V4_M16 0xFFFFUL
#define V4_NCTX 330u
#define V4_NREAL 328u
#define V4_NLITM 50u

typedef struct Zc4
{
	Uha3ReadCb rcb;
	void* rctx;
	Uha3WriteCb wcb;
	void* wctx;
	unsigned char* inb;
	unsigned in_pos;
	unsigned in_avail;
	unsigned char* outb;
	unsigned out_fill;
	unsigned long produced;
	unsigned long target;
	int err;
	unsigned long code;
	unsigned long rng;
	unsigned long* htab;
	unsigned char* hval;
	unsigned long* ord;
	unsigned char* dict;
	unsigned short* freq;
	unsigned short* raw;
	unsigned short* escw;
	unsigned short litm[50][4];
	unsigned char state;
	unsigned long dsz;
	unsigned long dmask;
	unsigned long dpos;
	unsigned long ctx;
	long excl;
	long mpos;
	unsigned long run;
	unsigned long lastd;
	unsigned long thresh;
	const unsigned long* cum;
	unsigned long ncum;
	unsigned long cumidx;
} Zc4;

static const unsigned short v4_litw1[50] = {
	11, 12, 13, 14, 14, 14, 14, 14,
	11, 12, 13, 14, 14, 14, 14, 14,
	11, 12, 13, 14, 14, 14, 14, 14,
	6, 7, 8, 9, 9, 9, 9, 9,
	6, 7, 8, 9, 9, 9, 9, 9,
	6, 7, 8, 9, 9, 9, 9, 9,
	49, 79
};

static const unsigned char v4_st0[16] = {
	6, 0, 6, 0, 6, 1, 6, 2, 6, 3, 6, 3, 7, 4, 7, 5
};

static const unsigned short v4_lenctx[12] = {
	0x13E, 0x13E, 0x13F, 0x140, 0x141, 0x142,
	0x143, 0x143, 0x144, 0x144, 0x144, 0x144
};

static int v4_ord(unsigned c)
{
	if((c >= 133 && c <= 158) || (c >= 165 && c <= 190)) return 0;
	return 1;
}

static unsigned v4_len_context(unsigned prev)
{
	if(prev < 12u) return v4_lenctx[prev];
	return 0x145u;
}

static unsigned v4_ord0(unsigned ci)
{
	return (ci >= 0x13Eu) ? 329u : 328u;
}

static void v4_reset(Zc4* d);

int UhaDec3_04Init(void** pctx, int method)
{
	Zc4* d;
	unsigned long dsz;
	if(!pctx) return 1;
	*pctx = 0;
	if(method < 8 || method > 23) return 1;
	dsz = 1UL << (((method - 8) % 16) + 10);
	d = (Zc4*)malloc(sizeof(*d));
	if(!d) return 2;
	memset(d, 0, sizeof(*d));
	d->htab = (unsigned long*)malloc(262144UL * 2u * sizeof(unsigned long));
	d->hval = (unsigned char*)malloc(0x40000UL);
	d->ord = (unsigned long*)malloc(65536UL * sizeof(unsigned long));
	d->dict = (unsigned char*)calloc((size_t)(dsz + 256UL), 1);
	d->freq = (unsigned short*)malloc(V4_NCTX * 256u * sizeof(unsigned short));
	d->raw = (unsigned short*)malloc(V4_NCTX * 256u * sizeof(unsigned short));
	d->escw = (unsigned short*)malloc(V4_NREAL * sizeof(unsigned short));
	if(!d->htab || !d->hval || !d->ord || !d->dict || !d->freq || !d->raw || !d->escw) {
		free(d->htab); free(d->hval); free(d->ord); free(d->dict);
		free(d->freq); free(d->raw); free(d->escw); free(d);
		return 2;
	}
	memset(d->htab, 0xFF, 262144UL * 2u * sizeof(unsigned long));
	memset(d->hval, 0, 0x40000UL);
	memset(d->ord, 0xFF, 65536UL * sizeof(unsigned long));
	d->dsz = dsz;
	d->dmask = dsz - 1UL;
	d->dpos = 0;
	d->ctx = 0;
	d->excl = -1;
	d->mpos = -1;
	d->run = 0;
	d->lastd = 0;
	d->state = 0;
	v4_reset(d);
	*pctx = d;
	return 0;
}

void UhaDec3_04Free(void* ctx)
{
	Zc4* d = (Zc4*)ctx;
	if(!d) return;
	free(d->htab); free(d->hval); free(d->ord); free(d->dict);
	free(d->freq); free(d->raw); free(d->escw); free(d);
}

static int v4_get(Zc4* d)
{
	int n;
	if(d->err) return 0;
	if(d->in_pos >= d->in_avail) {
		n = d->rcb(d->rctx, d->inb, 65536);
		if(n <= 0) { d->err = 3; return 0; }
		d->in_avail = (unsigned)n;
		d->in_pos = 0;
	}
	return d->inb[d->in_pos++];
}

static void v4_emit(Zc4* d, unsigned b)
{
	if(d->produced < d->target) {
		d->outb[d->out_fill++] = (unsigned char)b;
		if(d->out_fill == 65536) {
			if(d->wcb(d->wctx, d->outb, d->out_fill) != 0) d->err = 4;
			d->out_fill = 0;
		}
	}
	d->produced++;
}

static void v4_rc_init(Zc4* d)
{
	int i;
	d->code = 0;
	d->rng = V4_M32;
	for(i = 0; i < 5; i++)
		d->code = ((d->code << 8) | (unsigned long)v4_get(d)) & V4_M32;
}

static int v4_renorm(Zc4* d, unsigned long lo, unsigned long hi)
{
	unsigned long rng, code;
	if(hi <= lo) { d->err = 7; return 0; }
	rng = (d->rng * ((hi - lo) & V4_M32)) & V4_M32;
	code = (d->code - ((d->rng * lo) & V4_M32)) & V4_M32;
	if(rng == 0) { d->err = 7; return 0; }
	while(rng < 0x1000000UL) {
		code = ((code << 8) | (unsigned long)v4_get(d)) & V4_M32;
		rng = (rng << 8) & V4_M32;
		if(d->err) return 0;
	}
	d->code = code;
	d->rng = rng;
	return 1;
}

static void v4_reset(Zc4* d)
{
	unsigned i;
	unsigned short* f328, *f329;
	unsigned short* r328, *r329;
	memset(d->freq, 0, (size_t)V4_NREAL * 512u);
	memset(d->raw, 0, (size_t)V4_NREAL * 512u);
	f328 = d->freq + 328u * 256u;
	f329 = d->freq + 329u * 256u;
	r328 = d->raw + 328u * 256u;
	r329 = d->raw + 329u * 256u;
	for(i = 0; i < 256; i++) {
		unsigned short v = (unsigned short)((i == 0) ? 256u : (i & (0u - i)));
		f328[i] = (unsigned short)(v * 8u);
		f329[i] = (unsigned short)(v * 8u);
		r328[i] = 8;
		r329[i] = 8;
	}
	for(i = 0; i < V4_NREAL; i++) d->escw[i] = 1;
	for(i = 0; i < V4_NLITM; i++) {
		d->litm[i][0] = 0;
		d->litm[i][1] = v4_litw1[i];
		d->litm[i][2] = 99;
		d->litm[i][3] = 0;
	}
	d->thresh = 0x10000UL;
}

static void v4_progress(Zc4* d)
{
	if(!d->cum || !d->ncum) return;
	while(d->cumidx < d->ncum - 1UL && d->produced > d->cum[d->cumidx]) {
		unsigned long v = d->cum[d->cumidx];
		d->cumidx++;
		if(v > d->thresh) {
			v4_reset(d);
			d->thresh = d->produced + 0x10000UL;
		}
	}
}

static void v4_rescale(Zc4* d, unsigned ci)
{
	unsigned short* raw = d->raw + ci * 256u;
	unsigned short* fr = d->freq + ci * 256u;
	unsigned long total = 0;
	unsigned i, k, j;
	for(i = 0; i < 256; i += 8) {
		unsigned long d0 = (unsigned long)raw[i] | ((unsigned long)raw[i+1] << 16);
		unsigned long d1 = (unsigned long)raw[i+2] | ((unsigned long)raw[i+3] << 16);
		unsigned long d2 = (unsigned long)raw[i+4] | ((unsigned long)raw[i+5] << 16);
		unsigned long d3 = (unsigned long)raw[i+6] | ((unsigned long)raw[i+7] << 16);
		unsigned long h0 = ((d0 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h1 = ((d1 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h2 = ((d2 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		unsigned long h3 = ((d3 + 0x10001UL) & 0xFFFEFFFEUL) >> 1;
		raw[i]     = (unsigned short)(h0 & V4_M16);
		raw[i+1]   = (unsigned short)((h0 >> 16) & V4_M16);
		raw[i+2]   = (unsigned short)(h1 & V4_M16);
		raw[i+3]   = (unsigned short)((h1 >> 16) & V4_M16);
		raw[i+4]   = (unsigned short)(h2 & V4_M16);
		raw[i+5]   = (unsigned short)((h2 >> 16) & V4_M16);
		raw[i+6]   = (unsigned short)(h3 & V4_M16);
		raw[i+7]   = (unsigned short)((h3 >> 16) & V4_M16);
		total = (total + h0 + h1) & V4_M32;
		total = (total + h2 + h3) & V4_M32;
	}
	for(i = 1; i < 256; i++) fr[i] = raw[i-1];
	fr[0] = (unsigned short)(((total & V4_M16) + ((total >> 16) & V4_M16)) & V4_M16);
	for(k = 1; k <= 64; k *= 2) {
		j = 256 - 2 * k;
		for(;;) {
			fr[j] = (unsigned short)(fr[j] + fr[j-k]);
			if(j == 2 * k) break;
			j -= 2 * k;
		}
	}
	if(ci < V4_NREAL) d->escw[ci] = (unsigned short)((d->escw[ci] >> 2) + 1);
}

static void v4_update(Zc4* d, unsigned ci, unsigned sym)
{
	unsigned short* r = d->raw + ci * 256u;
	unsigned short* f = d->freq + ci * 256u;
	unsigned fb = v4_ord0(ci);
	unsigned idx;
	unsigned t;
	if(r[sym] == 0) {
		unsigned short* r0 = d->raw + fb * 256u;
		unsigned short* f0 = d->freq + fb * 256u;
		d->escw[ci] = (unsigned short)(d->escw[ci] + 32);
		r0[sym] = (unsigned short)(r0[sym] + 16);
		idx = (sym + 1) & 0xFFu;
		while(idx) {
			f0[idx] = (unsigned short)(f0[idx] + 16);
			idx = (idx + (idx & (0u - idx))) & 0xFFu;
		}
		f0[0] = (unsigned short)(f0[0] + 16);
		if(f0[0] >= 0x4000u) v4_rescale(d, fb);
	}
	r[sym] = (unsigned short)(r[sym] + 32);
	idx = (sym + 1) & 0xFFu;
	while(idx) {
		f[idx] = (unsigned short)(f[idx] + 32);
		idx = (idx + (idx & (0u - idx))) & 0xFFu;
	}
	f[0] = (unsigned short)(f[0] + 32);
	t = (unsigned)d->escw[ci] + (unsigned)f[0];
	if(t >= 0x4000u) v4_rescale(d, ci);
}

static unsigned v4_sym(Zc4* d, unsigned ci)
{
	unsigned short* f = d->freq + ci * 256u;
	unsigned short* r = d->raw + ci * 256u;
	unsigned short* f0;
	unsigned short* r0;
	unsigned long scaled;
	unsigned di, esc, total, rem, p, step, q, sym, i;
	unsigned eb;
	int have_eb;
	unsigned fb;
	eb = 0;
	have_eb = (d->excl >= 0);
	if(have_eb) eb = (unsigned)d->dict[d->excl];
	di = f[0];
	if(have_eb) di = (di - r[eb]) & V4_M16;
	fb = v4_ord0(ci);
	if(di == 0) {
		ci = fb;
		f = d->freq + ci * 256u;
		r = d->raw + ci * 256u;
		di = f[0];
		if(have_eb) di = (di - r[eb]) & V4_M16;
		if(di == 0) { d->err = 7; return 0; }
		d->rng /= di;
		if(d->rng == 0) { d->err = 7; return 0; }
		scaled = d->code / d->rng;
		goto search;
	}
	esc = d->escw[ci];
	total = (di + esc) & V4_M16;
	if(total == 0) { d->err = 7; return 0; }
	d->rng /= total;
	if(d->rng == 0) { d->err = 7; return 0; }
	scaled = d->code / d->rng;
	if((scaled & V4_M16) >= di) goto escape;

search:
	rem = (di - scaled) & V4_M16;
	p = 0;
	if(have_eb) {
		unsigned re = r[eb];
		int ebon = 1;
		for(step = 0x80; step; step >>= 1) {
			q = p + step;
			if(ebon && q > eb) {
				if((unsigned long)f[q] < (unsigned long)re + (unsigned long)rem) {
					p = q;
					ebon = 0;
					rem = (rem - f[q] + re) & V4_M16;
				}
			} else {
				if(rem > (unsigned)f[q]) {
					p = q;
					rem = (rem - f[q]) & V4_M16;
				}
			}
		}
	} else {
		for(step = 0x80; step; step >>= 1) {
			q = p + step;
			if(rem > (unsigned)f[q]) {
				p = q;
				rem = (rem - f[q]) & V4_M16;
			}
		}
	}
	if(p > 255) { d->err = 7; return 0; }
	sym = p;
	{
		unsigned hi = (unsigned)((scaled + rem) & V4_M16);
		unsigned lo = (unsigned)((hi - r[sym]) & V4_M16);
		if(!v4_renorm(d, lo, hi)) return 0;
	}
	return sym;

escape:
	{
		unsigned hi = total;
		unsigned lo = di;
		if(!v4_renorm(d, lo, hi)) return 0;
	}
	f0 = d->freq + fb * 256u;
	r0 = d->raw + fb * 256u;
	{
		unsigned xsum = 0;
		for(i = 0; i < 256; i++) {
			if(r[i] != 0) xsum = (xsum + r0[i]) & V4_M16;
		}
		if(have_eb && r[eb] == 0) xsum = (xsum + r0[eb]) & V4_M16;
		total = (f0[0] - xsum) & V4_M16;
	}
	if(total == 0) { d->err = 7; return 0; }
	d->rng /= total;
	if(d->rng == 0) { d->err = 7; return 0; }
	scaled = d->code / d->rng;
	{
		unsigned scaled16 = (unsigned)(scaled & V4_M16);
		unsigned acc = 0;
		unsigned v40 = 256;
		if(have_eb) {
			unsigned stop = eb + 1u;
			while(v40 != stop) {
				--v40;
				if(r[v40] == 0) {
					acc = (acc + r0[v40]) & V4_M16;
					if(acc > scaled16) break;
				}
			}
			if(acc <= scaled16) {
				if(v40 == 0) { d->err = 7; return 0; }
				--v40;
			}
		}
		if(acc <= scaled16) {
			for(;;) {
				if(v40 == 0) { d->err = 7; return 0; }
				--v40;
				if(r[v40] == 0) break;
			}
			for(;;) {
				acc = (acc + r0[v40]) & V4_M16;
				if(acc > scaled16) break;
				for(;;) {
					if(v40 == 0) { d->err = 7; return 0; }
					--v40;
					if(r[v40] == 0) break;
				}
			}
		}
		if(v40 > 255) { d->err = 7; return 0; }
		sym = v40;
		{
			unsigned hi = acc;
			unsigned lo = (acc - r0[sym]) & V4_M16;
			if(!v4_renorm(d, lo, hi)) return 0;
		}
	}
	return sym;
}

static void v4_bitupd(Zc4* d, unsigned ai, unsigned bit)
{
	unsigned short* e;
	if((ai & 0xFFu) >= V4_NLITM) {
		d->err = 7;
		return;
	}
	e = d->litm[ai & 0xFFu];
	if(!bit) e[1] = (unsigned short)(e[1] + 64);
	e[2] = (unsigned short)(e[2] + 64);
	if(e[2] >= 0x4000u) {
		unsigned c0 = e[0];
		unsigned c1 = e[1];
		unsigned c2 = e[2];
		unsigned n1 = c0 + (((c1 - c0) + 1) >> 1);
		unsigned n2 = n1 + (((c2 - c1) + 1) >> 1);
		e[1] = (unsigned short)(n1 & V4_M16);
		e[2] = (unsigned short)(n2 & V4_M16);
	}
	d->state = v4_st0[2u * (unsigned)d->state + (bit & 1u)];
}

static unsigned v4_bit(Zc4* d, unsigned ai)
{
	unsigned short* e;
	unsigned total, scaled16, bit, lo, hi;
	if((ai & 0xFFu) >= V4_NLITM) {
		d->err = 7;
		return 0;
	}
	e = d->litm[ai & 0xFFu];
	total = e[2];
	if(total == 0) { d->err = 7; return 0; }
	d->rng /= total;
	if(d->rng == 0) { d->err = 7; return 0; }
	scaled16 = (unsigned)((d->code / d->rng) & V4_M16);
	if(scaled16 >= (unsigned)e[1]) {
		bit = 1;
		lo = e[1];
		hi = e[2];
	} else {
		bit = 0;
		lo = e[0];
		hi = e[1];
	}
	if(!v4_renorm(d, lo, hi)) return 0;
	v4_bitupd(d, ai, bit);
	return bit;
}

static void v4_literal(Zc4* d)
{
	unsigned long c16 = d->ctx & V4_M16;
	unsigned lo8 = (unsigned)(c16 & 0xFFUL);
	unsigned hi8 = (unsigned)((c16 >> 8) & 0xFFUL);
	unsigned midx;
	unsigned b;
	if(c16 == 0xFFFFUL) midx = 314u;
	else if(c16 == 0xD1DFUL) midx = 315u;
	else if(c16 == 0xD3DFUL) midx = 316u;
	else if(lo8 == 0xDFu) midx = v4_ord(hi8) ? 317u : 223u;
	else if(!v4_ord(lo8) && v4_ord(hi8)) midx = lo8 + 123u;
	else midx = lo8;
	b = v4_sym(d, midx);
	if(d->err) return;
	v4_update(d, midx, b);
	d->excl = -1;
	if(d->mpos >= 0 && (unsigned)d->dict[d->mpos] == b) {
		d->excl = (long)(((unsigned long)d->mpos + 1UL) & d->dmask);
	}
	v4_emit(d, b ^ 0xFFu);
	d->ctx = ((d->ctx << 8) + (unsigned long)b) & V4_M32;
	d->dict[d->dpos] = (unsigned char)b;
	if(d->dpos < 0x100UL)
		d->dict[d->dsz + d->dpos] = (unsigned char)b;
	d->dpos = (d->dpos + 1UL) & d->dmask;
}

static int v4_match(Zc4* d)
{
	unsigned long h;
	unsigned long* he;
	unsigned long mpos;
	unsigned long pos;
	unsigned long dist;
	unsigned al;
	unsigned idx;
	unsigned bit;
	unsigned ln;
	unsigned n;
	unsigned lctx;
	unsigned long mcx, mdp, mmp;
	if(d->err) return 0;
	pos = d->dpos;
	h = (d->ctx ^ (d->ctx >> 13)) & 0x3FFFFUL;
	he = d->htab + h * 2UL;
	mpos = he[0] & V4_M32;
	d->mpos = -1;
	al = 0;
	if(d->run) {
		mpos = (pos - d->lastd) & d->dmask;
		al = 48u + ((d->run > 2) ? 1u : 0u);
	}
	if(!al) {
		if(mpos == V4_M32 || d->ctx != he[1]) {
			unsigned long mp = (pos - d->lastd) & d->dmask;
			mpos = mp;
			dist = (mp - pos) & d->dmask;
			if(dist >= 0x104UL) {
				unsigned long mp4 = (mp - 4UL) & d->dmask;
				unsigned long a0 = ((unsigned long)d->dict[mp4] << 24)
					| ((unsigned long)d->dict[mp4 + 1] << 16)
					| ((unsigned long)d->dict[mp4 + 2] << 8)
					| (unsigned long)d->dict[mp4 + 3];
				unsigned long c = d->ctx;
				if((a0 & 0xFFFFUL) == (c & 0xFFFFUL)
					|| (a0 & 0xFFFF00FFUL) == (c & 0xFFFF00FFUL)
					|| (a0 & 0xFFFFFF00UL) == (c & 0xFFFFFF00UL))
					al = 9;
			}
			if(!al) {
				unsigned long o = d->ord[d->ctx & 0xFFFFUL] & V4_M32;
				if(o != V4_M32) {
					mpos = o;
					al = 0x11u;
				}
			}
		} else {
			al = 1;
		}
	}
	he[0] = pos;
	d->ord[d->ctx & 0xFFFFUL] = pos;
	he[1] = d->ctx;
	if(!al) {
		d->run = 0;
		return 0;
	}
	if(((mpos - pos) & d->dmask) < 0x100UL) {
		d->run = 0;
		return 0;
	}
	if(!d->run) {
		if(d->hval[h]) al += (unsigned)d->state - 1u;
		else al += (unsigned)d->state + 23u;
	}
	idx = al & 0xFFu;
	if(d->excl >= 0 && d->dict[d->excl] == d->dict[mpos & d->dmask]) {
		v4_bitupd(d, idx, 1);
		d->run = 0;
		return 0;
	}
	d->mpos = (long)(mpos & d->dmask);
	bit = v4_bit(d, idx);
	if(d->err) return 0;
	if(bit) {
		d->hval[h] = 0;
		d->run = 0;
		return 0;
	}
	if(d->run)
		lctx = 326u + ((d->run > 2) ? 1u : 0u);
	else
		lctx = v4_len_context((unsigned)d->hval[h]);
	d->excl = -1;
	ln = v4_sym(d, lctx);
	if(d->err) return 0;
	v4_update(d, lctx, ln);
	d->hval[h] = (unsigned char)ln;
	d->lastd = (d->dpos - (mpos & d->dmask)) & d->dmask;
	if(ln != 0xFFu) {
		d->excl = (long)((mpos + ln) & d->dmask);
		d->run = 0;
	} else {
		d->excl = -1;
		d->run++;
	}
	n = (ln == 0) ? 256u : ln;
	mcx = d->ctx;
	mdp = d->dpos;
	mmp = mpos & d->dmask;
	while(n--) {
		unsigned b = (unsigned)d->dict[mmp];
		v4_emit(d, b ^ 0xFFu);
		mcx = ((mcx << 8) + (unsigned long)b) & V4_M32;
		d->dict[mdp] = (unsigned char)b;
		if(mdp < 0x100UL) d->dict[d->dsz + mdp] = (unsigned char)b;
		mdp = (mdp + 1UL) & d->dmask;
		mmp = (mmp + 1UL) & d->dmask;
	}
	d->ctx = mcx;
	d->dpos = mdp;
	return 1;
}

int UhaDec3_04(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha3ReadCb rcb, void* rctx, Uha3WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	Zc4* d = (Zc4*)ctx;
	int err;
	if(!d || !rcb || !wcb) return 1;
	if(out_len == 0) return 0;
	d->inb = (unsigned char*)malloc(65536UL);
	d->outb = (unsigned char*)malloc(65536UL);
	if(!d->inb || !d->outb) {
		free(d->inb); free(d->outb);
		d->inb = 0; d->outb = 0;
		return 2;
	}
	d->rcb = rcb; d->rctx = rctx;
	d->wcb = wcb; d->wctx = wctx;
	d->in_pos = 0; d->in_avail = 0;
	d->out_fill = 0; d->produced = 0;
	d->target = out_len;
	d->err = 0;
	v4_reset(d);
	d->cumidx = 0;
	if(cum && ncum && cum[ncum - 1] == out_len) {
		d->cum = cum;
		d->ncum = ncum;
	} else {
		d->cum = 0;
		d->ncum = 0;
	}
	v4_rc_init(d);
	while(d->produced < d->target && !d->err) {
		if(!v4_match(d)) {
			if(d->err) break;
			v4_literal(d);
		}
		if(!d->err) v4_progress(d);
	}
	err = d->err;
	if(!err && d->out_fill) {
		if(wcb(wctx, d->outb, d->out_fill) != 0) err = 4;
	}
	free(d->inb);
	free(d->outb);
	d->inb = 0; d->outb = 0;
	return err;
}
