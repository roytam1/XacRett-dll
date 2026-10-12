#include "stdafx.h"
#include "AinCodec.h"
#include <stdlib.h>
#include <string.h>

#define AIN_WINDOW_SIZE 0x8000u
#define AIN_OVERRUN_SIZE 0x0100u
#define AIN_BUFFER_LIMIT (AIN_WINDOW_SIZE + AIN_OVERRUN_SIZE)
#define AIN_N_SYMS_LIT 272
#define AIN_N_SYMS_DIST 254
#define AIN_N_SYMS_PRE 19

struct AinBS
{
	const BYTE* data;
	unsigned long size;
	unsigned long pos;
	DWORD buf;
	int nbits;
	int exhausted;
};

static void AinBsInit(AinBS* bs, const BYTE* data, unsigned long size)
{
	bs->data = data; bs->size = size;
	bs->pos = 0; bs->buf = 0; bs->nbits = 0; bs->exhausted = 0;
}

static void AinBsRefill(AinBS* bs)
{
	if(bs->pos >= bs->size){ bs->exhausted = 1; return; }
	bs->buf |= (DWORD)bs->data[bs->pos++] << bs->nbits;
	bs->nbits += 8;
}

static WORD AinBsRead(AinBS* bs, int n)
{
	while(bs->nbits < n && !bs->exhausted)
		AinBsRefill(bs);
	if(bs->nbits < n)
		bs->nbits = n;// clamp on exhaustion
	return (WORD)(bs->buf & ((1u << n) - 1u));
}

static void AinBsConsume(AinBS* bs, int n)
{
	bs->buf >>= n;
	bs->nbits -= n;
}

static BYTE AinBsPeek8(AinBS* bs)
{
	while(bs->nbits < 8 && !bs->exhausted)
		AinBsRefill(bs);
	return (BYTE)(bs->buf & 0xFFu);
}

struct AinHuffTable
{
	WORD fast[256];
	short tree[8192];
	int tree_next;
};

static void AinSortSymbols(const BYTE* lengths, int n_sym,
	WORD* sorted, int* n_active_out)
{
	WORD buckets[256], chains[272 + 254 + 19 + 4];
	int i, out;
	memset(buckets,0xFF,256*sizeof(WORD));
	memset(chains,0xFF,(n_sym+1)*sizeof(WORD));
	for(i=n_sym-1;i>=0;i--)
	{
		BYTE bl = (BYTE)~lengths[i];
		chains[i] = buckets[bl];
		buckets[bl] = (WORD)i;
	}
	out = 0;
	for(i=0;i<256;i++)
	{
		WORD idx = buckets[i];
		while(idx!=0xFFFFu && (int)idx<n_sym)
		{
			sorted[out++] = idx;
			{
				WORD ni = chains[idx];
				chains[idx] = 0xFFFFu;
				idx = ni;
			}
		}
	}
	{
		int n_zero = 0;
		for(i=0;i<n_sym;i++)
			if(!lengths[i])
				n_zero++;
		*n_active_out = n_sym - n_zero;
	}
}

static WORD AinRol16(WORD v, int n)
{
	n &= 15;
	return n ? (WORD)((v<<n) | (v>>(16-n))) : v;
}

struct AinFillCtx
{
	AinHuffTable* tbl;
	const WORD* ss;
	int si;
	int dx;
	int count[17];
};

static void AinFillTable(AinFillCtx* ctx, int bx, int di, WORD ax, int depth)
{
	int d, dx, sym, cl, stride, idx, wi;
	WORD di_s, entry;
	if(depth>16)
		return;
	d = bx>>1;
	ctx->count[d]--;
	if(ctx->count[d]<0)
	{
		dx = ctx->dx;
		ctx->dx += 4;
		if(di>>1 < 8192)
			ctx->tbl->tree[di>>1] = (short)dx;
		if(d==8)
		{
			wi = (int)(AinRol16(ax,8) & 0xFFu);
			ctx->tbl->fast[wi] = (WORD)((unsigned)dx | 0xF800u);
		}
		AinFillTable(ctx,bx+2,dx,(WORD)(ax>>1),depth+1);
		AinFillTable(ctx,bx+2,dx+2,(WORD)((ax>>1)|0x8000u),depth+1);
		return;
	}
	if(ctx->si<0)
		return;
	sym = (int)ctx->ss[ctx->si--];
	if(di>>1 < 8192)
		ctx->tbl->tree[di>>1] = (short)(-(short)sym);
	if(d>8)
		return;
	cl = d+1;
	di_s = AinRol16(ax,(int)cl);
	entry = (WORD)(((((WORD)sym>>8) | (BYTE)(8*d))<<8) | (BYTE)(sym & 0xFF));
	stride = 1<<cl;
	for(idx=(int)di_s;idx<0x200;idx+=stride)
	{
		wi = idx>>1;
		if(wi<256)
			ctx->tbl->fast[wi] = entry;
	}
}

static void AinBuildTable(AinHuffTable* tbl, const BYTE* lengths, int n_syms)
{
	// tree[] is cleared per block, fast[] only per session (DOS BSS rule)
	memset(tbl->tree,0,sizeof(tbl->tree));
	tbl->tree_next = 2;
	{
		int freq[17] = {0};
		int i, l, total = 0;
		for(i=0;i<n_syms;i++)
			if(lengths[i] && lengths[i]<=16)
				freq[lengths[i]]++;
		for(l=1;l<=16;l++)
			total += freq[l];
		if(total==0)
			return;
		{
			static WORD ss[272 + 254 + 19 + 4];
			int n_active;
			AinFillCtx ctx;
			AinSortSymbols(lengths,n_syms,ss,&n_active);
			ctx.tbl = tbl;
			ctx.ss = ss;
			ctx.si = n_active-1;
			ctx.dx = 2;
			memcpy(ctx.count,freq,sizeof(ctx.count));
			ctx.count[0] = 0;
			AinFillTable(&ctx,0,0,0,0);
		}
	}
}

static int AinDecodeSymbol(AinBS* bs, const AinHuffTable* tbl)
{
	BYTE peek = AinBsPeek8(bs);
	WORD entry = tbl->fast[peek];
	int curr, bit;
	if(entry==0xFFFFu)
		return -1;
	if(entry<=0x4777u)
	{
		int clen = (int)(entry>>11) & 0x1F;
		AinBsRead(bs,clen);
		AinBsConsume(bs,clen);
		return (int)(entry & 0x7FFu);
	}
	AinBsRead(bs,8);
	AinBsConsume(bs,8);
	curr = (int)(entry & 0x7FFu);
	while(curr>0)
	{
		bit = (int)AinBsRead(bs,1);
		AinBsConsume(bs,1);
		curr = (int)tbl->tree[(curr>>1)+bit];
	}
	return (int)(WORD)(-(short)curr);
}

static void AinReadPrecode(AinBS* bs, AinHuffTable* tbl)
{
	BYTE pre_lens[AIN_N_SYMS_PRE] = {0};
	int i, n_trailing, to_read;
	n_trailing = (int)AinBsRead(bs,5);
	AinBsConsume(bs,5);
	if(n_trailing>(int)AIN_N_SYMS_PRE)
		n_trailing = (int)AIN_N_SYMS_PRE;
	to_read = (int)AIN_N_SYMS_PRE - n_trailing;
	for(i=0;i<to_read;i++)
	{
		int v = (int)AinBsRead(bs,3);
		AinBsConsume(bs,3);
		if(v>6)
		{
			while(AinBsRead(bs,1)){ AinBsConsume(bs,1); v++; }
			AinBsConsume(bs,1);
		}
		pre_lens[i] = (BYTE)v;
	}
	AinBuildTable(tbl,pre_lens,(int)AIN_N_SYMS_PRE);
}

static void AinReadLengths(AinBS* bs, const AinHuffTable* pre_tbl,
	BYTE* lengths, int n)
{
	int i;
	memset(lengths,0,n);
	for(i=0;i<n;)
	{
		int s = AinDecodeSymbol(bs,pre_tbl);
		if(s<0)
			break;
		if(s==0)
			lengths[i++] = 0;
		else if(s==1)
		{
			int run = (int)AinBsRead(bs,4);
			AinBsConsume(bs,4);
			run += 3;
			if(i+run>n)
				run = n-i;
			i += run;
		}
		else if(s==2)
		{
			int run = (int)AinBsRead(bs,9);
			AinBsConsume(bs,9);
			run += 20;
			if(i+run>n)
				run = n-i;
			i += run;
		}
		else
			lengths[i++] = (BYTE)(s-2);
	}
}

static void AinReadBlockTable(AinBS* bs, AinHuffTable* tbl, int n_syms,
	AinHuffTable* pre_tbl)
{
	static BYTE sym_lens[AIN_N_SYMS_LIT];
	int nm, active;
	AinReadPrecode(bs,pre_tbl);
	nm = (int)AinBsRead(bs,9);
	AinBsConsume(bs,9);
	if(nm>n_syms)
		nm = n_syms;
	active = n_syms-nm;
	memset(sym_lens,0,n_syms);
	if(active>0)
		AinReadLengths(bs,pre_tbl,sym_lens,active);
	AinBuildTable(tbl,sym_lens,n_syms);
}

struct AinGrow
{
	BYTE* data;
	unsigned long size;
	unsigned long cap;
};

static int AinGrowAppend(AinGrow* g, const BYTE* src, unsigned long n)
{
	if(g->size+n>g->cap)
	{
		unsigned long nc = g->cap ? g->cap*2 : (1u<<20);
		while(nc<g->size+n)
			nc *= 2;
		{
			BYTE* p = (BYTE*)realloc(g->data,nc);
			if(!p)
				return -1;
			g->data = p;
			g->cap = nc;
		}
	}
	memcpy(g->data+g->size,src,n);
	g->size += n;
	return 0;
}

BYTE* AinDecode(const BYTE* src, unsigned long srclen, unsigned long* outlen)
{
	static BYTE window[AIN_BUFFER_LIMIT + 256];
	static AinHuffTable sym_tbl, dist_tbl, pre_tbl;
	AinBS bs;
	AinGrow out = {0,0,0};

	*outlen = 0;
	memset(sym_tbl.fast,0,sizeof(sym_tbl.fast));
	memset(dist_tbl.fast,0,sizeof(dist_tbl.fast));
	memset(pre_tbl.fast,0,sizeof(pre_tbl.fast));
	AinBsInit(&bs,src,srclen);

	for(;;)
	{
		int di;
		if(bs.exhausted && bs.nbits<=0)
			break;
		memset(window,0,sizeof(window));
		{
			int sessionBit = (int)AinBsRead(&bs,1);
			AinBsConsume(&bs,1);
			(void)sessionBit;// consumed and ignored
		}
		di = 0;
		for(;;)
		{
			int block_done = 0;
			AinReadBlockTable(&bs,&sym_tbl,(int)AIN_N_SYMS_LIT,&pre_tbl);
			AinReadBlockTable(&bs,&dist_tbl,(int)AIN_N_SYMS_DIST,&pre_tbl);
			while(!block_done)
			{
				int sym, D;
				if((unsigned)di >= AIN_BUFFER_LIMIT)
				{
					if(AinGrowAppend(&out,window,AIN_WINDOW_SIZE)<0)
						return NULL;
					{
						int overrun = di-(int)AIN_WINDOW_SIZE;
						if(overrun>0)
							memmove(window,window+AIN_WINDOW_SIZE,overrun);
						di = overrun;
					}
				}
				if(bs.exhausted && bs.nbits<=0)
					goto final_flush;
				sym = AinDecodeSymbol(&bs,&sym_tbl);
				if(sym<0)
					goto final_flush;
				if(sym<256)
					window[di++] = (BYTE)sym;
				else
				{
					DWORD dist;
					int length_sym, length, k, src;
					D = sym-256;
					if(D==0)
						dist = 1;
					else if(D==1)
						dist = 2;
					else
					{
						WORD extra = AinBsRead(&bs,D-1);
						AinBsConsume(&bs,D-1);
						if(D==15 && extra==0x3FFFu)
						{
							block_done = 1;
							break;
						}
						dist = ((1u<<(D-1)) | extra) + 1u;
					}
					length_sym = AinDecodeSymbol(&bs,&dist_tbl);
					if(length_sym<0)
						goto final_flush;
					length = length_sym+3;
					src = di-(int)dist;
					if(src<0)
						src += (int)AIN_WINDOW_SIZE;
					for(k=0;k<length;k++)
						window[di+k] = window[src+k];
					di += length;
				}
			}
			{
				int end_bit = (int)AinBsRead(&bs,1);
				AinBsConsume(&bs,1);
				if(end_bit || (bs.exhausted && bs.nbits<=0))
				{
					if(di>0 && AinGrowAppend(&out,window,di)<0)
						return NULL;
					break;
				}
			}
		}
		continue;
final_flush:
		if(di>0 && AinGrowAppend(&out,window,di)<0)
			return NULL;
		*outlen = out.size;
		return out.data;
	}
	*outlen = out.size;
	return out.data;
}
