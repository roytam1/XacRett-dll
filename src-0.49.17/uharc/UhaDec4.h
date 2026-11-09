#ifndef AFX_UHA_DEC4_H__UHA_DEC4_INCLUDED__
#define AFX_UHA_DEC4_H__UHA_DEC4_INCLUDED__

/* Core 4 (LZP/PPM, range coder). Three version-specific coders share
 * the shape (hash + Fenwick + cost model + PPM) but differ in
 * context counts, dictionary handling and static tables.
 * This file implements the 0.6 coder (methods 24..39); 0.3/0.4
 * variants follow in this file next.
 *
 * Method range (0.6): dictionary 1 << (((method-24) % 16) + 10).
 * Written from the observed stream format; verified against the
 * 06_mx / D3X / gmud-06x oracle archives in the corpus.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Uha4ReadCb)(void* ctx, unsigned char* buf, unsigned want);
typedef int (*Uha4WriteCb)(void* ctx, const unsigned char* buf, unsigned len);

/* Decode one core-4 block. cum[] holds the container's cumulative
 * file-end table (may be NULL): the model resets at its ends.
 * Returns 0 ok, 1 bad args, 2 nomem, 3 input short, 4 output short,
 * 7 data error.
 */
int UhaDec4_06(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha4ReadCb rcb, void* rctx, Uha4WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec4_06Init(void** pctx, int method);
void UhaDec4_06Free(void* ctx);
void UhaDec4_06SetFlags2(unsigned char flags2);

/* 0.4 core 4 (methods 24..39). */
int UhaDec4_04(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha4ReadCb rcb, void* rctx, Uha4WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec4_04Init(void** pctx, int method);
void UhaDec4_04Free(void* ctx);
void UhaDec4_04SetFlags2(unsigned char flags2);

/* 0.3 core 4 (methods 22..35). */
int UhaDec4_03(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha4ReadCb rcb, void* rctx, Uha4WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec4_03Init(void** pctx, int method);
void UhaDec4_03Free(void* ctx);
void UhaDec4_03SetFlags2(unsigned char flags2);

#ifdef __cplusplus
}
#endif

#endif
