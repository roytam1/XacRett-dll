///////////////////////////////////////////////////////////////////
// gca_numb.h -- binary-tree range decoders (see gca_cod.h).
///////////////////////////////////////////////////////////////////
#ifndef GCA_NUMB_H
#define GCA_NUMB_H

#include "gca_cod.h"

// Single-table binary descent over `bits` levels. Table entries are
// addressed by node index (root=1); layout must mirror the original
// (contiguous prob block) so out-of-range reads match.
struct GcaBitDecoder {
    GcaRangeDecoder* rc;
    int decodeNumB(unsigned long* t, int bits, int shift);
};

#endif
