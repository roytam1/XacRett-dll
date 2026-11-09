/* UHARC core 4 (LZP/PPM, methods 22..39). Scaffold: tables +
 * dispatch only. The three version-specific decoders refuse loud
 * (rc=7) until each is ported and verified byte-exact against the
 * oracle corpus; no garbage output is emitted.
 *
 * Method ranges (version-specific; see FORMAT.md):
 *   0.6: 24..39, dict 1 << (((m-24) % 16) + 10)
 *   0.4: 24..39, dict 1 << (((m-24) % 16) + 10)
 *   0.3: 22..35, dict 1 << (((m-22) % 14) + 10)
 */

#include <stdlib.h>
#include <string.h>
#include "UhaDec4.h"

/* ---- 0.6 core 4 (methods 24..39) ---- */

static unsigned char g4_06_flags2;

void UhaDec4_06SetFlags2(unsigned char flags2)
{
	g4_06_flags2 = flags2;
}

int UhaDec4_06Init(void** pctx, int method)
{
	(void)pctx; (void)method;
	return 1;
}

void UhaDec4_06Free(void* ctx)
{
	(void)ctx;
}

int UhaDec4_06(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha4ReadCb rcb, void* rctx, Uha4WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	(void)ctx; (void)cum; (void)ncum; (void)rcb; (void)rctx;
	(void)wcb; (void)wctx; (void)out_len;
	return 7;
}

/* ---- 0.4 core 4 (methods 24..39) ---- */

static unsigned char g4_04_flags2;

void UhaDec4_04SetFlags2(unsigned char flags2)
{
	g4_04_flags2 = flags2;
}

int UhaDec4_04Init(void** pctx, int method)
{
	(void)pctx; (void)method;
	return 1;
}

void UhaDec4_04Free(void* ctx)
{
	(void)ctx;
}

int UhaDec4_04(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha4ReadCb rcb, void* rctx, Uha4WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	(void)ctx; (void)cum; (void)ncum; (void)rcb; (void)rctx;
	(void)wcb; (void)wctx; (void)out_len;
	return 7;
}

/* ---- 0.3 core 4 (methods 22..35) ---- */

static unsigned char g4_03_flags2;

void UhaDec4_03SetFlags2(unsigned char flags2)
{
	g4_03_flags2 = flags2;
}

int UhaDec4_03Init(void** pctx, int method)
{
	(void)pctx; (void)method;
	return 1;
}

void UhaDec4_03Free(void* ctx)
{
	(void)ctx;
}

int UhaDec4_03(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha4ReadCb rcb, void* rctx, Uha4WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	(void)ctx; (void)cum; (void)ncum; (void)rcb; (void)rctx;
	(void)wcb; (void)wctx; (void)out_len;
	return 7;
}
