///////////////////////////////////////////////////////////////////
// gca_cod.cpp -- range decoder transliteration (see gca_cod.h).
///////////////////////////////////////////////////////////////////
#include "gca_cod.h"

void GcaMemStream::open(unsigned char* buf, unsigned long len)
{
    eof = 0;
    size = len;
    base = buf;
    pos = buf;
}

unsigned GcaMemStream::readByteInit()
{
    if ((unsigned long)(pos - base) >= size) {
        eof = 1;
        return 0;
    }
    { unsigned b = *pos; pos++; return b & 0xFF; }
}

void GcaRangeDecoder::decodeInit()
{
    // Consume-and-discard one byte (checked).
    if ((unsigned long)(stream->pos - stream->base) >= stream->size)
        stream->eof = 1;
    else
        stream->advanceRaw();
    // Read one byte (checked, 0 on EOF).
    { unsigned b = stream->readByteInit(); buf = b; code = b >> 1; }
    range = 0x80;
}

void GcaRangeDecoder::normalize()
{
    if (range > 0x800000UL)
        return;
    do {
        code = ((code << 1) | (buf & 1UL)) << 7;
        { unsigned b = stream->readByteRaw(); buf = b; code |= (b >> 1); }
        range <<= 8;
    } while (range <= 0x800000UL);
}

unsigned long GcaRangeDecoder::decode2()
{
    normalize();
    return code / (range >> 12);
}

void GcaRangeDecoder::update2(unsigned long cum, unsigned long freq)
{
    unsigned long r12 = range >> 12;
    range = r12 * freq;
    code -= r12 * cum;
}

int GcaRangeDecoder::decodeNum(unsigned long* probs, int n, int shift)
{
    unsigned long* pb;
    int i;
    unsigned sc = ((unsigned)shift) & 31;
    if (n <= 0)
        return 0;
    pb = probs;
    i = 0;
    for (;;) {
        unsigned long r12;
        unsigned long bound;
        normalize();
        r12 = range >> 12;
        bound = r12 * (*pb);
        if (code < bound) {
            range = bound;
            *pb = (*pb) + ((0x1000UL - (*pb)) >> sc);
            pb++;
            i++;
            if (i >= n)
                return i;
        } else {
            code -= bound;
            range -= bound;
            *pb = (*pb) - ((*pb) >> sc);
            return i;
        }
    }
}
