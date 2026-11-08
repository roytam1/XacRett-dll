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
