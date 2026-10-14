#ifndef AFX_SEVENZCORE_H__2C7E4A19_88D5_4B60_B1E3_5A9C7D2E4F10__INCLUDED_
#define AFX_SEVENZCORE_H__2C7E4A19_88D5_4B60_B1E3_5A9C7D2E4F10__INCLUDED_

// Thin wrapper over the vendored 7-Zip SDK decoder cores (src-0.49.17/sevenz).
//
// This lives in its own translation unit for a hard reason: both
// sevenz/7zTypes.h and lzma/7zTypes.h define `struct ISzAlloc` and share the
// SAME include guard (ZIP7_7Z_TYPES_H), so they can never coexist in one TU.
// SevenZDecode.cpp needs the lzma cores (Lzma2Dec) and this file needs the
// sevenz cores, hence the split. Same pattern as ZipPpmd.cpp vs ZipTool.cpp.
//
// Nothing here leaks an SDK type across the boundary - only plain bytes.

// 7zAES (coder id 0x06F10701) key derivation. cyclesPower/saltSize come
// straight out of the coder props. Returns false for a power we refuse.
// password is the RAW password bytes the KDF hashes, which for 7zAES is
// UTF-16LE - NOT the ANSI string the UI hands us (see the call site in
// SevenZDecode.cpp, which does the ACP conversion).
bool SevenZAesDeriveKey( const BYTE* password, size_t passwordSize,
                         unsigned cyclesPower,
                         const BYTE* salt, unsigned saltSize, BYTE key[32] );

// AES-256-CBC decrypt in place. len MUST be a multiple of 16.
void SevenZAesDecryptCbc( const BYTE key[32], const BYTE iv[16],
                          BYTE* data, DWORD len );

// One-shot SHA-256 (needed for the 7zAES payload check).
void SevenZSha256( const BYTE* data, DWORD len, BYTE digest[32] );

// Branch converter (coder ids 0x0303xxxx / legacy 0x05..0x0B), decode
// direction. Operates in place over the whole buffer with pc = 0, which is
// exactly what 7-Zip does for a folder-wide filter. Returns false if the
// id is not a branch coder we vendored.
bool SevenZBranchDecode( DWORD coderId, BYTE* data, DWORD size );

// BCJ2 (coder id 0x0303011B). All four inputs are exact-length memory
// buffers, dest is the folder output. Returns 0 on success, else a
// reason code (1 call/jump misaligned, 2 dest too big, 3 decode error,
// 4 decoder did not reach the ORIG state).
int  SevenZBcj2Decode( const BYTE* main, DWORD mainLen,
                       const BYTE* call, DWORD callLen,
                       const BYTE* jump, DWORD jumpLen,
                       const BYTE* rc,   DWORD rcLen,
                       BYTE* dest, DWORD destLen );

// Delta (coder id 0x03). `delta` is the distance (props byte + 1).
// Starts from the zero state the encoder started from.
void SevenZDeltaDecode( unsigned delta, BYTE* data, DWORD size );

// PPMd var. H (coder id 0x030401), 7z range-coder flavour.
// props = order byte + 4-byte LE memory size.
bool SevenZPpmd7Decode( const BYTE* props, DWORD propsSize,
                        const BYTE* src, DWORD srcLen,
                        BYTE* dest, DWORD destLen );

#endif