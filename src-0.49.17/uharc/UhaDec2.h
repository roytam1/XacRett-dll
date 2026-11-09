#ifndef AFX_UHA_DEC2_H__UHA_DEC2_INCLUDED__
#define AFX_UHA_DEC2_H__UHA_DEC2_INCLUDED__

/* Core 2 (Huffman/LZW, methods 2..7). Two bit orders:
 *  - LSB (0.2/0.3/0.4): plain LZW with parent/char dictionary;
 *    code width starts at 9, grows to maxbits = method+8, with the
 *    complement scheme for the top of the range (see FORMAT model:
 *    "2 Huffman/LZW" / "2-7 LZW, LSB-first").
 *  - MSB (0.6): tree-based coder with 5-byte nodes and pruning.
 *
 * Written from the observed stream format, verified against
 * 02-W.UHA (LSB) and the -mw generated archives in the oracle corpus
 * (single_*_mw, lz78_*, full_mw, filt_*_mw) for MSB.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*Uha2ReadCb)(void* ctx, unsigned char* buf, unsigned want);
typedef int (*Uha2WriteCb)(void* ctx, const unsigned char* buf, unsigned len);

/* Decode one core-2 block. method 2..7 selects maxbits = method+8.
 * lsb nonzero selects the LSB-first stream (0.2/0.3/0.4), zero the
 * 0.6 MSB stream. out_len is the exact expected size (table sizes
 * or the solid-stream total). Returns 0 ok, 1 bad args, 2 nomem,
 * 3 input short, 4 output short, 7 data error.
 */
int UhaDec2(int method, int lsb,
	Uha2ReadCb rcb, void* rctx, Uha2WriteCb wcb, void* wctx,
	unsigned long out_len);

#ifdef __cplusplus
}
#endif

#endif
