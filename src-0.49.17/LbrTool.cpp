
#include "stdafx.h"
#include "LbrTool.h"

#define LBR_SECTOR 128
#define LBR_ENTRY 32

// CCITT/XMODEM CRC-16 (poly 0x1021, init 0). LBR stores the CRC so that
// member bytes (including short-sector pad) followed by the CRC itself
// high-byte-first yield zero.
static unsigned short LbrCrcUpdate(unsigned short crc, unsigned char b)
{
	int i;
	crc ^= (unsigned short)(b<<8);
	for(i=0;i<8;i++)
	{
		if(crc & 0x8000)
			crc = (crc<<1) ^ 0x1021;
		else
			crc <<= 1;
	}
	return (unsigned short)(crc & 0xFFFF);
}

bool CLbrTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<128 || fsize%LBR_SECTOR!=0 )
		return false;
	// directory control entry (fatal rule): active, blank name,
	// index 0, non-zero length
	if( hdr[0]!=0 )
		return false;
	{
		int i;
		for(i=1;i<=11;i++)
			if( hdr[i]!=0x20 )
				return false;
	}
	if( hdr[12]!=0 || hdr[13]!=0 )
		return false;
	{
		unsigned dirSectors = (hdr[14])|(hdr[15]<<8);
		if( dirSectors==0
			|| (unsigned long)dirSectors*LBR_SECTOR>(unsigned long)fsize
			|| (unsigned long)dirSectors*LBR_SECTOR>(unsigned long)siz )
			return false;
	}
	return true;
}

struct LbrSqNode
{
	short child[2];
};
#define LBR_SPEOF 256
#define LBR_NUMVALS 257

// ---- CP/M Crunch v2.x decoder (adaptive LZW + RLE90) ----
// Container: 76 FE, NUL-terminated name, 4 bytes (ref/sig/err/spare),
// MSB-first variable codes (9-12 bits), EOF code, zero-pad to byte,
// LE16 checksum over final output bytes (only if errdetect != 0).
// V2 only (siglevel >= 0x20); V1 (fixed 12-bit hash slots) unsupported.
#define CR_EOF 256
#define CR_RESET 257
#define CR_MAXBITS 12
#define CR_TABSIZE 4096
#define CR_HASH 5003
#define CR_HASHOV 0x200
#define CR_EMPTY 0x80
#define CR_USED 0x20
#define CR_ROOT 0x80
#define CR_FILLING 0
#define CR_LASTPASS 0xFE
#define CR_REPLACING 0xFF

struct CrTable
{
	BYTE flags[CR_TABSIZE];
	BYTE prefix[CR_TABSIZE];
	BYTE suffix[CR_TABSIZE];
	BYTE hashHi[CR_HASH+CR_HASHOV];
	BYTE hashLo[CR_HASH+CR_HASHOV];
	int nextCode;
	int codeSize;
	int threshold;
	int state;
};

static void CrClear(CrTable* t)
{
	memset(t->flags,CR_EMPTY,CR_TABSIZE);
	memset(t->prefix,0,CR_TABSIZE);
	memset(t->suffix,0,CR_TABSIZE);
	memset(t->hashHi,CR_EMPTY,CR_HASH+CR_HASHOV);
	memset(t->hashLo,0,CR_HASH+CR_HASHOV);
	t->hashHi[0] = 0x7F;// slot 0 never reads as empty
	t->nextCode = 0;
	t->codeSize = 9;
	t->threshold = 1<<(9-8);
	t->state = CR_FILLING;
}

static void CrHashAddr(int prefix, int suffix, unsigned* addr, unsigned* stride)
{
	unsigned shifted = ((unsigned)prefix<<4) & 0xFFFF;
	unsigned lo = (unsigned)suffix ^ (shifted>>8);
	unsigned hi = (0x60 + ((unsigned)prefix & 0x0F)) & 0xFF;
	unsigned a = (((hi<<8)|lo) + 1) & 0xFFFF;
	*addr = a;
	*stride = (a + 0x8C75) & 0xFFFF;
}

static unsigned CrProbe(unsigned addr, unsigned stride)
{
	addr = (addr + stride) & 0xFFFF;
	if((addr>>8) < 0x60)
		addr = (addr + CR_HASH) & 0xFFFF;
	return addr;
}

// slot index, or -1 when the address walked out of the table
static int CrSlot(unsigned addr)
{
	if(addr < 0x6000 || addr >= 0x6000+CR_HASH+CR_HASHOV)
		return -1;
	return (int)(addr - 0x6000);
}

static void CrHashInsert(CrTable* t, int prefix, int suffix)
{
	unsigned addr, stride, k;
	CrHashAddr(prefix,suffix,&addr,&stride);
	for(k=0;k<CR_HASH;k++)
	{
		int slot = CrSlot(addr);
		if(slot<0)
			return;
		if(t->hashHi[slot]==CR_EMPTY)
		{
			t->hashHi[slot] = (BYTE)((t->nextCode>>8)&0xFF);
			t->hashLo[slot] = (BYTE)(t->nextCode & 0xFF);
			return;
		}
		addr = CrProbe(addr,stride);
	}
}

static void CrInsert(CrTable* t, int prefix, int suffix, int flag)
{
	CrHashInsert(t,prefix,suffix);
	{
		int code = t->nextCode;
		if(code < CR_TABSIZE)
		{
			t->flags[code] = (BYTE)(flag | ((prefix>>8)&0xFF));
			t->prefix[code] = (BYTE)(prefix & 0xFF);
			t->suffix[code] = (BYTE)suffix;
		}
		t->nextCode = code+1;
		if( (((t->nextCode+1)>>8)&0xFF) == t->threshold )
		{
			t->threshold = (t->threshold<<1)&0xFF;
			if(t->codeSize+1 > CR_MAXBITS)
				t->state = CR_LASTPASS;
			else
				t->codeSize++;
		}
	}
}

static void CrSeed(CrTable* t)
{
	int b;
	CrClear(t);
	for(b=0;b<256;b++)
		CrInsert(t,0xFFFF,b,CR_USED);
	for(b=0;b<4;b++)
		CrInsert(t,0x7FFF,0,CR_USED);
}

static void CrReplace(CrTable* t, int prefix, int suffix)
{
	unsigned addr, stride, k;
	CrHashAddr(prefix,suffix,&addr,&stride);
	for(k=0;k<CR_HASH;k++)
	{
		int slot = CrSlot(addr);
		int victim;
		if(slot<0)
			return;
		if(t->hashHi[slot]==CR_EMPTY)
			return;
		victim = (t->hashHi[slot]<<8) | t->hashLo[slot];
		if(victim < CR_TABSIZE && !(t->flags[victim] & CR_USED))
		{
			t->flags[victim] = (BYTE)((prefix>>8)&0xFF);
			t->prefix[victim] = (BYTE)(prefix & 0xFF);
			t->suffix[victim] = (BYTE)suffix;
			return;
		}
		addr = CrProbe(addr,stride);
	}
}

struct CrBits
{
	FILE* fp;
	long left;// member bytes remaining
	unsigned buf;
	int have;
};

static int CrGetc(CrBits* b)
{
	int c;
	if(b->left<=0)
		return -1;
	c = fgetc(b->fp);
	if(c<0)
		return -1;
	b->left--;
	return c;
}

// n-bit MSB-first code; CR_EOF when the member data runs out
static int CrRead(CrBits* b, int n)
{
	while(b->have < n)
	{
		int c = CrGetc(b);
		if(c<0)
			return CR_EOF;
		b->buf = (b->buf<<8) | (unsigned)c;
		b->have += 8;
	}
	{
		int code = (b->buf >> (b->have - n)) & ((1<<n)-1);
		b->have -= n;
		if(b->have>0)
			b->buf &= (1<<b->have)-1;
		else
			b->buf = 0;
		return code;
	}
}

struct CrOut
{
	FILE* fp;
	int prev;
	int esc;
	unsigned sum;
	int tap;
};

static void CrPutc(CrOut* o, int b)
{
	int k;
	if(o->esc)
	{
		o->esc = 0;
		if(b==0)
		{
			fputc(0x90,o->fp);
			if(o->tap) o->sum = (o->sum+0x90)&0xFFFF;
		}
		else
		{
			for(k=0;k<b-1;k++)
			{
				fputc(o->prev,o->fp);
				if(o->tap) o->sum = (o->sum+o->prev)&0xFFFF;
			}
		}
		return;
	}
	if(b==0x90)
	{
		o->esc = 1;
		return;
	}
	fputc(b,o->fp);
	if(o->tap) o->sum = (o->sum+b)&0xFFFF;
	o->prev = b;
}

// Emit one code's string through the RLE layer. Returns 0 on corrupt input.
static int CrEmit(CrTable* t, int code, int prev, int* firstByte, int* skip, CrOut* o)
{
	int stack[CR_TABSIZE+2];
	int sp = 0;
	for(;;)
	{
		int fl = t->flags[code];
		if( (fl & ~CR_USED & 0xFF) == CR_EMPTY )
		{
			*skip = 1;
			CrInsert(t,prev,*firstByte,CR_USED);
			fl = t->flags[code];
			if( (fl & ~CR_USED & 0xFF) == CR_EMPTY )
				return 0;
		}
		if(sp > CR_TABSIZE)
			return 0;
		stack[sp++] = t->suffix[code];
		if( fl & CR_ROOT )
		{
			*firstByte = t->suffix[code];
			break;
		}
		code = ((fl & ~CR_USED & 0xFF)<<8) | t->prefix[code];
		if(code >= CR_TABSIZE)
			return 0;
	}
	while(sp>0)
		CrPutc(o,stack[--sp]);
	return 1;
}

// Decode one crunched (76 FE, v2.x) member to `out`.
// `start` points at the member's first byte, `datasize` bounds the reads.
static bool UnCrunchMember(FILE* lbr, long start, long datasize, FILE* out)
{
	CrBits br;
	CrTable t;
	CrOut o;
	int prev, first, skip, replacing;
	long pos;

	fseek(lbr,start,SEEK_SET);
	if(fgetc(lbr)!=0x76 || fgetc(lbr)!=0xFE)
		return false;
	pos = 2;
	// NUL-terminated name (cap: must end inside the member)
	for(;;)
	{
		int c;
		if(pos >= datasize)
			return false;
		c = fgetc(lbr);
		pos++;
		if(c<0)
			return false;
		if(c==0)
			break;
		if(pos-2 > 128)
			return false;
	}
	// ref/sig/err/spare
	if(pos+4 > datasize)
		return false;
	fgetc(lbr);// reflevel (reference only)
	{
		int sig = fgetc(lbr);
		if(sig<0x20)
			return false;// v1 (fixed 12-bit hash slots) unsupported
	}
	int err = fgetc(lbr);
	fgetc(lbr);// spare
	pos += 4;

	br.fp = lbr; br.left = datasize-pos; br.buf = 0; br.have = 0;
	CrSeed(&t);
	o.fp = out; o.prev = 0; o.esc = 0; o.sum = 0; o.tap = (err>0);
	prev = 0xFFFF; first = 0; skip = 1; replacing = 0;
	for(;;)
	{
		int code = CrRead(&br,t.codeSize);
		while(code==258 || code==259)
			code = CrRead(&br,t.codeSize);
		if(code==CR_EOF)
			break;
		if(code==CR_RESET)
		{
			CrSeed(&t);
			prev = 0xFFFF; skip = 1; replacing = 0;
			continue;
		}
		if(code<0 || code>=CR_TABSIZE)
			return false;
		t.flags[code] |= CR_USED;
		if(!CrEmit(&t,code,prev,&first,&skip,&o))
			return false;
		if(replacing)
			CrReplace(&t,prev,first);
		else
		{
			int was = skip;
			skip = 0;
			if(!was)
				CrInsert(&t,prev,first,0);
			if(t.state==CR_LASTPASS)
				t.state = CR_REPLACING;
			else if(t.state==CR_REPLACING)
				replacing = 1;
		}
		prev = code;
	}
	if(o.esc)
		o.esc = 0;// dangling escape is dropped
	if(err>0)
	{
		// checksum follows the EOF zero-padding (already byte-aligned:
		// the bit reader only ever consumed whole bytes)
		int lo, hi;
		if(br.left<2)
			return false;
		lo = fgetc(lbr); hi = fgetc(lbr);
		if(lo<0 || hi<0)
			return false;
		if(o.sum != (unsigned)(lo|(hi<<8)))
			return false;
	}
	return true;
}

// Decode one squeezed (SQ, magic 76 FF) member to `out`.
// `start` points at the member's first byte. Returns true on clean EOF.
static bool UnSqueezeMember(FILE* lbr, long start, FILE* out)
{
	LbrSqNode node[LBR_NUMVALS];
	int bpos = 99, curin = 0;
	int numnodes, i, c;

	fseek(lbr,start,SEEK_SET);
	if( fgetc(lbr)!=0x76 || fgetc(lbr)!=0xFF )
		return false;
	fgetc(lbr); fgetc(lbr);// SQ checksum (LBR CRC guards integrity)
	// skip NUL-terminated embedded name
	for(i=0;i<256;i++)
	{
		c = fgetc(lbr);
		if(c<0)
			return false;
		if(c==0)
			break;
	}
	c = fgetc(lbr);
	{
		int d = fgetc(lbr);
		if(c<0 || d<0)
			return false;
		numnodes = c|(d<<8);
	}
	if( numnodes<=0 || numnodes>=LBR_NUMVALS )
		return false;
	// tree pairs, low byte first (sequenced reads; MSVC may reorder
	// unsequenced myfgetc() pairs under /O2)
	for(i=0;i<numnodes;i++)
	{
		int lo, hi;
		lo = fgetc(lbr); hi = fgetc(lbr);
		if(lo<0 || hi<0)
			return false;
		node[i].child[0] = (short)(lo|(hi<<8));
		lo = fgetc(lbr); hi = fgetc(lbr);
		if(lo<0 || hi<0)
			return false;
		node[i].child[1] = (short)(lo|(hi<<8));
	}
	// bit stream, LSB first, raw bytes out (no NCR layer in SQ)
	for(;;)
	{
		for(i=0;i>=0;)
		{
			if( ++bpos>7 )
			{
				curin = fgetc(lbr);
				if(curin<0)
					return false;
				bpos = 0;
				i = node[i].child[1&curin];
			}
			else
				i = node[i].child[1&(curin>>=1)];
		}
		i = -(i+1);
		if(i==LBR_SPEOF)
			break;
		fputc(i,out);
	}
	return true;
}

// ---- CP/M CrLZH decoder (LZSS + adaptive Huffman, LZHUF lineage) ----
// Container: 76 FD, NUL-terminated name, 4 bytes (used/required/err/spare),
// MSB-first codes, stop symbol 256, LE16 byte-sum over the output bytes.
// errdetect must be 0 (01 = CRC16 with unknown parameters, else reserved).
// used >= 0x21 is refused (UCRLZH20 refuses); used >= 0x20 selects the
// v2 position code, below it the v1 code. No RLE layer: output is final.
#define LZH_N 2048
#define LZH_F 60
#define LZH_NCHAR 315
#define LZH_T (LZH_NCHAR*2-1)
#define LZH_R (LZH_T-1)
#define LZH_MAXFREQ 0x8000
#define LZH_STOP 256

static const BYTE LzhDCode[256] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
	0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
	0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
	0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
	0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
	0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
	0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
	0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,
	0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,
	0x0C,0x0C,0x0C,0x0C,0x0D,0x0D,0x0D,0x0D,
	0x0E,0x0E,0x0E,0x0E,0x0F,0x0F,0x0F,0x0F,
	0x10,0x10,0x10,0x10,0x11,0x11,0x11,0x11,
	0x12,0x12,0x12,0x12,0x13,0x13,0x13,0x13,
	0x14,0x14,0x14,0x14,0x15,0x15,0x15,0x15,
	0x16,0x16,0x16,0x16,0x17,0x17,0x17,0x17,
	0x18,0x18,0x19,0x19,0x1A,0x1A,0x1B,0x1B,
	0x1C,0x1C,0x1D,0x1D,0x1E,0x1E,0x1F,0x1F,
	0x20,0x20,0x21,0x21,0x22,0x22,0x23,0x23,
	0x24,0x24,0x25,0x25,0x26,0x26,0x27,0x27,
	0x28,0x28,0x29,0x29,0x2A,0x2A,0x2B,0x2B,
	0x2C,0x2C,0x2D,0x2D,0x2E,0x2E,0x2F,0x2F,
	0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
	0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F
};

static const BYTE LzhDLen[256] = {
	0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
	0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
	0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
	0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
	0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
	0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
	0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
	0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
	0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
	0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
	0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
	0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
	0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08
};

struct LzhTree
{
	int freq[LZH_T+1];
	int prnt[LZH_T+LZH_NCHAR];
	int son[LZH_T];
};

static void LzhInit(LzhTree* t)
{
	int i, j;
	for(i=0;i<LZH_NCHAR;i++)
	{
		t->freq[i] = 1;
		t->son[i] = i+LZH_T;
		t->prnt[i+LZH_T] = i;
	}
	i = 0; j = LZH_NCHAR;
	while(j <= LZH_R)
	{
		t->freq[j] = t->freq[i]+t->freq[i+1];
		t->son[j] = i;
		t->prnt[i] = t->prnt[i+1] = j;
		i += 2; j++;
	}
	t->freq[LZH_T] = 0xFFFF;
	t->prnt[LZH_R] = 0;
}

static void LzhReconst(LzhTree* t)
{
	int i, j, k, l, f;
	j = 0;
	for(i=0;i<LZH_T;i++)
	{
		if(t->son[i] >= LZH_T)
		{
			t->freq[j] = (t->freq[i]+1)/2;
			t->son[j] = t->son[i];
			j++;
		}
	}
	i = 0; j = LZH_NCHAR;
	while(j < LZH_T)
	{
		k = i+1;
		f = t->freq[i]+t->freq[k];
		t->freq[j] = f;
		k = j-1;
		while(f < t->freq[k])
			k--;
		k++;
		for(l=j;l>k;l--)
		{
			t->freq[l] = t->freq[l-1];
			t->son[l] = t->son[l-1];
		}
		t->freq[k] = f;
		t->son[k] = i;
		i += 2; j++;
	}
	for(i=0;i<LZH_T;i++)
	{
		k = t->son[i];
		if(k >= LZH_T)
			t->prnt[k] = i;
		else
		{
			t->prnt[k] = i;
			t->prnt[k+1] = i;
		}
	}
}

static void LzhUpdate(LzhTree* t, int c)
{
	int k, l, i, j;
	if(t->freq[LZH_R] == LZH_MAXFREQ)
		LzhReconst(t);
	c = t->prnt[c+LZH_T];
	for(;;)
	{
		t->freq[c]++;
		k = t->freq[c];
		l = c+1;
		if(k > t->freq[l])
		{
			while(k > t->freq[l+1])
				l++;
			t->freq[c] = t->freq[l];
			t->freq[l] = k;
			i = t->son[c];
			t->prnt[i] = l;
			if(i < LZH_T)
				t->prnt[i+1] = l;
			j = t->son[l];
			t->son[l] = i;
			t->prnt[j] = c;
			if(j < LZH_T)
				t->prnt[j+1] = c;
			t->son[c] = j;
			c = l;
		}
		c = t->prnt[c];
		if(c==0)
			break;
	}
}

struct LzhBits
{
	FILE* fp;
	long left;
	unsigned buf;
	int have;
};

static int LzhGetc(LzhBits* b)
{
	int c;
	if(b->left<=0)
		return -1;
	c = fgetc(b->fp);
	if(c<0)
		return -1;
	b->left--;
	return c;
}

// n-bit MSB-first code; -1 when the member data runs out
static int LzhRead(LzhBits* b, int n)
{
	while(b->have < n)
	{
		int c = LzhGetc(b);
		if(c<0)
			return -1;
		b->buf = (b->buf<<8) | (unsigned)c;
		b->have += 8;
	}
	{
		int code = (b->buf >> (b->have - n)) & ((1<<n)-1);
		b->have -= n;
		if(b->have>0)
			b->buf &= (1<<(unsigned)b->have)-1;
		else
			b->buf = 0;
		return code;
	}
}

static int LzhDecodeChar(LzhTree* t, LzhBits* b)
{
	int c = t->son[LZH_R];
	while(c < LZH_T)
	{
		int bit = LzhRead(b,1);
		if(bit<0)
			return -1;
		c += bit;
		c = t->son[c];
	}
	c -= LZH_T;
	LzhUpdate(t,c);
	return c;
}

static int LzhPosition(LzhBits* b, int v2)
{
	int i = LzhRead(b,8);
	if(i<0)
		return -1;
	{
		int dc = LzhDCode[i];
		int extra = LzhDLen[i] - (v2 ? 3 : 2);
		int accum = i;
		while(extra>0)
		{
			int bit = LzhRead(b,1);
			if(bit<0)
				return -1;
			accum = (accum<<1)|bit;
			extra--;
		}
		if(v2)
			return (dc<<5)|(accum & 0x1F);
		return (dc<<6)|(accum & 0x3F);
	}
}

// Decode one CrLZH (76 FD) member to `out`.
// `start` points at the member's first byte, `datasize` bounds the reads.
static bool UnCrlzhMember(FILE* lbr, long start, long datasize, FILE* out)
{
	LzhBits br;
	LzhTree t;
	BYTE win[LZH_N];
	int r, v1, v2flag;
	unsigned sum;
	long pos, dataoff, pulled, consumed, next;

	fseek(lbr,start,SEEK_SET);
	if(fgetc(lbr)!=0x76 || fgetc(lbr)!=0xFD)
		return false;
	pos = 2;
	// NUL-terminated name (cap: must end inside the member)
	for(;;)
	{
		int c;
		if(pos >= datasize)
			return false;
		c = fgetc(lbr);
		pos++;
		if(c<0)
			return false;
		if(c==0)
			break;
		if(pos-2 > 128)
			return false;
	}
	// used/required/err/spare
	if(pos+4 > datasize)
		return false;
	v1 = fgetc(lbr);
	fgetc(lbr);// required (reference only)
	{
		int err = fgetc(lbr);
		if(err!=0)
			return false;// 01 = CRC16 (unknown), else reserved
	}
	fgetc(lbr);// spare
	pos += 4;
	if(v1>=0x21)
		return false;// UCRLZH20 refuses >= 0x21
	v2flag = (v1>=0x20);

	LzhInit(&t);
	memset(win,0x20,sizeof(win));
	r = LZH_N-LZH_F;
	sum = 0;
	dataoff = start+pos;
	br.fp = lbr; br.left = datasize-pos; br.buf = 0; br.have = 0;
	for(;;)
	{
		int c = LzhDecodeChar(&t,&br);
		if(c<0)
			return false;
		if(c < 256)
		{
			fputc(c,out);
			sum = (sum+(unsigned)c)&0xFFFF;
			win[r] = (BYTE)c;
			r = (r+1)&(LZH_N-1);
		}
		else if(c==LZH_STOP)
			break;
		else
		{
			int len = c-254;
			int p = LzhPosition(&br,v2flag);
			int i, k;
			if(p<0)
				return false;
			i = (r-p-1)&(LZH_N-1);
			for(k=0;k<len;k++)
			{
				int b = win[i];
				fputc(b,out);
				sum = (sum+(unsigned)b)&0xFFFF;
				win[r] = (BYTE)b;
				r = (r+1)&(LZH_N-1);
				i = (i+1)&(LZH_N-1);
			}
		}
	}
	// LE16 byte-sum checksum at the first byte boundary after the codes
	pulled = (datasize-pos)-br.left;
	consumed = pulled*8-br.have;
	if(consumed<=0)
		return false;
	next = dataoff+(consumed-1)/8+1;
	if(next+2 > start+datasize)
		return false;
	fseek(lbr,next,SEEK_SET);
	{
		int lo = fgetc(lbr), hi = fgetc(lbr);
		if(lo<0 || hi<0)
			return false;
		if(sum != (unsigned)(lo|(hi<<8)))
			return false;
	}
	return true;
}


bool CLbrTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	FILE* lbr = fopen( fname,"rb" );
	if( !lbr )
		return false;
	if( LBR_ENTRY!=fread( buf,1,LBR_ENTRY,lbr ) )
	{fclose(lbr);return false;}

	// directory control entry (same fatal rule as Check)
	{
		int i;
		if(buf[0]!=0)
		{fclose(lbr);return false;}
		for(i=1;i<=11;i++)
			if(buf[i]!=0x20)
			{fclose(lbr);return false;}
		if(buf[12]!=0 || buf[13]!=0)
		{fclose(lbr);return false;}
	}
	unsigned dirSectors = (buf[14])|(buf[15]<<8);
	if(dirSectors==0)
	{fclose(lbr);return false;}
	int entries = dirSectors*4;

	for(int e=1;e<entries;e++)
	{
		BYTE rec[LBR_ENTRY];
		fseek(lbr,(long)e*LBR_ENTRY,SEEK_SET);
		if( LBR_ENTRY!=fread(rec,1,LBR_ENTRY,lbr) )
			break;
		if(rec[0]==0xFF)
			break;// unused terminates the directory
		if(rec[0]!=0)
			continue;// deleted
		unsigned index = (rec[12])|(rec[13]<<8);
		unsigned length = (rec[14])|(rec[15]<<8);
		unsigned crc = (rec[16])|(rec[17]<<8);
		unsigned pad = rec[26];
		if(pad>127 || length==0)
			continue;
		unsigned long fullsize = (unsigned long)length*LBR_SECTOR;
		if(fullsize<pad)
			continue;
		unsigned long datasize = fullsize-pad;
		long start = (long)index*LBR_SECTOR;

		char name[9], ext[4], outname[MAX_PATH];
		int i;
		for(i=0;i<8 && rec[1+i]!=0x20;i++)
			name[i] = (char)rec[1+i];
		name[i] = '\0';
		for(i=0;i<3 && rec[9+i]!=0x20;i++)
			ext[i] = (char)rec[9+i];
		ext[i] = '\0';
		if(ext[0])
			wsprintf(outname,"%s.%s",name,ext);
		else
			lstrcpy(outname,name);
		if(outname[0]=='\0')
			continue;

		// member kind (extension flag confirmed by magic)
		int kind = 0;// stored
		if(datasize>=2)
		{
			BYTE magic[2];
			fseek(lbr,start,SEEK_SET);
			if(2==fread(magic,1,2,lbr))
			{
				if((ext[1]=='q' || ext[1]=='Q')
					&& magic[0]==0x76 && magic[1]==0xFF)
					kind = 1;// squeezed
				else if((ext[1]=='z' || ext[1]=='Z')
					&& magic[0]==0x76 && magic[1]==0xFE)
					kind = 2;// crunched (v2.x adaptive LZW)
				else if((ext[1]=='y' || ext[1]=='Y')
					&& magic[0]==0x76 && magic[1]==0xFD)
					kind = 3;// CrLZH (LZSS + adaptive Huffman)
			}
		}

		if (m_Endflg) return true;
		if (!CheckStr(outname))
			continue;

		char* cn;
		FILE* out = fopen( cn=kiutil::pathMake(outname), "wb" );
		if( !out )
			continue;

		// squeezed/crunched/crlzh members carry their CRC
		// over the compressed bytes (verified before decode)
		bool ok = false;
		if(kind==0)
		{
			// stored: copy data bytes, CRC covers full sectors + pad
			unsigned short c = 0;
			unsigned long n;
			ok = true;
			fseek(lbr,start,SEEK_SET);
			for(n=0;n<fullsize;n++)
			{
				int b = fgetc(lbr);
				if(b<0){ ok = false; break; }
				c = LbrCrcUpdate(c,(unsigned char)b);
				if(n<datasize)
					fputc(b,out);
			}
			if(ok)
			{
				c = LbrCrcUpdate(c,(unsigned char)(crc>>8));
				c = LbrCrcUpdate(c,(unsigned char)(crc&0xFF));
				ok = (c==0);
			}
		}
		else
		{
			// squeezed/crunched/crlzh: CRC-verify the compressed bytes first ...
			unsigned short c = 0;
			unsigned long n;
			fseek(lbr,start,SEEK_SET);
			ok = true;
			for(n=0;n<fullsize;n++)
			{
				int b = fgetc(lbr);
				if(b<0){ ok = false; break; }
				c = LbrCrcUpdate(c,(unsigned char)b);
			}
			if(ok)
			{
				c = LbrCrcUpdate(c,(unsigned char)(crc>>8));
				c = LbrCrcUpdate(c,(unsigned char)(crc&0xFF));
				ok = (c==0);
			}
			// ... then decode
			if(ok)
			{
				if(kind==1)
					ok = UnSqueezeMember(lbr,start,out);
				else if(kind==3)
					ok = UnCrlzhMember(lbr,start,(long)fullsize,out);
				else
					ok = UnCrunchMember(lbr,start,(long)fullsize,out);
			}
		}

		fclose(out);
		if(!ok)
			DeleteFile(cn);
	}

	fclose(lbr);
	return true;
}
