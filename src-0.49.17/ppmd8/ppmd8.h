/* Ppmd8.h -- Ppmd8 (PPMdI) compression codec.
 * ANSI C89 adaptation for Info-ZIP.
 *
 * Derived from 7-Zip 26.03 commit
 * 0766b733fe3e06dd2a7f9a3cfbf2108ac73abd17.
 * Igor Pavlov: Public domain.  Based on PPMd var.I (2002),
 * Dmitry Shkarin, and the carryless range coder (1999),
 * Dmitry Subbotin; both public domain.
 */
#ifndef INFOZIP_PPMD8_H
#define INFOZIP_PPMD8_H

#include "ppmd.h"

#define PPMD8_MIN_ORDER 2
#define PPMD8_MAX_ORDER 16

struct CPpmd8_Context_;
typedef UInt32 CPpmd8_Context_Ref;

typedef struct CPpmd8_Context_ {
    Byte NumStats;
    Byte Flags;
    union {
        UInt16 SummFreq;
        CPpmd_State2 State2;
    } Union2;
    union {
        CPpmd_State_Ref Stats;
        CPpmd_State4 State4;
    } Union4;
    CPpmd8_Context_Ref Suffix;
} CPpmd8_Context;

#define Ppmd8Context_OneState(p) ((CPpmd_State *)&(p)->Union2)

/* ZIP method 98 defines restore values 0, 1, and 2.  The upstream 26.03
 * codec disables FREEZE (2) by default because the corrected PPMdI rev.2
 * model is not bit-compatible with some streams made by PPMdI rev.1.
 * Info-ZIP keeps that distinction explicit in the wrapper. */
enum {
    PPMD8_RESTORE_METHOD_RESTART,
    PPMD8_RESTORE_METHOD_CUT_OFF,
    PPMD8_RESTORE_METHOD_FREEZE,
    PPMD8_RESTORE_METHOD_UNSUPPORTED
};

typedef struct {
    CPpmd8_Context *MinContext, *MaxContext;
    CPpmd_State *FoundState;
    unsigned OrderFall, InitEsc, PrevSuccess, MaxOrder, RestoreMethod;
    Int32 RunLength, InitRL;
    UInt32 Size;
    UInt32 GlueCount;
    unsigned LegacyFreeze;
    UInt32 AlignOffset;
    Byte *Base, *LoUnit, *HiUnit, *Text, *UnitsStart;
    UInt32 Range;
    UInt32 Code;
    UInt32 Low;
    union {
        IByteInPtr In;
        IByteOutPtr Out;
    } Stream;
    Byte Indx2Units[PPMD_NUM_INDEXES + 2];
    Byte Units2Indx[128];
    CPpmd_Void_Ref FreeList[PPMD_NUM_INDEXES];
    UInt32 Stamps[PPMD_NUM_INDEXES];
    Byte NS2BSIndx[256], NS2Indx[260];
    Byte ExpEscape[16];
    CPpmd_See DummySee, See[24][32];
    UInt16 BinSumm[25][64];
} CPpmd8;

void Ppmd8_Construct(CPpmd8 *p);
BoolInt Ppmd8_Alloc(CPpmd8 *p, UInt32 size, ISzAllocPtr alloc);
void Ppmd8_Free(CPpmd8 *p, ISzAllocPtr alloc);
void Ppmd8_Init(CPpmd8 *p, unsigned maxOrder, unsigned restoreMethod);
void Ppmd8_SetLegacyFreeze(CPpmd8 *p, BoolInt legacy);
#define Ppmd8_WasAllocated(p) ((p)->Base != NULL)

#define Ppmd8_GetPtr(p, ptr) Ppmd_GetPtr((p), (ptr))
#define Ppmd8_GetContext(p, ptr) Ppmd_GetPtr_Type((p), (ptr), CPpmd8_Context)
#define Ppmd8_GetStats(p, ctx) \
    Ppmd_GetPtr_Type((p), (ctx)->Union4.Stats, CPpmd_State)

void Ppmd8_Update1(CPpmd8 *p);
void Ppmd8_Update1_0(CPpmd8 *p);
void Ppmd8_Update2(CPpmd8 *p);
#define Ppmd8_GetBinSumm(p) \
    &(p)->BinSumm[(p)->NS2Indx[(size_t)Ppmd8Context_OneState((p)->MinContext)->Freq - 1]] \
    [(p)->PrevSuccess + (((p)->RunLength >> 26) & 0x20) + \
     (p)->NS2BSIndx[Ppmd8_GetContext((p), (p)->MinContext->Suffix)->NumStats] + \
     (p)->MinContext->Flags]

CPpmd_See *Ppmd8_MakeEscFreq(CPpmd8 *p, unsigned numMasked, UInt32 *scale);
#define PPMD8_CORRECT_SUM_RANGE(p, sum) \
    if ((sum) > (p)->Range) (sum) = (p)->Range;

#define PPMD8_SYM_END (-1)
#define PPMD8_SYM_ERROR (-2)
BoolInt Ppmd8_Init_RangeDec(CPpmd8 *p);
#define Ppmd8_RangeDec_IsFinishedOK(p) ((p)->Code == 0)
int Ppmd8_DecodeSymbol(CPpmd8 *p);

#define Ppmd8_Init_RangeEnc(p) { (p)->Low = 0; (p)->Range = 0xFFFFFFFFUL; }
void Ppmd8_Flush_RangeEnc(CPpmd8 *p);
void Ppmd8_EncodeSymbol(CPpmd8 *p, int symbol);

#endif
