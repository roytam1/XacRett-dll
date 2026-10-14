/* Ppmd.h -- minimal PPMd common definitions for Info-ZIP.
 *
 * Derived from the public-domain 7-Zip PPMd sources, 26.03,
 * commit 0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17.
 * This compatibility header intentionally uses only ANSI C89 facilities.
 */
#ifndef INFOZIP_PPMD_H
#define INFOZIP_PPMD_H

#include <limits.h>
#include <stddef.h>

#if UCHAR_MAX != 255
# error PPMd requires 8-bit bytes
#endif

typedef unsigned char Byte;

#if USHRT_MAX == 65535U
typedef unsigned short UInt16;
#elif UINT_MAX == 65535U
typedef unsigned int UInt16;
#else
# error PPMd requires a 16-bit unsigned integer type
#endif

#if UINT_MAX == 4294967295U
typedef int Int32;
typedef unsigned int UInt32;
#elif ULONG_MAX == 4294967295UL
typedef long Int32;
typedef unsigned long UInt32;
#else
# error PPMd requires a 32-bit integer type
#endif

typedef int BoolInt;
#ifndef True
# define True 1
#endif
#ifndef False
# define False 0
#endif

/* The Info-ZIP adaptation uses offset references on every target.  This is
 * slightly less aggressive than 7-Zip's optional 32-bit pointer fast path,
 * but avoids pointer-size assumptions and is valid on 16/32/64-bit hosts. */
#define Ppmd_Ref_Type(type) UInt32
#define Ppmd_GetRef(p, ptr) ((UInt32)((Byte *)(ptr) - (p)->Base))
#define Ppmd_GetPtr(p, offs) ((void *)((p)->Base + (offs)))
#define Ppmd_GetPtr_Type(p, offs, type) ((type *)Ppmd_GetPtr(p, offs))

#define PPMD_INT_BITS 7
#define PPMD_PERIOD_BITS 7
#define PPMD_BIN_SCALE (1 << (PPMD_INT_BITS + PPMD_PERIOD_BITS))

#define PPMD_GET_MEAN_SPEC(summ, shift, round) \
    (((summ) + (1 << ((shift) - (round)))) >> (shift))
#define PPMD_GET_MEAN(summ) \
    PPMD_GET_MEAN_SPEC((summ), PPMD_PERIOD_BITS, 2)
#define PPMD_UPDATE_PROB_0(prob) \
    ((prob) + (1 << PPMD_INT_BITS) - PPMD_GET_MEAN(prob))
#define PPMD_UPDATE_PROB_1(prob) ((prob) - PPMD_GET_MEAN(prob))

#define PPMD_N1 4
#define PPMD_N2 4
#define PPMD_N3 4
#define PPMD_N4 ((128 + 3 - PPMD_N1 - 2 * PPMD_N2 - 3 * PPMD_N3) / 4)
#define PPMD_NUM_INDEXES (PPMD_N1 + PPMD_N2 + PPMD_N3 + PPMD_N4)

typedef struct {
    UInt16 Summ;
    Byte Shift;
    Byte Count;
} CPpmd_See;

#define Ppmd_See_UPDATE(p) \
    { if ((p)->Shift < PPMD_PERIOD_BITS && --(p)->Count == 0) { \
        (p)->Summ = (UInt16)((p)->Summ << 1); \
        (p)->Count = (Byte)(3 << (p)->Shift++); \
    } }

typedef struct {
    Byte Symbol;
    Byte Freq;
    UInt16 Successor_0;
    UInt16 Successor_1;
} CPpmd_State;

typedef struct CPpmd_State2_ {
    Byte Symbol;
    Byte Freq;
} CPpmd_State2;

typedef struct CPpmd_State4_ {
    UInt16 Successor_0;
    UInt16 Successor_1;
} CPpmd_State4;

typedef UInt32 CPpmd_State_Ref;
typedef UInt32 CPpmd_Void_Ref;
typedef UInt32 CPpmd_Byte_Ref;

/* Successor halves are internal numeric fields; fixed low/high ordering is
 * endian-neutral because they are never serialized as a native UInt32. */
#define Ppmd_GET_SUCCESSOR(p) \
    ((CPpmd_Void_Ref)((p)->Successor_0 | ((UInt32)(p)->Successor_1 << 16)))
#define Ppmd_SET_SUCCESSOR(p, v) { \
    (p)->Successor_0 = (UInt16)((UInt32)(v)); \
    (p)->Successor_1 = (UInt16)((UInt32)(v) >> 16); \
}

#define PPMD_SetAllBitsIn256Bytes(p) \
    { size_t z; for (z = 0; z < 256 / sizeof((p)[0]); z += 8) { \
        (p)[z + 7] = (p)[z + 6] = (p)[z + 5] = (p)[z + 4] = \
        (p)[z + 3] = (p)[z + 2] = (p)[z + 1] = (p)[z] = ~(size_t)0; \
    } }

struct IByteIn_;
typedef const struct IByteIn_ *IByteInPtr;
typedef struct IByteIn_ {
    Byte (*Read)(IByteInPtr p);
} IByteIn;
#define IByteIn_Read(p) ((p)->Read(p))

struct IByteOut_;
typedef const struct IByteOut_ *IByteOutPtr;
typedef struct IByteOut_ {
    void (*Write)(IByteOutPtr p, Byte b);
} IByteOut;
#define IByteOut_Write(p, b) ((p)->Write((p), (b)))

typedef struct ISzAlloc ISzAlloc;
typedef const ISzAlloc *ISzAllocPtr;
struct ISzAlloc {
    void *(*Alloc)(ISzAllocPtr p, size_t size);
    void (*Free)(ISzAllocPtr p, void *address);
};
#define ISzAlloc_Alloc(p, size) ((p)->Alloc((p), (size)))
#define ISzAlloc_Free(p, address) ((p)->Free((p), (address)))

/* 7-Zip uses these only as optimization annotations in Ppmd8. */
#define Z7_NO_INLINE
#define Z7_FORCE_INLINE
#define MY_ALIGN(n)

#endif
