#ifndef AFX_UHA_DEC3_H__UHA_DEC3_INCLUDED__
#define AFX_UHA_DEC3_H__UHA_DEC3_INCLUDED__

/* Core 3 (ALZ/PPM range coder). Four version-specific coders share
 * the shape (hash + Fenwick + range coder) but differ in context
 * counts and static tables. This file implements the 0.6 coder
 * (methods 8..23, 319 contexts); 0.3/0.4/0.2 variants follow in
 * this file in Phase 4c.
 *
 * Method range (0.6): dictionary 1 << (((method-8) % 16) + 10).
 * Written from the observed stream format; verified against the
 * 06_m1/m2/m3 oracle archives in the corpus.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Uha3ReadCb)(void* ctx, unsigned char* buf, unsigned want);
typedef int (*Uha3WriteCb)(void* ctx, const unsigned char* buf, unsigned len);

/* Decode one core-3 block. cum[] holds the container's cumulative
 * file-end table (may be NULL): the model resets at its ends via
 * the thresh walk (see model_reset). Returns 0 ok, 1 bad args,
 * 2 nomem, 3 input short, 4 output short, 7 data error.
 */
int UhaDec3_06(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha3ReadCb rcb, void* rctx, Uha3WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec3_06Init(void** pctx, int method);
void UhaDec3_06Free(void* ctx);

/* 0.2 core 3 (methods 8..43, 16-bit range init, token model). */
int UhaDec3_02(void* ctx, Uha3ReadCb rcb, void* rctx,
	Uha3WriteCb wcb, void* wctx, unsigned long out_len);
int UhaDec3_02Init(void** pctx, int method);
void UhaDec3_02Free(void* ctx);

/* 0.3 core 3 (methods 8..21, 24-bit range/offset, 328 contexts).
 * First stream byte is a discarded ~out_len marker. */
int UhaDec3_03(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha3ReadCb rcb, void* rctx, Uha3WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec3_03Init(void** pctx, int method);
void UhaDec3_03Free(void* ctx);

/* 0.4 core 3 (methods 8..23, 5-byte range init, 330 contexts). */
int UhaDec3_04(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha3ReadCb rcb, void* rctx, Uha3WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec3_04Init(void** pctx, int method);
void UhaDec3_04Free(void* ctx);

#ifdef __cplusplus
}
#endif

#endif
