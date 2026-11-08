#ifndef AFX_UHA_DEC0_H__UHA_DEC0_INCLUDED__
#define AFX_UHA_DEC0_H__UHA_DEC0_INCLUDED__

/* Shared decode callbacks + core 0 (STORE) + core 1 (RLE).
 * The pump mirrors the house ZipTool/LzmaDecode pattern: decoders pull
 * via read_cb and push via write_cb; the container supplies them.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*UhaReadCb)(void* ctx, unsigned char* buf, unsigned want);
typedef int (*UhaWriteCb)(void* ctx, const unsigned char* buf, unsigned len);

/* Core selector: method -> core, version-specific (FORMAT.md 1.2).
 * ver is 2, 3, 4 or 6. Returns -1 for an unknown method.
 * 0.3 method 80 aliases to 0 (STORE).
 */
int UhaCoreOf(int method, int ver);

/* Dictionary size for a method, 1 << (((method-base) % span) + 10);
 * base/span are version-specific (FORMAT.md section 1.2 table).
 * Returns 0 for methods that have no dictionary.
 */
unsigned long UhaDictSize(int method, int ver);

/* STORE: copy out_len bytes through.
 * RLE: decode to out_len bytes (FORMAT.md 6.x core 1 description is
 * not in FORMAT.md; the byte format is control/literal pairs in
 * uharc_dec0.c — decode follows the oracle until Phase 2 lands a
 * verified implementation, then this function is final).
 * Both return 0 ok, 1 bad args, 3 input short, 4 output short.
 */
int UhaDecStore(UhaReadCb rcb, void* rctx, UhaWriteCb wcb, void* wctx,
	unsigned long out_len);
int UhaDecRle(UhaReadCb rcb, void* rctx, UhaWriteCb wcb, void* wctx,
	unsigned long out_len);

#ifdef __cplusplus
}
#endif

#endif
