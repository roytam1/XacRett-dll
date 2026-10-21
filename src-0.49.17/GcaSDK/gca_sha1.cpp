/////////////////////////////////////////////////////////////////
// gca_sha1.cpp -- SHA-1 (FIPS 180-1), portable C89.
/////////////////////////////////////////////////////////////////
#include "gca_sha1.h"
#include <string.h>

#define GCA_SHA1_ROL(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

static void gcaSha1Transform(unsigned long state[5], const unsigned char blk[64])
{
    unsigned long w[80];
    unsigned long a, b, c, d, e, t;
    int i;
    for (i = 0; i < 16; i++) {
        w[i] = ((unsigned long)blk[i * 4] << 24) |
               ((unsigned long)blk[i * 4 + 1] << 16) |
               ((unsigned long)blk[i * 4 + 2] << 8) |
               ((unsigned long)blk[i * 4 + 3]);
    }
    for (i = 16; i < 80; i++)
        w[i] = GCA_SHA1_ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];
    for (i = 0; i < 80; i++) {
        if (i < 20)      t = ((b & c) | ((~b) & d)) + 0x5A827999UL;
        else if (i < 40) t = (b ^ c ^ d) + 0x6ED9EBA1UL;
        else if (i < 60) t = ((b & c) | (b & d) | (c & d)) + 0x8F1BBCDCUL;
        else             t = (b ^ c ^ d) + 0xCA62C1D6UL;
        t += GCA_SHA1_ROL(a, 5) + e + w[i];
        e = d; d = c; c = GCA_SHA1_ROL(b, 30); b = a; a = t;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

void gcaSha1(const unsigned char* data, unsigned long len,
             unsigned char digest[20])
{
    unsigned long state[5];
    unsigned char blk[64];
    unsigned long i, n, rem;

    state[0] = 0x67452301UL;
    state[1] = 0xEFCDAB89UL;
    state[2] = 0x98BADCFEUL;
    state[3] = 0x10325476UL;
    state[4] = 0xC3D2E1F0UL;

    n = len / 64;
    for (i = 0; i < n; i++)
        gcaSha1Transform(state, data + i * 64);

    rem = len % 64;
    memset(blk, 0, sizeof(blk));
    if (rem) memcpy(blk, data + n * 64, rem);
    blk[rem] = 0x80;
    if (rem >= 56) {
        gcaSha1Transform(state, blk);
        memset(blk, 0, sizeof(blk));
    }
    {
        /* The message length is appended as a 64 bit big endian bit
           count.  MSVC6 has no "long long", so it is built from two
           32 bit halves. */
        unsigned long lo = (len & 0x1FFFFFFFUL) << 3;
        unsigned long hi = len >> 29;
        blk[63] = (unsigned char)(lo & 0xFF);
        blk[62] = (unsigned char)((lo >> 8) & 0xFF);
        blk[61] = (unsigned char)((lo >> 16) & 0xFF);
        blk[60] = (unsigned char)((lo >> 24) & 0xFF);
        blk[59] = (unsigned char)(hi & 0xFF);
        blk[58] = (unsigned char)((hi >> 8) & 0xFF);
        blk[57] = (unsigned char)((hi >> 16) & 0xFF);
        blk[56] = (unsigned char)((hi >> 24) & 0xFF);
    }
    gcaSha1Transform(state, blk);

    for (i = 0; i < 5; i++) {
        digest[i * 4]     = (unsigned char)((state[i] >> 24) & 0xFF);
        digest[i * 4 + 1] = (unsigned char)((state[i] >> 16) & 0xFF);
        digest[i * 4 + 2] = (unsigned char)((state[i] >> 8) & 0xFF);
        digest[i * 4 + 3] = (unsigned char)(state[i] & 0xFF);
    }
}

void gcaSha1Hex(const unsigned char* data, unsigned long len, char* out)
{
    static const char hex[] = "0123456789abcdef";
    unsigned char digest[20];
    int i;
    gcaSha1(data, len, digest);
    for (i = 0; i < 20; i++) {
        out[i * 2]     = hex[(digest[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[digest[i] & 0xF];
    }
    out[40] = '\0';
}
