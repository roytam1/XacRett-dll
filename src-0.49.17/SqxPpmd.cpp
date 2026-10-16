/* SqxPpmd.cpp -- SQX method 6 (PPMd text) decode glue.
 *
 * Method 6 is PPMd variant I over the SQX carryless range coder. The model
 * core is the tree's vendored ppmd8/ decoder (7-Zip Ppmd8 lineage, public
 * domain -- the same lineage the reference below was ported from), driven
 * here by an SQX front end: the word-swapped bit reader, the SQX range
 * coder (TOP 1<<24 / BOT 1<<16, NOT the 7-Zip byte-stream one), and the
 * cont/order/mem stream header. The symbol loop mirrors the reference
 * decode structure against our core's model API (Update1/Update1_0/
 * Update2/UpdateModel/MakeEscFreq); the range layer is never shared.
 *
 * Reference (TEMP only, never vendored): seb3773/Squeez-c (MIT).
 */
#include "stdafx.h"
#include "SqxPpmd.h"
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "ppmd8/ppmd8.h"
#include "unsqx/cx_type.h"
#include "unsqx/sqx_h.h"
#include "unsqx/sqx_vars.h"
#include "unsqx/arcio.h"
#include "unsqx/cx_err.h"

extern void SetProgress(int iFinished);
}

/* Ppmd8_UpdateModel is global in ppmd8.c but not in ppmd8.h; declare it
   here rather than touching the vendored header (link fails loudly if it
   ever goes away). */
extern "C" void Ppmd8_UpdateModel(CPpmd8 *p);

/* ---- ISzAlloc for Ppmd8 (malloc backed, like ZipPpmd) ---- */
static void *sqxPpmdAlloc(ISzAllocPtr p, size_t size)
	{ (void)p; return malloc(size); }
static void sqxPpmdFree(ISzAllocPtr p, void *addr)
	{ (void)p; free(addr); }
static ISzAlloc g_sqxpAlloc = { sqxPpmdAlloc, sqxPpmdFree };

/* ---- session (solid archives keep the model across members) ---- */
struct SqxPpmdSession
{
	CPpmd8 model;
	int have_model;
	int constructed;
	/* per-member range state (SQX carryless coder, re-inited per member) */
	UInt32 rLow;
	UInt32 rRange;
	UInt32 rCode;
	/* per-member word-swapped bit reader over the member payload */
	const unsigned char *bitBase;
	UInt32 bitLen;
	UInt32 bitWpos;
	UInt32 bitWord;
	unsigned bitBits;
};

static SqxPpmdSession g_sq;

void SqxPpmd_BeginArchive(void)
{
	if( g_sq.constructed )
		Ppmd8_Free(&g_sq.model, &g_sqxpAlloc);
	Ppmd8_Construct(&g_sq.model);
	g_sq.constructed = 1;
	g_sq.have_model = 0;
}

/* ---- word-swapped bit reader (bytes in groups of 4, reversed) ---- */
static UInt32 SqxBitsWord(const SqxPpmdSession *s, UInt32 w)
{
	UInt32 o = w * 4;
	UInt32 v = 0;
	unsigned i;
	for( i = 0; i < 4; i++ )
		if( o + i < s->bitLen )
			v |= (UInt32)s->bitBase[o + i] << (8 * i);
	return v;
}

static void SqxBitsInit(SqxPpmdSession *s, const unsigned char *buf, UInt32 len)
{
	s->bitBase = buf;
	s->bitLen = len;
	s->bitWpos = 0;
	s->bitBits = 0;
	s->bitWord = len ? SqxBitsWord(s, 0) : 0;
}

static UInt32 SqxBitsPeek(const SqxPpmdSession *s, unsigned n)
{
	if( n == 0 )
		return 0;
	return n >= 32 ? s->bitWord : (s->bitWord >> (32 - n));
}

static void SqxBitsDump(SqxPpmdSession *s, unsigned n)
{
	s->bitBits += n;
	s->bitWpos += s->bitBits >> 5;
	s->bitBits &= 31;
	s->bitWord = SqxBitsWord(s, s->bitWpos) << s->bitBits;
	if( s->bitBits )
		s->bitWord |= SqxBitsWord(s, s->bitWpos + 1) >> (32 - s->bitBits);
}

static UInt32 SqxBitsGet(SqxPpmdSession *s, unsigned n)
{
	UInt32 v = SqxBitsPeek(s, n);
	SqxBitsDump(s, n);
	return v;
}

/* ---- SQX carryless range coder (TOP 1<<24 / BOT 1<<16) ---- */
#define SQX_RC_TOP ((UInt32)1 << 24)
#define SQX_RC_BOT ((UInt32)1 << 16)

static void SqxRcNorm(SqxPpmdSession *s)
{
	for( ;; )
	{
		if( (s->rLow ^ (s->rLow + s->rRange)) >= SQX_RC_TOP )
		{
			if( s->rRange >= SQX_RC_BOT )
				break;
			s->rRange = (0UL - s->rLow) & (SQX_RC_BOT - 1UL);
		}
		s->rCode = (s->rCode << 8) | SqxBitsGet(s, 8);
		s->rRange <<= 8;
		s->rLow <<= 8;
	}
}

static UInt32 SqxRcThreshold(SqxPpmdSession *s, UInt32 total)
{
	if( total == 0 )
		return 0xFFFFFFFFUL;
	s->rRange /= total;
	if( s->rRange == 0 )
		return 0xFFFFFFFFUL;
	return (s->rCode - s->rLow) / s->rRange;
}

static void SqxRcDecode(SqxPpmdSession *s, UInt32 start, UInt32 size)
{
	s->rLow += s->rRange * start;
	s->rRange *= size;
}

/* uniform n-bit decode, MSB first */
static UInt32 SqxRcGetBits(SqxPpmdSession *s, unsigned n)
{
	UInt32 v = 0;
	while( n-- )
	{
		UInt32 r = s->rRange >> 1;
		UInt32 q;
		s->rRange = r;
		if( r == 0 )
			return v;
		q = (s->rCode - s->rLow) / r;
		v = (v << 1) | q;
		s->rLow += q * r;
		SqxRcNorm(s);
	}
	return v;
}

/* ---- one-state probability (reference get_bin_summ, with clamps) ---- */
static UInt16 *SqxPpmdBinSumm(CPpmd8 *p)
{
	CPpmd8_Context *mc = p->MinContext;
	unsigned freq = Ppmd8Context_OneState(mc)->Freq;
	unsigned suffix_ns = Ppmd8_GetContext(p, mc->Suffix)->NumStats;
	unsigned row = (unsigned)p->NS2Indx[freq ? freq - 1 : 0];
	unsigned col = p->PrevSuccess
		+ (((UInt32)p->RunLength >> 26) & 0x20U)
		+ (unsigned)p->NS2BSIndx[suffix_ns]
		+ mc->Flags;
	if( row > 24 )
		row = 24;
	if( col > 63 )
		col = 63;
	return &p->BinSumm[row][col];
}

/* ---- symbol loop (mirrors the reference decode structure) ----
   Returns 0..255, or -1 on a corrupt stream. */
static int SqxPpmdDecodeSymbol(SqxPpmdSession *s)
{
	CPpmd8 *p = &s->model;
	Byte mask[256];
	unsigned i;

	if( p->MinContext->NumStats != 0 )
	{
		CPpmd_State *st = Ppmd8_GetStats(p, p->MinContext);
		UInt32 count, hiCnt;
		UInt32 summFreq = p->MinContext->Union2.SummFreq;

		count = SqxRcThreshold(s, summFreq);
		hiCnt = count;
		if( (Int32)(count -= st->Freq) < 0 )
		{
			Byte sym;
			SqxRcDecode(s, 0, st->Freq);
			SqxRcNorm(s);
			p->FoundState = st;
			sym = st->Symbol;
			Ppmd8_Update1_0(p);
			return sym;
		}
		p->PrevSuccess = 0;
		i = p->MinContext->NumStats;
		do {
			st++;
			if( (Int32)(count -= st->Freq) < 0 )
			{
				Byte sym;
				SqxRcDecode(s, (hiCnt - count) - st->Freq, st->Freq);
				SqxRcNorm(s);
				p->FoundState = st;
				sym = st->Symbol;
				Ppmd8_Update1(p);
				return sym;
			}
		} while( --i );
		if( hiCnt >= summFreq )
			return -1;
		hiCnt -= count;
		SqxRcDecode(s, hiCnt, summFreq - hiCnt);
		memset(mask, 0xFF, sizeof(mask));
		{
			CPpmd_State *q = Ppmd8_GetStats(p, p->MinContext);
			mask[st->Symbol] = 0;
			do {
				unsigned sym0 = q[0].Symbol;
				unsigned sym1 = q[1].Symbol;
				q += 2;
				mask[sym0] = 0;
				mask[sym1] = 0;
			} while( q < st );
		}
	}
	else
	{
		CPpmd_State *st = Ppmd8Context_OneState(p->MinContext);
		UInt16 *prob = SqxPpmdBinSumm(p);
		UInt32 pr_old = *prob;
		UInt32 q14 = s->rRange >> 14;
		UInt32 size0 = q14 * pr_old;
		UInt32 pr_new = pr_old
			- ((pr_old + (1U << (PPMD_PERIOD_BITS - 2))) >> PPMD_PERIOD_BITS);

		if( s->rCode - s->rLow < size0 )
		{
			unsigned freq = st->Freq;
			CPpmd8_Context *c = Ppmd8_GetContext(p, Ppmd_GET_SUCCESSOR(st));
			Byte sym = st->Symbol;
			*prob = (UInt16)(pr_new + (1U << PPMD_INT_BITS));
			s->rRange = size0;
			SqxRcNorm(s);
			p->FoundState = st;
			p->PrevSuccess = 1;
			p->RunLength++;
			st->Freq = (Byte)(freq + (freq < 196));
			if( p->OrderFall == 0 && (const Byte *)c >= p->UnitsStart )
				p->MaxContext = p->MinContext = c;
			else
				Ppmd8_UpdateModel(p);
			return sym;
		}
		*prob = (UInt16)pr_new;
		p->InitEsc = p->ExpEscape[pr_new >> 10];
		s->rLow += size0;
		s->rRange = (PPMD_BIN_SCALE - pr_old) * q14;
		p->PrevSuccess = 0;
		memset(mask, 0xFF, sizeof(mask));
		mask[Ppmd8Context_OneState(p->MinContext)->Symbol] = 0;
	}

	/* escape loop */
	for( ;; )
	{
		CPpmd_State *st;
		UInt32 freqSum, count, hiCnt, freqSum2;
		CPpmd_See *see;
		CPpmd8_Context *mc;
		unsigned numMasked;
		CPpmd_State *low_states[256];
		unsigned n_low = 0;

		SqxRcNorm(s);
		mc = p->MinContext;
		numMasked = mc->NumStats;
		do {
			p->OrderFall++;
			if( !mc->Suffix )
				return -1;
			mc = Ppmd8_GetContext(p, mc->Suffix);
		} while( mc->NumStats == numMasked );
		p->MinContext = mc;
		st = Ppmd8_GetStats(p, mc);
		{
			unsigned ns2 = mc->NumStats;
			unsigned want = ns2 > numMasked ? ns2 - numMasked : 0;
			unsigned pos = 0, limit = ns2 + 1;
			CPpmd_State *q = st;
			hiCnt = 0;
			for( ; pos < limit && want > 0; pos++, q++ )
			{
				if( mask[q->Symbol] != 0 )
				{
					low_states[n_low++] = q;
					hiCnt += q->Freq;
					want--;
				}
			}
		}
		see = Ppmd8_MakeEscFreq(p, numMasked, &freqSum);
		freqSum += hiCnt;
		freqSum2 = freqSum;
		if( freqSum2 > s->rRange )
			freqSum2 = s->rRange;
		count = SqxRcThreshold(s, freqSum2);
		if( count < hiCnt )
		{
			Byte sym;
			hiCnt = count;
			for( i = 0; ; )
			{
				st = low_states[i++];
				count -= st->Freq;
				if( (Int32)count < 0 )
					break;
			}
			SqxRcDecode(s, (hiCnt - count) - st->Freq, st->Freq);
			SqxRcNorm(s);
			Ppmd_See_UPDATE(see);
			p->FoundState = st;
			sym = st->Symbol;
			Ppmd8_Update2(p);
			return sym;
		}
		if( count >= freqSum2 )
			return -1;
		SqxRcDecode(s, hiCnt, freqSum2 - hiCnt);
		see->Summ = (UInt16)(see->Summ + freqSum);
		/* mask only the counted window (reference masks low_states,
		   not the whole context -- wider masking undercounts later
		   orders and diverges) */
		for( i = 0; i < n_low; i++ )
			mask[low_states[i]->Symbol] = 0;
	}
}

/* ---- member driver: decode IoStruct.l64CompSize bytes into
   IoStruct.l64OrigSize bytes through WriteOutBuf ---- */
#define SQXPPMD_OUTBUF 4096

int SqxUnPpmd(void)
{
	SLONG64 csz64 = IoStruct.l64CompSize;
	SLONG64 usz64 = IoStruct.l64OrigSize;
	unsigned char *comp;
	SLONG64 got;
	UInt32 csz, usz;
	UInt32 order, mem;
	int i;
	UBYTE out[SQXPPMD_OUTBUF];
	UInt32 outn = 0;
	SLONG64 total = 0;

	if( csz64 < 0 || csz64 > 0x7FFFFFFFL || usz64 < 0 || usz64 > 0x7FFFFFFFL )
	{
		ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return 0;
	}
	csz = (UInt32)csz64;
	usz = (UInt32)usz64;

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

	if( !g_sq.constructed )
	{
		Ppmd8_Construct(&g_sq.model);
		g_sq.constructed = 1;
		g_sq.have_model = 0;
	}
	SqxBitsInit(&g_sq, comp, csz);
	g_sq.rLow = 0;
	g_sq.rCode = 0;
	g_sq.rRange = 0xFFFFFFFFUL;
	for( i = 0; i < 4; i++ )
		g_sq.rCode = (g_sq.rCode << 8) | SqxBitsGet(&g_sq, 8);

	if( SqxRcGetBits(&g_sq, 1) == 0 )
	{
		order = SqxRcGetBits(&g_sq, 8);
		mem = SqxRcGetBits(&g_sq, 10);
		if( order < PPMD8_MIN_ORDER || order > PPMD8_MAX_ORDER || mem == 0 )
		{
			free(comp);
			ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
			return 0;
		}
		if( !Ppmd8_Alloc(&g_sq.model, mem << 20, &g_sqxpAlloc) )
		{
			free(comp);
			ArcStruct.ArchiveStatus = SQX_EC_OUT_OF_MEMORY;
			return 0;
		}
		Ppmd8_Init(&g_sq.model, order, PPMD8_RESTORE_METHOD_RESTART);
		g_sq.have_model = 1;
	}
	else if( !g_sq.have_model )
	{
		free(comp);
		ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return 0;
	}

	for( total = 0; total < (SLONG64)usz; )
	{
		int sym = SqxPpmdDecodeSymbol(&g_sq);
		if( sym < 0 )
			break;
		out[outn++] = (UBYTE)sym;
		total++;
		if( outn == SQXPPMD_OUTBUF )
		{
			WriteOutBuf(out, outn);
			outn = 0;
			if( ArcStruct.ArchiveStatus != SQX_EC_OK )
				break;
		}
	}
	if( outn != 0 && ArcStruct.ArchiveStatus == SQX_EC_OK )
		WriteOutBuf(out, outn);
	free(comp);

	if( total != (SLONG64)usz || ArcStruct.ArchiveStatus != SQX_EC_OK )
	{
		if( ArcStruct.ArchiveStatus == SQX_EC_OK )
			ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return 0;
	}
	IoStruct.l64OrigSize = 0;
	ProgressStruct.l64Processed += usz;
	SetProgress(1);
	return 1;
}
