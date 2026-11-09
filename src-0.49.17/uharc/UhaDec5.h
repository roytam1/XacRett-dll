#ifndef AFX_UHA_DEC5_H__UHA_DEC5_INCLUDED__
#define AFX_UHA_DEC5_H__UHA_DEC5_INCLUDED__

/* Core 5 (PPM2/LZW). Three version-specific coders; this file implements
 * the 0.3 coder (methods 36..79) and the 0.6 coder (methods 40..87,
 * scaffold: refuses loud until its ppmdec is verified). The 0.4 coder
 * follows in this file next.
 *
 * Method range (0.3): dictionary 1 << (((m-36) % 14) + 10).
 * Written from the observed stream format; verified against the
 * 03_mz / img / snd / trig oracle archives in the corpus.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Uha5ReadCb)(void* ctx, unsigned char* buf, unsigned want);
typedef int (*Uha5WriteCb)(void* ctx, const unsigned char* buf, unsigned len);

/* 0.3 core 5 (methods 36..79, 24-bit range/offset coder). */
int UhaDec5_03(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha5ReadCb rcb, void* rctx, Uha5WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec5_03Init(void** pctx, int method);
void UhaDec5_03Free(void* ctx);

/* 0.6 core 5 (methods 40..87). Scaffold: tables + dispatch only. */
int UhaDec5_06(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha5ReadCb rcb, void* rctx, Uha5WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec5_06Init(void** pctx, int method);
void UhaDec5_06Free(void* ctx);

/* 0.4 core 5 (methods 40..87). Placeholder: refuses loud. */
int UhaDec5_04(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha5ReadCb rcb, void* rctx, Uha5WriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDec5_04Init(void** pctx, int method);
void UhaDec5_04Free(void* ctx);

#ifdef __cplusplus
}
#endif

#endif
