///////////////////////////////////////////////////////////////////
// gca_stage.cpp -- decode stages.
///////////////////////////////////////////////////////////////////
#include "gca_stage.h"
#include <string.h>
#include <stdio.h>

void GcaProbTables::clearSpace()
{
    int i;
    for (i = 0; i < 256; i++) t256[i] = 0x800;
    for (i = 0; i < 32; i++) t32[i] = 0x800;
    for (i = 0; i < 65536; i++) t64K[i] = 0x800;
    for (i = 0; i < 8192; i++) t8K[i] = 0x800;
}

void GcaProbTables::clearSpace1()
{
    int i;
    for (i = 0; i < 256; i++) t256[i] = 0x800;
    for (i = 0; i < 32; i++) t32[i] = 0x800;
}

int GcaStages::variableDecode(unsigned char* out, GcaMemStream* stream,
                              unsigned long total, unsigned long mode)
{
    GcaRangeDecoder rc;
    GcaVarDecoder vd;
    GcaBitDecoder bd;
    unsigned char mtf[256];
    int i;
    unsigned long produced = 0;
    unsigned prev = 0;
    /* mode dword fields (verified vs disassembly):
       bit28 = MTF enable, bit24 = run-length enable,
       (>>20)&F = NumB variant (0 plain, 2 V2, else V),
       (>>16)&F = LargeNum variant (0 plain, else _V),
       (>>12)&F = runMAX?, (>>8)&F = LargeNum shift,
       (>>4)&F = V x-param, NumB shift = FULL dword (&31). */
    unsigned useMtf = (mode >> 28) & 1;
    unsigned useRun = (mode >> 24) & 1;
    unsigned vSel = (mode >> 20) & 0xF;
    unsigned lSel = (mode >> 16) & 0xF;
    unsigned f12 = (mode >> 12) & 0xF;
    unsigned lnShift = (mode >> 8) & 0xF;
    unsigned f4  = (mode >> 4) & 0xF;
    rc.setStream(stream);
    rc.decodeInit();
    vd.rc = &rc;
    bd.rc = &rc;
    if (vSel == 0 && lSel == 0)
        pt->clearSpace1();
    else
        pt->clearSpace();
    for (i = 0; i < 256; i++)
        mtf[i] = (unsigned char)i;
    if (total == 0)
        return 1;
    for (;;) {
        int idx;
        unsigned sym;
        if (vSel == 0)
            idx = bd.decodeNumB(pt->t256, 8, (int)f12);
        else if (vSel == 2)
            idx = vd.decodeNumB_V2(pt->t256,
                /* (prev&FF)*256 DWORDs: asm scales by 1024 BYTES */
                pt->t64K + ((prev & 0xFF) << 8), 8, (int)f12, (int)f4);
        else
            idx = vd.decodeNumB_V(pt->t256,
                pt->t64K + ((prev & 0xFF) << 8), 8, (int)f12, (int)f4, (int)vSel);
        if (useRun) {
            int len;
            if (lSel)
                len = vd.decodeLargeNum_V(
                    /* (prev&FF)*32 DWORDs: asm scales by 128 BYTES */
                    pt->t8K + ((prev & 0xFF) << 5),
                    pt->t32, 32, (int)lnShift, (int)f4, (int)lSel);
            else
                len = vd.decodeLargeNum(pt->t32, 32, (int)lnShift);
            if (useMtf) {
                unsigned mi = ((unsigned)idx + 1) & 0xFF;
                sym = mtf[mi];
                if (mi) {
                    int k;
                    for (k = (int)mi; k > 0; k--) mtf[k] = mtf[k - 1];
                    mtf[0] = (unsigned char)sym;
                }
            } else {
                sym = (unsigned)idx & 0xFF;
            }
            if (len > 0) {
                int run = len + 1;
                while (run > 0 && produced < total) {
                    out[produced++] = (unsigned char)sym;
                    run--;
                }
            } else {
                if (produced < total) out[produced++] = (unsigned char)sym;
            }
        } else {
            if (useMtf) {
                /* runs off: MTF without the +1 (verified vs asm) */
                unsigned mi = (unsigned)idx & 0xFF;
                sym = mtf[mi];
                if (mi) {
                    int k;
                    for (k = (int)mi; k > 0; k--) mtf[k] = mtf[k - 1];
                    mtf[0] = (unsigned char)sym;
                }
            } else {
                sym = (unsigned)idx & 0xFF;
            }
            if (produced < total) out[produced++] = (unsigned char)sym;
        }
        prev = sym;
        if (produced >= total)
            break;
    }
    return 1;
}

int GcaStages::runLengthDecode(unsigned char* out, GcaMemStream* stream,
                               unsigned long size)
{
    GcaRangeDecoder rc;
    GcaVarDecoder vd;
    GcaBitDecoder bd;
    unsigned char mtf[256];
    int i;
    unsigned long produced = 0;
    rc.setStream(stream);
    rc.decodeInit();
    vd.rc = &rc;
    bd.rc = &rc;
    pt->clearSpace1();
    for (i = 0; i < 256; i++)
        mtf[i] = (unsigned char)i;
    while (produced < size) {
        int idx = bd.decodeNumB(pt->t256, 8, 5);
        int len = vd.decodeLargeNum(pt->t32, 32, 6);
        unsigned mi = ((unsigned)idx + 1) & 0xFF;
        unsigned sym = mtf[mi];
        int k;
        for (k = (int)mi; k > 0; k--) mtf[k] = mtf[k - 1];
        mtf[0] = (unsigned char)sym;
        if (len >= 0) {
            int run = len + 1;
            while (run > 0 && produced < size) {
                out[produced++] = (unsigned char)sym;
                run--;
            }
        }
    }
    return 1;
}

int GcaStages::iterationDecode(unsigned char* out, const unsigned char* lit,
                               GcaMemStream* stream, int total,
                               unsigned long* hist)
{
    GcaRangeDecoder rc;
    GcaVarDecoder vd;
    unsigned long locA[32], locB[32];
    int i;
    int outPos = 0;
    int litPos = 0;
    unsigned rolling = 0;
    rc.setStream(stream);
    rc.decodeInit();
    vd.rc = &rc;
    for (i = 0; i < 32; i++) locA[i] = locB[i] = 0x800;
    if (total <= 0)
        return 1;
    while (outPos < total) {
        /* literal phase: count from local table A */
        int v = vd.decodeLargeNum(locA, 32, 6);
        int k;
        if (v < 0)
            return 0;
        for (k = 0; k < v && outPos < total; k++) {
            unsigned char b = lit[litPos++];
            out[outPos] = b;
            hist[(rolling & 0xFFFF)] = (unsigned long)outPos;
            rolling = ((rolling & 0xFF) << 8) | b;
            outPos++;
        }
        if (outPos >= total)
            break;
        /* match phase: count from local table B; 0 = none */
        {
            int m = vd.decodeLargeNum(locB, 32, 6);
            if (m < 0)
                return 0;
            if (m != 0) {
                int mlen = m + 0x40;
                unsigned h = rolling & 0xFFFF;
                int src = (int)hist[h];
                int j;
                if (src < 0 || src > outPos)
                    return 0; /* original throws */
                for (j = 0; j < mlen && outPos < total; j++) {
                    unsigned char b = out[src++];
                    out[outPos] = b;
                    hist[(rolling & 0xFFFF)] = (unsigned long)outPos;
                    rolling = ((rolling & 0xFF) << 8) | b;
                    outPos++;
                }
            }
        }
    }
    return 1;
}
