#ifndef AFX_UHA_DEC5_H__UHA_DEC5_INCLUDED__
#define AFX_UHA_DEC5_H__UHA_DEC5_INCLUDED__

/* Core 5 (PPM2/LZW). Version-specific coders; this file implements
 * the 0.6 coder (methods 40..87). The 0.4 and 0.3 variants follow
 * in this file in Phase 5b/c.
 *
 * Method range (0.6): dictionary 1 << (((m-40) % 16) + 10).
 * Written from the observed stream format; verified against the
 * 06_m1/m2/m3 oracle archives and gmud-06m3 (44 files, 33 MB).
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Uha5ReadCb)(void* ctx, unsigned char* buf, unsigned want);
typedef int (*Uha5WriteCb)(void* ctx, const unsigned char* buf, unsigned len);

/* Decode one core-5 block. cum[] holds the container's cumulative
 * file-end table (may be NULL). Returns 0 ok, 1 bad args, 2 nomem,
 * 3 input short, 4 output short, 7 data error.
 */
int UhaDec5_06(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha5ReadCb rcb, void* rctx, Uha5WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec5_06Init(void** pctx, int method);
void UhaDec5_06Free(void* ctx);

/* 0.4 core 5 (methods 40..87). */
int UhaDec5_04(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha5ReadCb rcb, void* rctx, Uha5WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec5_04Init(void** pctx, int method);
void UhaDec5_04Free(void* ctx);

/* 0.3 core 5 (methods 36..79). */
int UhaDec5_03(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha5ReadCb rcb, void* rctx, Uha5WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec5_03Init(void** pctx, int method);
void UhaDec5_03Free(void* ctx);

#ifdef __cplusplus
}
#endif

#endif
