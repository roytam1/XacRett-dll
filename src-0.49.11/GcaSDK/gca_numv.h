///////////////////////////////////////////////////////////////////
// gca_numv.h -- mixed-table + large-number range decoders.
///////////////////////////////////////////////////////////////////
#ifndef GCA_NUMV_H
#define GCA_NUMV_H

#include "gca_cod.h"

struct GcaVarDecoder {
    GcaRangeDecoder* rc;
    // Mixed-table binary descent. t1 adapts with `shift`, t2 with `x`.
    int decodeNumB_V(unsigned long* t1, unsigned long* t2,
                     int bits, int shift, int x, int w);
    int decodeNumB_V2(unsigned long* t1, unsigned long* t2,
                      int bits, int shift, int x);
    // Model prefix (DecodeNum-style scan); small results returned
    // directly, larger ones get (s-1) raw bisection bits appended
    // to (1<<shift).
    int decodeLargeNum(unsigned long* t, int n, int shift);
    int decodeLargeNum_V(unsigned long* t1, unsigned long* t2,
                         int n, int shift, int x, int w);
};

#endif
