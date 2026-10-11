///////////////////////////////////////////////////////////////////
// gca_numv.cpp -- mixed-table + large-number decoders.
///////////////////////////////////////////////////////////////////
#include "gca_numv.h"

int GcaVarDecoder::decodeNumB_V(unsigned long* t1, unsigned long* t2,
                                int bits, int shift, int x, int w)
{
    unsigned sc = ((unsigned)shift) & 31;
    unsigned xc = ((unsigned)x) & 31;
    int result = 0;
    int node = 1;
    int left;
    if (bits <= 0)
        return 0;
    left = bits;
    for (;;) {
        unsigned long mixed;
        unsigned long bound;
        int bit;
        rc->normalize();
        mixed = (t1[node] * (unsigned long)(4 - w) +
                 t2[node] * (unsigned long)w) >> 2;
        bound = (rc->range >> 12) * mixed;
        if (rc->code < bound) {
            unsigned long a, b;
            rc->range = bound;
            a = t1[node]; t1[node] = a + ((0x1000UL - a) >> sc);
            b = t2[node]; t2[node] = b + ((0x1000UL - b) >> xc);
            bit = 0;
        } else {
            unsigned long a, b;
            rc->code -= bound;
            rc->range -= bound;
            a = t1[node]; t1[node] = a - (a >> sc);
            b = t2[node]; t2[node] = b - (b >> xc);
            bit = 1;
        }
        node = bit + node * 2;
        result = bit + result * 2;
        left--;
        if (left == 0)
            break;
    }
    return result;
}

int GcaVarDecoder::decodeNumB_V2(unsigned long* t1, unsigned long* t2,
                                 int bits, int shift, int x)
{
    unsigned sc = ((unsigned)shift) & 31;
    unsigned xc = ((unsigned)x) & 31;
    int result = 0;
    int node = 1;
    int left;
    if (bits <= 0)
        return 0;
    left = bits;
    for (;;) {
        unsigned long mixed;
        unsigned long bound;
        int bit;
        rc->normalize();
        mixed = (t1[node] + t2[node]) >> 1;
        bound = (rc->range >> 12) * mixed;
        if (rc->code < bound) {
            unsigned long a, b;
            rc->range = bound;
            a = t1[node]; t1[node] = a + ((0x1000UL - a) >> sc);
            b = t2[node]; t2[node] = b + ((0x1000UL - b) >> xc);
            bit = 0;
        } else {
            unsigned long a, b;
            rc->code -= bound;
            rc->range -= bound;
            a = t1[node]; t1[node] = a - (a >> sc);
            b = t2[node]; t2[node] = b - (b >> xc);
            bit = 1;
        }
        node = bit + node * 2;
        result = bit + result * 2;
        left--;
        if (left == 0)
            break;
    }
    return result;
}

int GcaVarDecoder::decodeLargeNum(unsigned long* t, int n, int shift)
{
    unsigned sc = ((unsigned)shift) & 31;
    int s, i, nb;
    unsigned long raw = 0;
    /* Phase 1: DecodeNum-style scan. NOTE: exhausting the scan
       (i>=n) falls THROUGH to the raw phase with s=n (it does
       NOT return i); verified against disassembly. */
    if (n <= 0)
        return 0;
    {
        unsigned long* pb = t;
        i = 0;
        for (;;) {
            /* normalize runs PER BIT (verified vs disassembly:
               the scan loops back through the normalize check). */
            unsigned long bound;
            rc->normalize();
            bound = (rc->range >> 12) * (*pb);
            if (rc->code < bound) {
                rc->range = bound;
                *pb = (*pb) + ((0x1000UL - (*pb)) >> sc);
                pb++;
                i++;
                if (i >= n)
                    break;
            } else {
                rc->code -= bound;
                rc->range -= bound;
                *pb = (*pb) - ((*pb) >> sc);
                break;
            }
        }
        s = i;
    }
    if (s <= 1)
        return s;
    /* Phase 2: (s-1) raw bisection bits; value = 2^(s-1) + bits.
       (Verified by rigged-selection probes, NOT (1<<shift).) */
    nb = s - 1;
    while (nb > 0) {
        unsigned long half;
        rc->normalize();
        half = rc->range >> 1;
        raw <<= 1;
        if (rc->code < half) {
            rc->range = half;
        } else {
            raw |= 1;
            rc->code -= half;
            rc->range -= half;
        }
        nb--;
    }
    return (int)(1UL << ((unsigned)(s - 1) & 31)) + (int)raw;
}

int GcaVarDecoder::decodeLargeNum_V(unsigned long* t1, unsigned long* t2,
                                    int n, int shift, int x, int w)
{
    unsigned sc = ((unsigned)shift) & 31;
    unsigned xc = ((unsigned)x) & 31;
    int s, i, nb;
    unsigned long raw = 0;
    /* Phase 1: V-style mixed scan (exhaustion falls to raw, as above). */
    if (n <= 0)
        return 0;
    {
        i = 0;
        for (;;) {
            unsigned long mixed = (t1[i] * (unsigned long)(4 - w) +
                                   t2[i] * (unsigned long)w) >> 2;
            unsigned long bound;
            unsigned long a, b;
            rc->normalize();
            bound = (rc->range >> 12) * mixed;
            if (rc->code < bound) {
                rc->range = bound;
                a = t1[i]; t1[i] = a + ((0x1000UL - a) >> sc);
                b = t2[i]; t2[i] = b + ((0x1000UL - b) >> xc);
                i++;
                if (i >= n)
                    break;
            } else {
                rc->code -= bound;
                rc->range -= bound;
                a = t1[i]; t1[i] = a - (a >> sc);
                b = t2[i]; t2[i] = b - (b >> xc);
                break;
            }
        }
        s = i;
    }
    if (s <= 1)
        return s;
    /* Phase 2: (s-1) raw bisection bits; value = 2^(s-1) + bits. */
    nb = s - 1;
    while (nb > 0) {
        unsigned long half;
        rc->normalize();
        half = rc->range >> 1;
        raw <<= 1;
        if (rc->code < half) {
            rc->range = half;
        } else {
            raw |= 1;
            rc->code -= half;
            rc->range -= half;
        }
        nb--;
    }
    return (int)(1UL << ((unsigned)(s - 1) & 31)) + (int)raw;
}
