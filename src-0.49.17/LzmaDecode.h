#ifndef AFX_LZMADECODE_H__LZMA_STREAM_ENGINE_INCLUDED_
#define AFX_LZMADECODE_H__LZMA_STREAM_ENGINE_INCLUDED_

// Shared LZMA-Alone decode engine (drives 7-Zip LzmaDec over callbacks).
// Used by CZipTool::UnLzma (ZIP method 14 framing) and by standalone
// .lzma / .tar.lzma providers (LZMA Alone framing). Transliterated from
// the verified UnLzma loop; re-verify all m14 fixtures if touched.

#include "Tool.h"
extern "C" {
#include "lzma/LzmaDec.h"
}

// Fill/write callbacks. Fill returns bytes placed (>0), 0 on clean EOF,
// negative on error. Write must accept everything offered.
typedef int (*LzmaFillFn)(void *ctx, BYTE *buf, int want);
typedef void (*LzmaWriteFn)(void *ctx, const BYTE *buf, int len);

// Decode one LZMA-Alone stream with 5-byte props. When exact is true,
// output is capped at usz (marker-less streams) and success requires
// total==usz; otherwise decoding runs to the EOS marker. Returns true
// on a clean end, false on corrupt/truncated input (never spins).
bool LzmaDecodeStream(const BYTE props[LZMA_PROPS_SIZE],
                      LzmaFillFn fill, void *ictx,
                      LzmaWriteFn write, void *octx,
                      DWORD usz, bool exact);

#endif
