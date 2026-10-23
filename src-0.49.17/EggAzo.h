#ifndef AFX_EGGAZO_H__AZO_CODEC_INCLUDED__
#define AFX_EGGAZO_H__AZO_CODEC_INCLUDED__

#include "Tool.h"

// AZO (EGG block method 3), decode side only.
//
// Transliterated from the reference decoder in alzgg/src/egg_azo.c, which
// is itself the reverse engineering recorded in alzgg/FORMAT-EGG.md
// appendix A.  The encoder half of the reference is not ported.
//
// One EGG AZO block is a stream:
//
//   '1' | flags | slice ... | 12 zero bytes
//
// Every slice is three big-endian 32-bit words (decompressed size,
// compressed size, size XOR check) followed by its data.  A slice is at
// most 2^19 bytes and every one restarts the models, the history lists and
// the match finder.  When the compressed size equals the decompressed size
// the slice is stored verbatim.  When flags bit 0 is set the x86 call
// filter is applied to every decoded slice (decoder direction).
//
// The output size must match exactly; the caller checks the block CRC.
bool EggAzoDecode( const BYTE* src, DWORD csize, BYTE* dst, DWORD usize );

#endif
