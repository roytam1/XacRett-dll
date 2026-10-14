#ifndef AFX_XZDECODE_H__XZ_STREAM_ENGINE_INCLUDED_
#define AFX_XZDECODE_H__XZ_STREAM_ENGINE_INCLUDED_

// Shared XZ container engine (one complete XZ stream: header, LZMA2
// blocks, index, footer; drives 7-Zip Lzma2Dec). Used by
// CZipTool::UnXz (ZIP method 95, exact size known) and by standalone
// .xz / .tar.xz providers (size from trailer/index). Transliterated
// from the verified UnXz; re-verify all m95 fixtures if touched.

#include "Tool.h"

// Fill returns bytes placed (>0), 0 on clean sticky EOF, <0 on error.
// Write must accept everything offered.
typedef int (*XzFillFn)(void *ctx, BYTE *buf, int want);
typedef void (*XzWriteFn)(void *ctx, const BYTE *buf, int len);

// Decode one XZ stream. When exact is true, output is capped at usz
// (with an end-marker path that cannot stall) and success requires
// total==usz; otherwise decoding runs to the LZMA2 markers and the
// index sum must equal the produced total. Single LZMA2 filter only;
// None/CRC32/CRC64 checks; dicts clamped to 4KB..64MB. Trailing bytes
// after the footer fail. Returns true on full validation.
bool XzDecodeStream(XzFillFn fill, void *ictx,
                    XzWriteFn write, void *octx,
                    DWORD usz, bool exact);

#endif
