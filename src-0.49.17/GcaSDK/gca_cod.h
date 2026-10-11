///////////////////////////////////////////////////////////////////
// gca_cod.h -- GCA codec foundation: memory stream + range decoder.
//
// Portable C++ (C89-ish, no ASM, LLP64/ILP32 clean). Transliterated
// from GcaSDK 0.9f2 (GflRangeCoder / GflMemoryStream) instruction
// for instruction; validated differentially against the original.
///////////////////////////////////////////////////////////////////
#ifndef GCA_COD_H
#define GCA_COD_H

struct GcaMemStream {
    unsigned long eof;   // +4
    unsigned long size;  // +8
    unsigned char* base; // +0xC
    unsigned char* pos;  // +0x10
    void open(unsigned char* buf, unsigned long len);
    // Blind read (mirrors original: no bounds check in normalize path).
    // Defensive clamp: valid streams never overrun; desyncs then fail
    // cleanly instead of faulting past the buffer.
    unsigned readByteRaw() {
        if ((unsigned long)(pos - base) >= size) return 0;
        { unsigned b = *pos; pos++; return b & 0xFF; }
    }
    // Checked read used at init (sets eof flag, yields 0 past end).
    unsigned readByteInit();
    void advanceRaw() { pos++; } // unchecked skip (init discard byte)
};

struct GcaRangeDecoder {
    unsigned long code;  // +0
    unsigned long range; // +4
    unsigned long buf;   // +8 (low byte live)
    unsigned long x;     // +0xC
    GcaMemStream* stream;// +0x10
    void setStream(GcaMemStream* s) { stream = s; }
    void decodeInit();
    void normalize();
    // Returns code/(range>>12) after normalizing.
    unsigned long decode2();
    // range=(range>>12)*freq; code-=(range>>12)*cum
    void update2(unsigned long cum, unsigned long freq);
    int decodeNum(unsigned long* probs, int n, int shift);
};

#endif
