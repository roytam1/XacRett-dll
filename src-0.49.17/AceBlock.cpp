#include "stdafx.h"
#include "AceBlock.h"
#include <stdlib.h>
#include <string.h>

#define AB_EOF32 0xFFFFFFFFu
#define AB_MAXCODEWIDTH 11
#define AB_MAXLEN 259
#define AB_MAXDIST2 255
#define AB_MAXDIST3 8191
#define AB_MINDICBITS 10
#define AB_MAXDICBITS 22
#define AB_MINDIC (1u<<AB_MINDICBITS)
#define AB_MAXDIC (1u<<AB_MAXDICBITS)
#define AB_TYPECODE (260+AB_MAXDICBITS+1)
#define AB_NUMMAIN (260+AB_MAXDICBITS+2)
#define AB_NUMLEN 256
#define AB_MODE_LZ77 0
#define AB_MODE_DELTA 1
#define AB_MODE_EXE 2
#define AB_MODE_S8 3
#define AB_MODE_S16 4
#define AB_MODE_S32A 5
#define AB_MODE_S32B 6
#define AB_MODE_PIC 7
#define AB_HUFFWWBITS 3
#define AB_HUFFMAXWW ((1u<<AB_HUFFWWBITS)-1)
#define AB_SND_RUNLEN 32
#define AB_SND_TYPECODE (256+AB_SND_RUNLEN)
#define AB_SND_NUMCODES (256+AB_SND_RUNLEN+1)
#define AB_SND_MAXW 10
#define AB_SND_MAXCH 3
#define AB_SND_MAXMODELS 9
#define AB_PIC_NCTX 365

struct AbMode
{
	int present;
	unsigned mode;
	unsigned delta_dist;
	unsigned delta_len;
	unsigned exe_mode;
};

/*** bitstream (pull-based, MSB-first) ***/
struct AbBs
{
	AcePullCb pull;
	void* ctx;
	BYTE buf[4096];
	int head;
	int tail;
	unsigned __int64 bits;
	unsigned count;
	int eof;
};

static void AbBsFill(AbBs* b)
{
	if(b->head>0)
	{
		if(b->tail>b->head)
			memmove(b->buf,b->buf+b->head,b->tail-b->head);
		b->tail -= b->head;
		b->head = 0;
	}
	{
		int got = b->pull(b->ctx,b->buf+b->tail,(int)(sizeof(b->buf)-b->tail));
		if(got<=0)
			b->eof = 1;
		else
			b->tail += got;
	}
}

static void AbBsTop(AbBs* b)
{
	DWORD w;
	int n;
	if(b->count>32)
		return;
	w = 0;
	n = 0;
	while(n<4)
	{
		if(b->head>=b->tail)
		{
			AbBsFill(b);
			if(b->head>=b->tail)
				break;
		}
		w |= (DWORD)b->buf[b->head++]<<(n*8);
		n++;
	}
	if(n==0)
		return;
	b->bits |= (unsigned __int64)w<<(32-b->count);
	b->count += (unsigned)(n*8);
}

static DWORD AbBsPeek(AbBs* b, unsigned n)
{
	if(n==0 || n>=32)
		return 0;
	if(b->count<n)
		AbBsTop(b);
	return (DWORD)(b->bits>>(64-n));
}

static DWORD AbBsSkip(AbBs* b, unsigned n)
{
	if(n==0 || n>=32)
		return AB_EOF32;
	if(b->count<n)
	{
		AbBsTop(b);
		if(b->count<n)
			return AB_EOF32;
	}
	b->bits <<= n;
	b->count -= n;
	return 0;
}

static DWORD AbBsRead(AbBs* b, unsigned n)
{
	DWORD v = AbBsPeek(b,n);
	if(AbBsSkip(b,n)==AB_EOF32)
		return AB_EOF32;
	return v;
}

static DWORD AbBsKnown(AbBs* b, unsigned bits)
{
	DWORD v;
	if(bits<2)
		return bits;
	bits--;
	v = AbBsRead(b,bits);
	if(v==AB_EOF32)
		return AB_EOF32;
	return v + (1u<<bits);
}

static int AbBsGolomb(AbBs* b, unsigned r_bits, int signd, int* out)
{
	DWORD value = 0;
	DWORD bit;
	if(r_bits>0)
	{
		value = AbBsRead(b,r_bits);
		if(value==AB_EOF32)
			return -1;
	}
	for(;;)
	{
		bit = AbBsRead(b,1);
		if(bit==AB_EOF32)
			return -1;
		if(bit==0)
			break;
		value += 1u<<r_bits;
	}
	if(!signd)
	{
		*out = (int)value;
		return 0;
	}
	if(value & 1)
		*out = -(int)(value>>1)-1;
	else
		*out = (int)(value>>1);
	return 0;
}

/*** canonical Huffman tables (decode side) ***/
struct AbHuff
{
	WORD codes[2048];
	BYTE widths[512];
	unsigned ncodes;
	unsigned nwidths;
	unsigned max_width;
};

static int AbReadSymbol(const AbHuff* t, AbBs* bs, unsigned* sym);

static void AbSwap8(BYTE* a, int i, int j)
{
	BYTE t = a[i];
	a[i] = a[j];
	a[j] = t;
}

static void AbSwap16(WORD* a, int i, int j)
{
	WORD t = a[i];
	a[i] = a[j];
	a[j] = t;
}

static void AbSortSub(BYTE* keys, WORD* values, int left, int right)
{
	int nl = left;
	int nr = right;
	BYTE m = keys[right];
	for(;;)
	{
		while(keys[nl]>m)
			nl++;
		while(keys[nr]<m)
			nr--;
		if(nl<=nr)
		{
			AbSwap8(keys,nl,nr);
			AbSwap16(values,nl,nr);
			nl++;
			nr--;
		}
		if(nl>=nr)
			break;
	}
	if(left<nr)
	{
		if(left<nr-1)
			AbSortSub(keys,values,left,nr);
		else if(keys[left]<keys[nr])
		{
			AbSwap8(keys,left,nr);
			AbSwap16(values,left,nr);
		}
	}
	if(right>nl)
	{
		if(nl<right-1)
			AbSortSub(keys,values,nl,right);
		else if(keys[nl]<keys[right])
		{
			AbSwap8(keys,nl,right);
			AbSwap16(values,nl,right);
		}
	}
}

static int AbMakeTree(const BYTE* widths_in, unsigned nwidths,
	unsigned max_width, AbHuff* out)
{
	BYTE* widths;
	BYTE* sorted_widths;
	WORD* sorted_symbols;
	unsigned i, used, max_codes, pos;

	memset(out,0,sizeof(*out));
	if(nwidths==0 || nwidths>512)
		return -1;
	widths = (BYTE*)malloc(nwidths);
	sorted_widths = (BYTE*)malloc(nwidths);
	sorted_symbols = (WORD*)malloc(nwidths*sizeof(WORD));
	if(!widths || !sorted_widths || !sorted_symbols)
	{
		free(widths);
		free(sorted_widths);
		free(sorted_symbols);
		return -1;
	}
	memcpy(widths,widths_in,nwidths);
	memcpy(sorted_widths,widths_in,nwidths);
	for(i=0;i<nwidths;i++)
		sorted_symbols[i] = (WORD)i;
	if(nwidths>1)
		AbSortSub(sorted_widths,sorted_symbols,0,(int)nwidths-1);
	used = 0;
	while(used<nwidths && sorted_widths[used]!=0)
		used++;
	// degenerate tree (0 or 1 used symbols): force width 1, no dummy
	if(used<2)
	{
		widths[sorted_symbols[0]] = 1;
		if(used==0)
			used = 1;
	}
	max_codes = 1u<<max_width;
	pos = 0;
	for(i=used;i-- >0;)
	{
		WORD sym = sorted_symbols[i];
		BYTE wdt = sorted_widths[i];
		unsigned repeat, k;
		if(wdt==0 || wdt>max_width)
		{
			free(widths);
			free(sorted_widths);
			free(sorted_symbols);
			return -1;
		}
		repeat = 1u<<(max_width-wdt);
		if(pos+repeat>max_codes || pos+repeat>2048)
		{
			free(widths);
			free(sorted_widths);
			free(sorted_symbols);
			return -1;
		}
		for(k=0;k<repeat;k++)
			out->codes[pos++] = sym;
	}
	out->ncodes = pos;
	memcpy(out->widths,widths,nwidths);
	out->nwidths = nwidths;
	out->max_width = max_width;
	free(widths);
	free(sorted_widths);
	free(sorted_symbols);
	return 0;
}

static int AbReadTree(AbBs* bs, unsigned max_width, unsigned num_codes, AbHuff* out)
{
	DWORD num_widths, lower_width, upper_width;
	BYTE width_widths[16];
	AbHuff width_tree;
	BYTE widths[512];
	unsigned i;
	int rc;

	memset(out,0,sizeof(*out));
	num_widths = AbBsRead(bs,9);
	if(num_widths==AB_EOF32)
		return -1;
	num_widths += 1;
	if(num_widths>num_codes+1)
		num_widths = num_codes+1;
	lower_width = AbBsRead(bs,4);
	upper_width = AbBsRead(bs,4);
	if(lower_width==AB_EOF32 || upper_width==AB_EOF32)
		return -1;
	if(upper_width+1>16)
		return -1;
	for(i=0;i<upper_width+1;i++)
	{
		DWORD w = AbBsRead(bs,AB_HUFFWWBITS);
		if(w==AB_EOF32)
			return -1;
		width_widths[i] = (BYTE)w;
	}
	memset(&width_tree,0,sizeof(width_tree));
	rc = AbMakeTree(width_widths,upper_width+1,AB_HUFFMAXWW,&width_tree);
	if(rc!=0)
		return rc;
	memset(widths,0,sizeof(widths));
	i = 0;
	while(i<num_widths)
	{
		unsigned symbol;
		if(AbReadSymbol(&width_tree,bs,&symbol)!=0)
			return -1;
		if(symbol<upper_width)
			widths[i++] = (BYTE)symbol;
		else
		{
			DWORD length = AbBsRead(bs,4);
			unsigned k;
			if(length==AB_EOF32)
				return -1;
			length += 4;
			if(length>num_widths-i)
				length = num_widths-i;
			for(k=0;k<length;k++)
				widths[i++] = 0;
		}
	}
	if(upper_width>0)
	{
		for(i=1;i<num_widths;i++)
			widths[i] = (BYTE)((widths[i]+widths[i-1])%upper_width);
	}
	for(i=0;i<num_widths;i++)
	{
		if(widths[i]>0)
			widths[i] = (BYTE)(widths[i]+lower_width);
	}
	return AbMakeTree(widths,num_widths,max_width,out);
}

static int AbReadSymbol(const AbHuff* t, AbBs* bs, unsigned* sym)
{
	DWORD maxwidth_code;
	unsigned symbol;
	if(!t || !t->ncodes || !t->max_width)
		return -1;
	maxwidth_code = AbBsPeek(bs,t->max_width);
	if(maxwidth_code>=t->ncodes)
		return -1;
	symbol = t->codes[maxwidth_code];
	if(symbol>=t->nwidths)
		return -1;
	if(AbBsSkip(bs,t->widths[symbol])==AB_EOF32)
		return -1;
	*sym = symbol;
	return 0;
}

/*** LZ77 dictionary + symbol reader ***/
struct AbDict
{
	BYTE* data;
	unsigned long len;
	unsigned long cap;
	unsigned long dicsize;
};

struct AbSymReader
{
	AbHuff main_tree;
	AbHuff len_tree;
	unsigned syms_to_read;
	int trees_valid;
};

static int AbDictReserve(AbDict* d, unsigned long extra)
{
	unsigned long need = d->len+extra;
	BYTE* nd;
	if(need<=d->cap)
		return 0;
	if(d->cap==0)
		d->cap = 65536;
	while(d->cap<need)
		d->cap *= 2;
	nd = (BYTE*)realloc(d->data,d->cap);
	if(!nd)
		return -1;
	d->data = nd;
	return 0;
}

static void AbDictTruncate(AbDict* d)
{
	if(d->len>8*d->dicsize)
	{
		memmove(d->data,d->data+(d->len-d->dicsize),d->dicsize);
		d->len = d->dicsize;
	}
}

static int AbModeRead(AbBs* bs, AbMode* mode)
{
	DWORD m, a, b;
	memset(mode,0,sizeof(*mode));
	m = AbBsRead(bs,8);
	if(m==AB_EOF32)
		return -1;
	mode->present = 1;
	mode->mode = m;
	if(m==AB_MODE_DELTA)
	{
		a = AbBsRead(bs,8);
		b = AbBsRead(bs,17);
		if(a==AB_EOF32 || b==AB_EOF32)
			return -1;
		mode->delta_dist = a;
		mode->delta_len = b;
		if(mode->delta_dist==0)
			return -1;
	}
	else if(m==AB_MODE_EXE)
	{
		a = AbBsRead(bs,8);
		if(a==AB_EOF32)
			return -1;
		mode->exe_mode = a;
	}
	return 0;
}

struct AbLz
{
	AbDict dict;
	AbSymReader sr;
	DWORD dh[4];
};

static int AbSrReadTrees(AbSymReader* sr, AbBs* bs)
{
	DWORD n;
	int rc;
	memset(&sr->main_tree,0,sizeof(sr->main_tree));
	memset(&sr->len_tree,0,sizeof(sr->len_tree));
	rc = AbReadTree(bs,AB_MAXCODEWIDTH,AB_NUMMAIN,&sr->main_tree);
	if(rc!=0)
		return rc;
	rc = AbReadTree(bs,AB_MAXCODEWIDTH,AB_NUMLEN-1,&sr->len_tree);
	if(rc!=0)
		return rc;
	n = AbBsRead(bs,15);
	if(n==AB_EOF32)
		return -1;
	sr->syms_to_read = n;
	sr->trees_valid = 1;
	return 0;
}

static int AbSrReadMain(AbSymReader* sr, AbBs* bs, unsigned* sym)
{
	int rc;
	if(sr->syms_to_read==0)
	{
		rc = AbSrReadTrees(sr,bs);
		if(rc!=0)
			return rc;
	}
	sr->syms_to_read--;
	return AbReadSymbol(&sr->main_tree,bs,sym);
}

static DWORD AbDhRetrieve(DWORD* dh, unsigned offset)
{
	unsigned idx = 4-offset-1;
	DWORD dist = dh[idx];
	memmove(&dh[idx],&dh[idx+1],(3-idx)*sizeof(DWORD));
	dh[3] = dist;
	return dist;
}

static void AbDhAppend(DWORD* dh, DWORD dist)
{
	memmove(&dh[0],&dh[1],3*sizeof(DWORD));
	dh[3] = dist;
}

static int AbLzRead(AbLz* z, AbBs* bs, unsigned long want, BYTE** chunk,
	unsigned long* n, int* switched, AbMode* nm)
{
	unsigned long have = 0;
	unsigned long start;
	int rc;
	memset(nm,0,sizeof(*nm));
	*chunk = NULL;
	*n = 0;
	*switched = 0;
	if(want==0)
		return 0;
	while(have<want)
	{
		unsigned symbol;
		rc = AbSrReadMain(&z->sr,bs,&symbol);
		if(rc!=0)
			return -1;
		if(symbol<=255)
		{
			if(AbDictReserve(&z->dict,1)!=0)
				return -1;
			z->dict.data[z->dict.len++] = (BYTE)symbol;
			have++;
		}
		else if(symbol<AB_TYPECODE)
		{
			unsigned copy_len;
			DWORD copy_dist;
			unsigned long src, i;
			if(symbol<=259)
			{
				unsigned offset = symbol & 3;
				rc = AbReadSymbol(&z->sr.len_tree,bs,&copy_len);
				if(rc!=0)
					return -1;
				copy_dist = AbDhRetrieve(z->dh,offset);
				if(offset>1)
					copy_len += 3;
				else
					copy_len += 2;
			}
			else
			{
				copy_dist = AbBsKnown(bs,symbol-260);
				if(copy_dist==AB_EOF32)
					return -1;
				rc = AbReadSymbol(&z->sr.len_tree,bs,&copy_len);
				if(rc!=0)
					return -1;
				AbDhAppend(z->dh,copy_dist);
				if(copy_dist<=AB_MAXDIST2)
					copy_len += 2;
				else if(copy_dist<=AB_MAXDIST3)
					copy_len += 3;
				else
					copy_len += 4;
			}
			copy_dist += 1;
			if(have+copy_len>want)
				return -1;
			if(z->dict.len<copy_dist)
				return -1;
			if(AbDictReserve(&z->dict,copy_len)!=0)
				return -1;
			src = z->dict.len-copy_dist;
			for(i=0;i<copy_len;i++)
				z->dict.data[z->dict.len+i] = z->dict.data[src+i];
			z->dict.len += copy_len;
			have += copy_len;
		}
		else if(symbol==AB_TYPECODE)
		{
			rc = AbModeRead(bs,nm);
			if(rc!=0)
				return -1;
			*switched = 1;
			break;
		}
		else
			return -1;
	}
	start = z->dict.len-have;
	*chunk = (BYTE*)malloc(have ? have : 1);
	if(!*chunk)
		return -1;
	if(have)
		memcpy(*chunk,z->dict.data+start,have);
	*n = have;
	AbDictTruncate(&z->dict);
	return 0;
}

/*** SOUND preprocessing decoder ***/
static const unsigned AbSndNumCh[4] = {1,2,3,3};
static const unsigned AbSndUseCh[4][4] = {
	{0,0,0,0},
	{0,1,0,1},
	{0,1,0,2},
	{1,0,2,0}
};

static BYTE AbSndQuant[256];
static int AbSndQuantReady = 0;

static int AbBitLength(unsigned x)
{
	int n = 0;
	while(x)
	{
		n++;
		x >>= 1;
	}
	return n;
}

static void AbSndQuantInit(void)
{
	int i;
	if(AbSndQuantReady)
		return;
	memset(AbSndQuant,0,sizeof(AbSndQuant));
	for(i=1;i<129;i++)
		AbSndQuant[(BYTE)(-i)] = AbSndQuant[i] = (BYTE)AbBitLength((unsigned)i);
	AbSndQuantReady = 1;
}

struct AbSndSr
{
	AbHuff trees[AB_SND_MAXMODELS];
	unsigned num_models;
	unsigned syms_to_read;
	int trees_valid;
};

struct AbSndCh
{
	AbSndSr* sr;
	unsigned model_base_idx;
	int pred_dif_cnt[2];
	int last_pred_dif_cnt[2];
	int rar_dif_cnt[4];
	int rar_coeff[4];
	int rar_dif[9];
	unsigned byte_count;
	int last_sample;
	int last_delta;
	int adapt_model_cnt;
	int adapt_model_use;
	int get_state;
	int get_code;
	int got_mode;
	AbMode mode;
};

struct AbSound
{
	unsigned mode_idx;
	unsigned num_channels;
	AbSndSr sr;
	AbSndCh ch[AB_SND_MAXCH];
};

static int AbSndRarPredict(AbSndCh* ch);

static void AbSndSrFree(AbSndSr* sr)
{
	unsigned i;
	for(i=0;i<AB_SND_MAXMODELS;i++)
		memset(&sr->trees[i],0,sizeof(sr->trees[i]));
	sr->trees_valid = 0;
	sr->syms_to_read = 0;
}

static int AbSndSrTrees(AbSndSr* sr, AbBs* bs)
{
	unsigned i;
	DWORD n;
	int rc;
	AbSndSrFree(sr);
	for(i=0;i<sr->num_models;i++)
	{
		rc = AbReadTree(bs,AB_SND_MAXW,AB_SND_NUMCODES,&sr->trees[i]);
		if(rc!=0)
			return rc;
	}
	n = AbBsRead(bs,15);
	if(n==AB_EOF32)
		return -1;
	sr->syms_to_read = n;
	sr->trees_valid = 1;
	return 0;
}

static int AbSndSrSymbol(AbSndSr* sr, AbBs* bs, unsigned model, unsigned* sym)
{
	int rc;
	if(sr->syms_to_read==0)
	{
		rc = AbSndSrTrees(sr,bs);
		if(rc!=0)
			return rc;
	}
	sr->syms_to_read--;
	return AbReadSymbol(&sr->trees[model],bs,sym);
}

static int AbSndChSymbol(AbSndCh* ch, AbBs* bs, unsigned* sym)
{
	unsigned model = (unsigned)ch->get_state<<1;
	if(model==0)
		model += (unsigned)ch->adapt_model_use;
	model += ch->model_base_idx;
	return AbSndSrSymbol(ch->sr,bs,model,sym);
}

static int AbSndChGet(AbSndCh* ch, AbBs* bs, int* sample_or_mode)
{
	int value = 0;
	ch->got_mode = 0;
	if(ch->get_state!=2)
	{
		unsigned code;
		int rc = AbSndChSymbol(ch,bs,&code);
		if(rc!=0)
			return rc;
		ch->get_code = (int)code;
		if(ch->get_code==AB_SND_TYPECODE)
		{
			rc = AbModeRead(bs,&ch->mode);
			if(rc!=0)
				return rc;
			ch->got_mode = 1;
			*sample_or_mode = 0;
			return 0;
		}
	}
	if(ch->get_state==0)
	{
		if(ch->get_code>=AB_SND_RUNLEN)
		{
			value = ch->get_code-AB_SND_RUNLEN;
			ch->adapt_model_cnt = (ch->adapt_model_cnt*7>>3)+value;
			if(ch->adapt_model_cnt>40)
				ch->adapt_model_use = 1;
			else
				ch->adapt_model_use = 0;
		}
		else
			ch->get_state = 2;
	}
	else if(ch->get_state==1)
	{
		value = ch->get_code;
		ch->get_state = 0;
	}
	if(ch->get_state==2)
	{
		if(ch->get_code==0)
			ch->get_state = 1;
		else
			ch->get_code -= 1;
		value = 0;
	}
	if(value & 1)
		*sample_or_mode = 255-(value>>1);
	else
		*sample_or_mode = value>>1;
	return 0;
}

static int AbSndPredicted(AbSndCh* ch)
{
	int v = (8*ch->last_sample +
		ch->rar_coeff[0]*ch->rar_dif_cnt[0] +
		ch->rar_coeff[1]*ch->rar_dif_cnt[1] +
		ch->rar_coeff[2]*ch->rar_dif_cnt[2] +
		ch->rar_coeff[3]*ch->rar_dif_cnt[3])>>3;
	return v & 0xFF;
}

static int AbAbs(int x)
{
	return x<0 ? -x : x;
}

static void AbSndRarAdjust(AbSndCh* ch, int sample)
{
	int pred_sample, pred_dif, i;
	AbSndQuantInit();
	ch->byte_count++;
	pred_sample = AbSndPredicted(ch);
	pred_dif = ((signed char)(pred_sample-sample))<<3;
	ch->rar_dif[0] += AbAbs(pred_dif-ch->rar_dif_cnt[0]);
	ch->rar_dif[1] += AbAbs(pred_dif+ch->rar_dif_cnt[0]);
	ch->rar_dif[2] += AbAbs(pred_dif-ch->rar_dif_cnt[1]);
	ch->rar_dif[3] += AbAbs(pred_dif+ch->rar_dif_cnt[1]);
	ch->rar_dif[4] += AbAbs(pred_dif-ch->rar_dif_cnt[2]);
	ch->rar_dif[5] += AbAbs(pred_dif+ch->rar_dif_cnt[2]);
	ch->rar_dif[6] += AbAbs(pred_dif-ch->rar_dif_cnt[3]);
	ch->rar_dif[7] += AbAbs(pred_dif+ch->rar_dif_cnt[3]);
	ch->rar_dif[8] += AbAbs(pred_dif);
	ch->last_delta = (signed char)(sample-ch->last_sample);
	ch->pred_dif_cnt[0] += AbSndQuant[(BYTE)(pred_dif>>3)];
	ch->pred_dif_cnt[1] += AbSndQuant[(BYTE)(ch->last_sample-sample)];
	ch->last_sample = sample;
	if((ch->byte_count & 0x1F)==0)
	{
		int min_dif = 0xFFFF;
		int min_dif_pos = 8;
		for(i=8;i>=0;i--)
		{
			if(ch->rar_dif[i]<=min_dif)
			{
				min_dif = ch->rar_dif[i];
				min_dif_pos = i;
			}
			ch->rar_dif[i] = 0;
		}
		if(min_dif_pos!=8)
		{
			i = min_dif_pos>>1;
			if((min_dif_pos & 1)==0)
			{
				if(ch->rar_coeff[i]>=-16)
					ch->rar_coeff[i] -= 1;
			}
			else
			{
				if(ch->rar_coeff[i]<=16)
					ch->rar_coeff[i] += 1;
			}
		}
		if((ch->byte_count & 0xFF)==0)
		{
			for(i=0;i<2;i++)
			{
				ch->pred_dif_cnt[i] -= ch->last_pred_dif_cnt[i];
				ch->last_pred_dif_cnt[i] = ch->pred_dif_cnt[i];
			}
		}
	}
	ch->rar_dif_cnt[3] = ch->rar_dif_cnt[2];
	ch->rar_dif_cnt[2] = ch->rar_dif_cnt[1];
	ch->rar_dif_cnt[1] = ch->last_delta-ch->rar_dif_cnt[0];
	ch->rar_dif_cnt[0] = ch->last_delta;
}

static int AbSoundReinit(AbSound* s, unsigned mode)
{
	unsigned i;
	if(mode<AB_MODE_S8 || mode>AB_MODE_S32B)
		return -1;
	AbSndSrFree(&s->sr);
	memset(s,0,sizeof(*s));
	s->mode_idx = mode-AB_MODE_S8;
	s->num_channels = AbSndNumCh[s->mode_idx];
	s->sr.num_models = s->num_channels*3;
	for(i=0;i<s->num_channels;i++)
	{
		s->ch[i].sr = &s->sr;
		s->ch[i].model_base_idx = 3*i;
	}
	return 0;
}

static int AbSoundRead(AbSound* s, AbBs* bs, unsigned long want,
	BYTE** chunk, unsigned long* n, int* switched, AbMode* nm)
{
	unsigned long cap = want & ~(unsigned long)3;
	BYTE* out;
	unsigned long len = 0;
	unsigned i;
	int rc;
	memset(nm,0,sizeof(*nm));
	*chunk = NULL;
	*n = 0;
	*switched = 0;
	if(want==0)
		return 0;
	out = (BYTE*)malloc(cap ? cap : 1);
	if(!out)
		return -1;
	for(i=0;i<cap;i++)
	{
		unsigned channel = AbSndUseCh[s->mode_idx][i%4];
		int value;
		BYTE sample;
		rc = AbSndChGet(&s->ch[channel],bs,&value);
		if(rc!=0)
		{
			free(out);
			return -1;
		}
		if(s->ch[channel].got_mode)
		{
			*nm = s->ch[channel].mode;
			*switched = 1;
			break;
		}
		sample = (BYTE)(value+AbSndRarPredict(&s->ch[channel]));
		out[i] = sample;
		len++;
		AbSndRarAdjust(&s->ch[channel],(signed char)sample);
	}
	*chunk = out;
	*n = len;
	return 0;
}

static int AbSndRarPredict(AbSndCh* ch)
{
	if(ch->pred_dif_cnt[0]>ch->pred_dif_cnt[1])
		return ch->last_sample;
	return AbSndPredicted(ch);
}

/*** PIC preprocessing decoder ***/
struct AbPicCtx
{
	int used_counter;
	int predictor_number;
	int average_counter;
	int error_counters[4];
};

struct AbPicErrModel
{
	AbPicCtx ctx[AB_PIC_NCTX];
};

struct AbPicDec
{
	int pixel_a, pixel_b, pixel_c, pixel_d, pixel_x;
	int kind;
};

struct AbPic
{
	int width;
	int planes;
	AbPicErrModel plane0;
	AbPicErrModel plane1n;
	BYTE* prevrow;
	BYTE* leftover;
	unsigned long leftover_len;
	unsigned long leftover_cap;
};

static int AbCDiv(int q, int d)
{
	int s;
	if(d==0)
		return 0;
	s = ((q<0)==(d<0)) ? 1 : -1;
	if(q<0)
		q = -q;
	if(d<0)
		d = -d;
	return s*(q/d);
}

static BYTE AbPicDifW[256];
static int AbPicQuant[511];
static int AbPicQuant9[511];
static int AbPicQuant81[511];
static int AbPicTablesReady = 0;

static void AbPicTablesInit(void)
{
	int i, v;
	if(AbPicTablesReady)
		return;
	for(i=0;i<128;i++)
		AbPicDifW[i] = (BYTE)AbBitLength((unsigned)(2*i));
	for(i=-128;i<0;i++)
		AbPicDifW[(BYTE)i] = (BYTE)AbBitLength((unsigned)(-2*i-1));
	i = 0;
	for(v=-255;v<-20;v++)
		AbPicQuant[i++] = -4;
	for(v=-20;v<-6;v++)
		AbPicQuant[i++] = -3;
	for(v=-6;v<-2;v++)
		AbPicQuant[i++] = -2;
	for(v=-2;v<0;v++)
		AbPicQuant[i++] = -1;
	AbPicQuant[i++] = 0;
	for(v=1;v<3;v++)
		AbPicQuant[i++] = 1;
	for(v=3;v<7;v++)
		AbPicQuant[i++] = 2;
	for(v=7;v<21;v++)
		AbPicQuant[i++] = 3;
	for(v=21;v<256;v++)
		AbPicQuant[i++] = 4;
	for(i=0;i<511;i++)
	{
		AbPicQuant9[i] = 9*AbPicQuant[i];
		AbPicQuant81[i] = 81*AbPicQuant[i];
	}
	AbPicTablesReady = 1;
}

static void AbPicErrReset(AbPicErrModel* m)
{
	int i;
	for(i=0;i<AB_PIC_NCTX;i++)
	{
		m->ctx[i].used_counter = 0;
		m->ctx[i].predictor_number = 0;
		m->ctx[i].average_counter = 4;
		m->ctx[i].error_counters[0] = 0;
		m->ctx[i].error_counters[1] = 0;
		m->ctx[i].error_counters[2] = 0;
		m->ctx[i].error_counters[3] = 0;
	}
}

static void AbPicDecInit0(AbPicDec* d)
{
	memset(d,0,sizeof(*d));
	d->kind = 0;
}

static void AbPicDecInitDiff(AbPicDec* d, int kind)
{
	d->pixel_a = 128;
	d->pixel_b = 128;
	d->pixel_c = 128;
	d->pixel_d = 0;
	d->pixel_x = 128;
	d->kind = kind;
}

static void AbPicDecShift(AbPicDec* d)
{
	d->pixel_c = d->pixel_a;
	d->pixel_a = d->pixel_d;
	d->pixel_b = d->pixel_x;
}

static int AbPicDecContext(AbPicDec* d)
{
	int ctx = AbPicQuant81[255+d->pixel_d-d->pixel_a] +
		AbPicQuant9[255+d->pixel_a-d->pixel_c] +
		AbPicQuant[255+d->pixel_c-d->pixel_b];
	return ctx<0 ? -ctx : ctx;
}

static int AbPicDecPredict(AbPicDec* d, int use_predictor)
{
	if(use_predictor==0)
		return d->pixel_a;
	if(use_predictor==1)
		return d->pixel_b;
	if(use_predictor==2)
		return (d->pixel_a+d->pixel_b)>>1;
	return (d->pixel_a+d->pixel_b-d->pixel_c) & 0xFF;
}

static int AbPicDecUpdateX(AbPicDec* d, AbBs* bs, AbPicCtx* context)
{
	int r, epsilon, predicted, i, best = 0;
	int rc;
	context->used_counter += 1;
	r = AbCDiv(context->average_counter,context->used_counter);
	rc = AbBsGolomb(bs,(unsigned)AbBitLength((unsigned)r),1,&epsilon);
	if(rc!=0)
		return rc;
	predicted = AbPicDecPredict(d,context->predictor_number);
	d->pixel_x = (predicted+epsilon) & 0xFF;
	if(epsilon<0)
		context->average_counter += -epsilon;
	else
		context->average_counter += epsilon;
	if(context->used_counter==128)
	{
		context->used_counter >>= 1;
		context->average_counter >>= 1;
	}
	for(i=0;i<4;i++)
	{
		int dif = d->pixel_x-AbPicDecPredict(d,i);
		context->error_counters[i] += AbPicDifW[(BYTE)dif];
		if(i==0 || context->error_counters[i]<context->error_counters[best])
			best = i;
	}
	context->predictor_number = best;
	if(context->error_counters[0]>0x7F ||
		context->error_counters[1]>0x7F ||
		context->error_counters[2]>0x7F ||
		context->error_counters[3]>0x7F)
	{
		for(i=0;i<4;i++)
			context->error_counters[i] >>= 1;
	}
	return 0;
}

static void AbPicDecUpdateD(AbPicDec* d, int thisplane_d, int refplane_d)
{
	if(d->kind==0)
		d->pixel_d = thisplane_d;
	else if(d->kind==1)
		d->pixel_d = (128+thisplane_d-refplane_d) & 0xFF;
	else
		d->pixel_d = (128+thisplane_d-(refplane_d*11>>4)) & 0xFF;
}

static int AbPicDecProduce(AbPicDec* d, int refplane_x)
{
	if(d->kind==0)
		return d->pixel_x;
	if(d->kind==1)
		return (d->pixel_x+refplane_x-128) & 0xFF;
	return (d->pixel_x+(refplane_x*11>>4)-128) & 0xFF;
}

static int AbPicReinit(AbPic* p, AbBs* bs)
{
	int w, planes, rc;
	AbPicTablesInit();
	rc = AbBsGolomb(bs,12,0,&w);
	if(rc!=0)
		return rc;
	rc = AbBsGolomb(bs,2,0,&planes);
	if(rc!=0)
		return rc;
	if(w<=0 || planes<=0)
		return -1;
	free(p->prevrow);
	free(p->leftover);
	memset(p,0,sizeof(*p));
	p->width = w;
	p->planes = planes;
	p->prevrow = (BYTE*)calloc((w+planes),1);
	if(!p->prevrow)
		return -1;
	AbPicErrReset(&p->plane0);
	AbPicErrReset(&p->plane1n);
	return 0;
}

static int AbPicRow(AbPic* p, AbBs* bs, BYTE* row_out)
{
	int plane, col;
	BYTE* row;
	unsigned long rowsz = (unsigned long)(p->width+p->planes);
	row = (BYTE*)calloc(rowsz,1);
	if(!row)
		return -1;
	for(plane=0;plane<p->planes;plane++)
	{
		AbPicErrModel* errmodel;
		AbPicDec decoder;
		int ref_m1;
		if(plane==0)
		{
			errmodel = &p->plane0;
			AbPicDecInit0(&decoder);
		}
		else
		{
			DWORD kind;
			errmodel = &p->plane1n;
			kind = AbBsRead(bs,2);
			if(kind==AB_EOF32)
			{
				free(row);
				return -1;
			}
			if(kind>2)
			{
				free(row);
				return -1;
			}
			if(kind==0)
				AbPicDecInit0(&decoder);
			else
				AbPicDecInitDiff(&decoder,(int)kind);
		}
		ref_m1 = (plane-1<0) ? 0 : p->prevrow[plane-1];
		AbPicDecUpdateD(&decoder,p->prevrow[plane],ref_m1);
		for(col=plane;col<p->width;col+=p->planes)
		{
			int ctx_i, rc, ref_x;
			AbPicDecShift(&decoder);
			AbPicDecUpdateD(&decoder,
				p->prevrow[col+p->planes],
				p->prevrow[col+p->planes-1]);
			ctx_i = AbPicDecContext(&decoder);
			if(ctx_i<0 || ctx_i>=AB_PIC_NCTX)
			{
				free(row);
				return -1;
			}
			rc = AbPicDecUpdateX(&decoder,bs,&errmodel->ctx[ctx_i]);
			if(rc!=0)
			{
				free(row);
				return rc;
			}
			ref_x = (col-1<0) ? 0 : row[col-1];
			row[col] = (BYTE)AbPicDecProduce(&decoder,ref_x);
		}
	}
	memcpy(p->prevrow,row,rowsz);
	memcpy(row_out,row,p->width);
	free(row);
	return 0;
}

static int AbPicRead(AbPic* p, AbBs* bs, unsigned long want,
	BYTE** chunk, unsigned long* n, int* switched, AbMode* nm)
{
	BYTE* out;
	unsigned long cap = want;
	unsigned long len = 0;
	int rc;
	memset(nm,0,sizeof(*nm));
	*chunk = NULL;
	*n = 0;
	*switched = 0;
	if(want==0)
		return 0;
	out = (BYTE*)malloc(cap ? cap : 1);
	if(!out)
		return -1;
	if(p->leftover_len>0)
	{
		unsigned long k = p->leftover_len<want ? p->leftover_len : want;
		memcpy(out,p->leftover,k);
		len = k;
		if(k<p->leftover_len)
			memmove(p->leftover,p->leftover+k,p->leftover_len-k);
		p->leftover_len -= k;
	}
	while(len<want)
	{
		DWORD bit = AbBsRead(bs,1);
		BYTE* row;
		unsigned long k;
		if(bit==AB_EOF32)
		{
			free(out);
			return -1;
		}
		if(bit==0)
		{
			rc = AbModeRead(bs,nm);
			if(rc!=0)
			{
				free(out);
				return rc;
			}
			*switched = 1;
			break;
		}
		row = (BYTE*)malloc(p->width);
		if(!row)
		{
			free(out);
			return -1;
		}
		rc = AbPicRow(p,bs,row);
		if(rc!=0)
		{
			free(row);
			free(out);
			return rc;
		}
		k = want-len;
		if(k>(unsigned long)p->width)
			k = (unsigned long)p->width;
		memcpy(out+len,row,k);
		len += k;
		if(k<(unsigned long)p->width)
		{
			unsigned long left = (unsigned long)p->width-k;
			if(p->leftover_cap<left)
			{
				BYTE* nl = (BYTE*)realloc(p->leftover,left);
				if(!nl)
				{
					free(row);
					free(out);
					return -1;
				}
				p->leftover = nl;
				p->leftover_cap = left;
			}
			memcpy(p->leftover,row+k,left);
			p->leftover_len = left;
		}
		free(row);
	}
	*chunk = out;
	*n = len;
	return 0;
}

/*** blocked-mode engine ***/
struct AceBlock
{
	AbBs bs;
	AbLz lz;
	AbSound sound;
	AbPic pic;
	AbMode mode;
	AbMode next_mode;
	BYTE exe_leftover[8];
	unsigned exe_leftover_n;
	int last_delta;
	int mode_inited_sound;
	int mode_inited_pic;
	unsigned long filesize;
	unsigned long produced;
	BYTE* stage;
	unsigned long stage_len;
	unsigned long stage_cap;
	int err;
};

static int AceStageAppend(AceBlock* b, const BYTE* src, unsigned long n)
{
	if(n==0)
		return 0;
	if(b->stage_len+n>b->stage_cap)
	{
		unsigned long nc = b->stage_cap ? b->stage_cap*2 : 65536;
		BYTE* nd;
		while(nc<b->stage_len+n)
			nc *= 2;
		nd = (BYTE*)realloc(b->stage,nc);
		if(!nd)
			return -1;
		b->stage = nd;
		b->stage_cap = nc;
	}
	memcpy(b->stage+b->stage_len,src,n);
	b->stage_len += n;
	return 0;
}

static int AceExePatch(BYTE* outchunk, unsigned long n, unsigned long produced,
	unsigned exe_mode, BYTE* leftover, unsigned* leftover_n, unsigned long* new_n)
{
	unsigned long i = 0;
	unsigned long cut = n;
	while(i+4<n)
	{
		if(outchunk[i]==0xE8)
		{
			unsigned long pos = produced+i;
			if(exe_mode==0)
			{
				WORD rel16 = (WORD)(outchunk[i+1]|(outchunk[i+2]<<8));
				rel16 = (WORD)(rel16-(WORD)pos);
				outchunk[i+1] = (BYTE)(rel16 & 0xFF);
				outchunk[i+2] = (BYTE)((rel16>>8) & 0xFF);
				i += 3;
			}
			else
			{
				DWORD rel32 = (DWORD)outchunk[i+1]
					|((DWORD)outchunk[i+2]<<8)
					|((DWORD)outchunk[i+3]<<16)
					|((DWORD)outchunk[i+4]<<24);
				rel32 = rel32-(DWORD)pos;
				outchunk[i+1] = (BYTE)(rel32 & 0xFF);
				outchunk[i+2] = (BYTE)((rel32>>8) & 0xFF);
				outchunk[i+3] = (BYTE)((rel32>>16) & 0xFF);
				outchunk[i+4] = (BYTE)((rel32>>24) & 0xFF);
				i += 5;
			}
		}
		else if(outchunk[i]==0xE9)
		{
			unsigned long pos = produced+i;
			WORD rel16 = (WORD)(outchunk[i+1]|(outchunk[i+2]<<8));
			rel16 = (WORD)(rel16-(WORD)pos);
			outchunk[i+1] = (BYTE)(rel16 & 0xFF);
			outchunk[i+2] = (BYTE)((rel16>>8) & 0xFF);
			i += 3;
		}
		else
			i++;
	}
	while(i<n)
	{
		if(outchunk[i]==0xE8 || outchunk[i]==0xE9)
		{
			unsigned k;
			cut = i;
			*leftover_n = (unsigned)(n-i);
			for(k=0;k<*leftover_n;k++)
				leftover[k] = outchunk[i+k];
			break;
		}
		i++;
	}
	*new_n = cut;
	return 0;
}

// Run one engine iteration, appending output to the stage. Returns 0 ok
// (possibly zero new bytes at a mode switch), -1 on corrupt input.
static int AceStep(AceBlock* b)
{
	int rc;
	if(b->next_mode.present)
	{
		if(b->mode.mode!=b->next_mode.mode)
		{
			if(b->next_mode.mode>=AB_MODE_S8 && b->next_mode.mode<=AB_MODE_S32B)
			{
				unsigned m = b->next_mode.mode;
				unsigned ci;
				memset(&b->sound,0,sizeof(b->sound));
				b->sound.mode_idx = m-AB_MODE_S8;
				b->sound.num_channels = AbSndNumCh[b->sound.mode_idx];
				b->sound.sr.num_models = b->sound.num_channels*3;
				for(ci=0;ci<b->sound.num_channels;ci++)
				{
					b->sound.ch[ci].sr = &b->sound.sr;
					b->sound.ch[ci].model_base_idx = 3*ci;
				}
				b->mode_inited_sound = 1;
			}
			else if(b->next_mode.mode==AB_MODE_PIC)
			{
				rc = AbPicReinit(&b->pic,&b->bs);
				if(rc!=0)
					return rc;
				b->mode_inited_pic = 1;
			}
		}
		b->mode = b->next_mode;
		memset(&b->next_mode,0,sizeof(b->next_mode));
	}
	if(b->mode.mode==AB_MODE_DELTA)
	{
		BYTE* delta = NULL;
		unsigned long delta_n = 0;
		unsigned long i;
		unsigned plane, plane_pos, plane_size;
		while(delta_n<b->mode.delta_len)
		{
			BYTE* chunk = NULL;
			unsigned long n = 0;
			AbMode nm;
			int switched = 0;
			rc = AbLzRead(&b->lz,&b->bs,b->mode.delta_len-delta_n,
				&chunk,&n,&switched,&nm);
			if(rc!=0)
			{
				free(delta);
				free(chunk);
				return rc;
			}
			if(n)
			{
				BYTE* nd = (BYTE*)realloc(delta,delta_n+n);
				if(!nd)
				{
					free(delta);
					free(chunk);
					return -1;
				}
				delta = nd;
				memcpy(delta+delta_n,chunk,n);
				delta_n += n;
			}
			free(chunk);
			if(switched)
			{
				if(b->next_mode.present)
				{
					free(delta);
					return -1;
				}
				b->next_mode = nm;
				if(delta_n==0)
					break;
			}
			if(delta_n>=b->mode.delta_len)
				break;
			if(n==0)
				break;
		}
		if(delta_n==0 && b->next_mode.present)
		{
			free(delta);
			return 0;
		}
		if(b->mode.delta_dist==0)
		{
			free(delta);
			return -1;
		}
		{
			int last = b->last_delta;
			for(i=0;i<delta_n;i++)
			{
				delta[i] = (BYTE)(delta[i]+last);
				last = delta[i];
			}
			b->last_delta = last;
		}
		{
			BYTE* outchunk = (BYTE*)malloc(delta_n ? delta_n : 1);
			unsigned long out_n = 0;
			if(!outchunk)
			{
				free(delta);
				return -1;
			}
			plane = 0;
			plane_pos = 0;
			plane_size = b->mode.delta_len/b->mode.delta_dist;
			out_n = 0;
			while(plane_pos<plane_size)
			{
				while(plane<b->mode.delta_len)
				{
					unsigned long idx = (unsigned long)plane+plane_pos;
					if(idx<delta_n)
						outchunk[out_n++] = delta[idx];
					plane += plane_size;
				}
				plane = 0;
				plane_pos++;
			}
			free(delta);
			rc = AceStageAppend(b,outchunk,out_n);
			free(outchunk);
			if(rc!=0)
				return rc;
		}
	}
	else if(b->mode.mode==AB_MODE_LZ77 || b->mode.mode==AB_MODE_EXE)
	{
		BYTE* chunk = NULL;
		unsigned long n = 0;
		unsigned long need;
		AbMode nm;
		int switched = 0;
		if(b->exe_leftover_n)
		{
			if(AceStageAppend(b,b->exe_leftover,b->exe_leftover_n)!=0)
				return -1;
			b->exe_leftover_n = 0;
		}
		need = b->filesize-b->produced;
		rc = AbLzRead(&b->lz,&b->bs,need,&chunk,&n,&switched,&nm);
		if(rc!=0)
		{
			free(chunk);
			return rc;
		}
		if(n)
		{
			if(AceStageAppend(b,chunk,n)!=0)
			{
				free(chunk);
				return -1;
			}
		}
		free(chunk);
		if(switched)
			b->next_mode = nm;
		if(b->mode.mode==AB_MODE_EXE)
		{
			unsigned long base = b->stage_len-n;
			unsigned long new_n = n;
			unsigned ln = 0;
			BYTE* tail = b->stage+base;
			AceExePatch(tail,n,b->produced,b->mode.exe_mode,
				b->exe_leftover,&ln,&new_n);
			b->exe_leftover_n = ln;
			b->stage_len = base+new_n;
		}
	}
	else if(b->mode.mode>=AB_MODE_S8 && b->mode.mode<=AB_MODE_S32B)
	{
		BYTE* chunk = NULL;
		unsigned long n = 0;
		AbMode nm;
		int switched = 0;
		if(!b->mode_inited_sound)
			b->mode_inited_sound = 1;
		rc = AbSoundRead(&b->sound,&b->bs,b->filesize-b->produced,
			&chunk,&n,&switched,&nm);
		if(rc!=0)
			return rc;
		if(n)
		{
			AbDict* d = &b->lz.dict;
			if(AbDictReserve(d,n)!=0)
			{
				free(chunk);
				return -1;
			}
			memcpy(d->data+d->len,chunk,n);
			d->len += n;
			AbDictTruncate(d);
			if(AceStageAppend(b,chunk,n)!=0)
			{
				free(chunk);
				return -1;
			}
		}
		free(chunk);
		if(switched)
			b->next_mode = nm;
	}
	else if(b->mode.mode==AB_MODE_PIC)
	{
		BYTE* chunk = NULL;
		unsigned long n = 0;
		AbMode nm;
		int switched = 0;
		if(!b->mode_inited_pic)
			b->mode_inited_pic = 1;
		rc = AbPicRead(&b->pic,&b->bs,b->filesize-b->produced,
			&chunk,&n,&switched,&nm);
		if(rc!=0)
			return rc;
		if(n)
		{
			AbDict* d = &b->lz.dict;
			if(AbDictReserve(d,n)!=0)
			{
				free(chunk);
				return -1;
			}
			memcpy(d->data+d->len,chunk,n);
			d->len += n;
			AbDictTruncate(d);
			if(AceStageAppend(b,chunk,n)!=0)
			{
				free(chunk);
				return -1;
			}
		}
		free(chunk);
		if(switched)
			b->next_mode = nm;
	}
	else
		return -1;
	return 0;
}

AceBlock* AceBlkCreate(void)
{
	AceBlock* b = (AceBlock*)calloc(1,sizeof(AceBlock));
	return b;
}

void AceBlkFree(AceBlock* b)
{
	if(!b)
		return;
	free(b->lz.dict.data);
	free(b->pic.prevrow);
	free(b->pic.leftover);
	free(b->stage);
	free(b);
}

void AceBlkBegin(AceBlock* b, unsigned long dicsize, unsigned long filesize,
	int continue_dict)
{
	if(!b)
		return;
	if(dicsize<AB_MINDIC)
		dicsize = AB_MINDIC;
	if(dicsize>AB_MAXDIC)
		dicsize = AB_MAXDIC;
	if(!continue_dict || b->lz.dict.dicsize==0)
	{
		free(b->lz.dict.data);
		b->lz.dict.data = NULL;
		b->lz.dict.len = 0;
		b->lz.dict.cap = 0;
		b->lz.dict.dicsize = dicsize;
		memset(&b->lz.sr,0,sizeof(b->lz.sr));
		memset(&b->lz.dh,0,sizeof(b->lz.dh));
	}
	else
	{
		if(dicsize>b->lz.dict.dicsize)
			b->lz.dict.dicsize = dicsize;
		memset(&b->lz.sr,0,sizeof(b->lz.sr));
		memset(&b->lz.dh,0,sizeof(b->lz.dh));
	}
	memset(&b->bs,0,sizeof(b->bs));
	memset(&b->mode,0,sizeof(b->mode));
	memset(&b->next_mode,0,sizeof(b->next_mode));
	b->mode.present = 1;
	b->mode.mode = AB_MODE_LZ77;
	b->exe_leftover_n = 0;
	b->last_delta = 0;
	b->mode_inited_sound = 0;
	b->mode_inited_pic = 0;
	b->filesize = filesize;
	b->produced = 0;
	b->stage_len = 0;
	b->err = 0;
}

void AceBlkRegister(AceBlock* b, const BYTE* buf, int len)
{
	AbDict* d;
	if(!b || !buf || len<=0)
		return;
	d = &b->lz.dict;
	if(d->dicsize==0)
		d->dicsize = AB_MINDIC;
	if(AbDictReserve(d,(unsigned long)len)!=0)
		return;
	memcpy(d->data+d->len,buf,len);
	d->len += len;
	AbDictTruncate(d);
}

int AceBlkRead(AceBlock* b, AcePullCb pull, void* ctx, BYTE* out, int len)
{
	unsigned long total = 0;
	if(!b || !out || len<=0)
		return 0;
	if(!b->bs.pull)
	{
		b->bs.pull = pull;
		b->bs.ctx = ctx;
	}
	while((int)total<len)
	{
		unsigned long take;
		if(b->err)
			break;
		if(b->produced>=b->filesize)
			break;
		if(b->stage_len==0)
		{
			if(AceStep(b)!=0)
			{
				b->err = 1;
				break;
			}
			if(b->stage_len==0 && !b->next_mode.present)
				break;
		}
		take = b->stage_len;
		if(take>(unsigned long)((int)len-(int)total))
			take = (unsigned long)((int)len-(int)total);
		if(b->produced+take>b->filesize)
			take = b->filesize-b->produced;
		memcpy(out+total,b->stage,take);
		if(take<b->stage_len)
			memmove(b->stage,b->stage+take,b->stage_len-take);
		b->stage_len -= take;
		b->produced += take;
		total += take;
	}
	return (int)total;
}
