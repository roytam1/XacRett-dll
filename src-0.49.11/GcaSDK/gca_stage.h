///////////////////////////////////////////////////////////////////
// gca_stage.h -- GCA decode stages (each owns its range coder,
// mirroring the original stack-embedded design).
///////////////////////////////////////////////////////////////////
#ifndef GCA_STAGE_H
#define GCA_STAGE_H

#include "gca_cod.h"
#include "gca_numv.h"
#include "gca_numb.h"

// Contiguous probability block mirroring the original layout so
// out-of-range table reads behave identically.
struct GcaProbTables {
    unsigned long t256[256];   // +0x18AC
    unsigned long t32[32];     // +0x1CAC
    unsigned long t64K[65536]; // +0x1D2C
    unsigned long t8K[8192];   // +0x41D2C
    void clearSpace();   // all four to 0x800
    void clearSpace1();  // first two to 0x800
};

struct GcaStages {
    GcaProbTables* pt;
    // MTF+RLE stage. Returns 1.
    int variableDecode(unsigned char* out, GcaMemStream* stream,
                       unsigned long total, unsigned long mode);
    // RLE stage. Returns 1.
    int runLengthDecode(unsigned char* out, GcaMemStream* stream,
                        unsigned long size);
    // LZ stage (literals from lit[], structure from stream).
    // hist: 65536 DWORDs, zeroed by caller. Returns 1.
    int iterationDecode(unsigned char* out, const unsigned char* lit,
                        GcaMemStream* stream, int total,
                        unsigned long* hist);
};

#endif
