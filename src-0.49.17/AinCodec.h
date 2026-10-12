#ifndef AFX_AINCODEC_H__A1B2C3D4_E5F6_4A7B_8C9D_0E1F20304051__INCLUDED_
#define AFX_AINCODEC_H__A1B2C3D4_E5F6_4A7B_8C9D_0E1F20304051__INCLUDED_

// Shared Transas AIN Huffman/LZ77 session decoder (decode side only).
// Clean-room port of the decode path documented in seb3773/AIN (FORMAT.md
// + src/ain.c, used as reference only, never vendored): LSB-first
// bitstream, per-block dynamic canonical Huffman tables (pre/main/len
// with RLE-coded lengths), 32KB LZ77 window, session/block framing.
// Used for both the solid data stream and the M1 index stream (M4 store
// members bypass this and are copied verbatim by the callers).

// Decode one session stream. Returns a malloc'd buffer (caller frees
// with free()) or NULL on failure; *outlen receives the decoded size.
BYTE* AinDecode(const BYTE* src, unsigned long srclen, unsigned long* outlen);

#endif
