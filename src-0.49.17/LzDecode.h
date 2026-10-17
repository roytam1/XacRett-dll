#ifndef AFX_LZDECODE_H__LZIP_STREAM_ENGINE_INCLUDED__
#define AFX_LZDECODE_H__LZIP_STREAM_ENGINE_INCLUDED__


// Shared lzip (.lz) container engine.  Drives 7-Zip LzmaDec over the
// "LZMA-302eos" stream that lzip mandates, and enforces lzip's 3-factor
// integrity check (CRC32, data size, member size) on every member.
//
// Layout, per the lzip format spec (all multibyte fields little-endian):
//     "LZIP" | VN(1) | DS(1) | LZMA stream | CRC32(4) | DataSize(8) | MemberSize(8)
// so the header is 6 bytes and the trailer 20.  The member header carries no
// properties byte and no size: lzip always uses the default LZMA parameters
// (lc=3, lp=0, pb=2) and terminates every stream with an End Of Stream
// marker.  DS holds the dictionary size, which is why the decoder must size
// its dictionary from DS rather than from a props field.
//
// A file is one or more members back to back; trailing data after the last
// member is ignored, matching lzip's default behaviour.
//
// This engine exists instead of reusing LzmaDecodeStream because the
// container needs the EXACT compressed-stream length: it is not stored
// anywhere (only the member size is, in the trailer at the end of the
// member), so the trailer cannot be located without accounting for every
// byte the decoder consumed.  LzmaDecodeStream buffers internally and does
// not report that, which is also what makes multimember files and trailing
// data untractable through it.


#include "Tool.h"


// Fill returns bytes placed (>0), 0 on clean EOF, <0 on error.
// Write must accept everything offered.
// ULONGLONG does not exist in the VC4 SDK; XzDecode.cpp uses
// __int64 for the same reason (see vc4compat/basetsd.h).  Sizes here
// are unsigned, so use the unsigned form.
typedef unsigned __int64 LzU64;
typedef int  (*LzFillFn)(void *ctx, BYTE *buf, int want);
typedef void (*LzWriteFn)(void *ctx, const BYTE *buf, int len);


// Decode a whole lzip file (every member) to the writer.  Returns true only
// when every member decoded cleanly AND passed all three integrity checks.
bool LzDecodeStream(LzFillFn fill, void *ictx, LzWriteFn write, void *octx);


// Decode a whole lzip file to dst (shared with .tar.lz).  Removes dst and
// returns false on any failure, so a rejected archive never leaves a
// plausible-looking partial file behind.
bool LzDecodeFile( const char *src, const char *dst );


// Total uncompressed size and member count, read from the trailing
// member-size index WITHOUT decoding (lzip's own distributed index).  Sets
// *pTotal to 0 and returns false when the index does not tile the file
// exactly (trailing data, corruption); callers report size 0 then.
bool LzScanFile( const char *src, LzU64 *pTotal, DWORD *pMembers );


#endif