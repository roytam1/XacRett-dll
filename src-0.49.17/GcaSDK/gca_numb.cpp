///////////////////////////////////////////////////////////////////
// gca_numb.cpp -- transliteration of RangeCoder::DecodeNumB.
///////////////////////////////////////////////////////////////////
#include "gca_numb.h"

int GcaBitDecoder::decodeNumB(unsigned long* t, int bits, int shift)
{
    unsigned sc = ((unsigned)shift) & 31;
    int result = 0;
    int node = 1;
    int left;
    if (bits <= 0)
        return 0;
    left = bits;
    for (;;) {
        unsigned long bound;
        unsigned long p;
        int bit;
        rc->normalize();
        bound = (rc->range >> 12) * t[node];
        if (rc->code < bound) {
            rc->range = bound;
            p = t[node];
            t[node] = p + ((0x1000UL - p) >> sc);
            bit = 0;
        } else {
            rc->code -= bound;
            rc->range -= bound;
            p = t[node];
            t[node] = p - (p >> sc);
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
