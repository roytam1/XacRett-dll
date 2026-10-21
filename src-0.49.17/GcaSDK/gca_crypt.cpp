/////////////////////////////////////////////////////////////////
// gca_crypt.cpp -- GCA "GflGCACryptograph" keystream cipher.
/////////////////////////////////////////////////////////////////
#include "gca_crypt.h"
#include <string.h>

const char* GCA_CRYPT_DEFAULT_PASSWORD = "GflGCACryptograph";

/* ---------------------------------------------------------------
 * The generator: a Mersenne Twister (MT19937).
 *
 *   twist constant   0x9908B0DF
 *   temper constants 0x9D2C5680 / 0xEFC60000
 *
 * gcac.exe uses the classic MT19937 recurrence but seeds the state
 * itself instead of using init_genrand() (sub_4112D0), see setKey().
 * --------------------------------------------------------------- */
#define GCA_MT_N       624
#define GCA_MT_M       397
#define GCA_MT_MAG     0x9908B0DFUL
#define GCA_MT_TEMPA   0x9D2C5680UL
#define GCA_MT_TEMPB   0xEFC60000UL

GcaCrypt::GcaCrypt()
{
    memset(state_, 0, sizeof(state_));
    idx_ = GCA_MT_N;
    word_ = 0;
    counter_ = 0;
}

void GcaCrypt::twist()
{
    unsigned long y;
    int k;
    for (k = 0; k < GCA_MT_N - GCA_MT_M; k++) {
        y = (state_[k] & 0x80000000UL) | (state_[k + 1] & 0x7FFFFFFFUL);
        state_[k] = state_[k + GCA_MT_M] ^ (y >> 1) ^
                    ((state_[k + 1] & 1) ? GCA_MT_MAG : 0);
    }
    for (; k < GCA_MT_N - 1; k++) {
        y = (state_[k] & 0x80000000UL) | (state_[k + 1] & 0x7FFFFFFFUL);
        state_[k] = state_[k + (GCA_MT_M - GCA_MT_N)] ^ (y >> 1) ^
                    ((state_[k + 1] & 1) ? GCA_MT_MAG : 0);
    }
    y = (state_[GCA_MT_N - 1] & 0x80000000UL) | (state_[0] & 0x7FFFFFFFUL);
    state_[GCA_MT_N - 1] = state_[GCA_MT_M - 1] ^ (y >> 1) ^
                           ((state_[0] & 1) ? GCA_MT_MAG : 0);
    idx_ = 0;
}

unsigned long GcaCrypt::next()
{
    unsigned long y;
    if (idx_ >= GCA_MT_N)
        twist();
    y = state_[idx_++];
    y ^= (y >> 11);
    y ^= (y << 7) & GCA_MT_TEMPA;
    y ^= (y << 15) & GCA_MT_TEMPB;
    y ^= (y >> 18);
    return y;
}

/* ---------------------------------------------------------------
 * Key -> generator state (gcac.exe.lst:456-600, sub_401320)
 *
 *   v19[(i+1) % 624] = (v19[(i+1) % 624] << 8) ^ (c * 0x01010101)
 *        with c the *sign extended* key byte (movsx)
 *
 *   state[i] = LCG[i] ^ v19[i % min(len+1, 624)]
 *        LCG seeded at 4357:
 *           s2      = 69069 * s + 1
 *           LCG[i]  = (s & 0xFFFF0000) | (s2 >> 16)
 *           s       = 69069 * s2 + 1
 *
 * The state is then twisted once, like a normal MT19937 init.
 * --------------------------------------------------------------- */
void GcaCrypt::setKey(const unsigned char* key, unsigned long len)
{
    unsigned long v19[GCA_MT_N];
    unsigned long s, s2, m, ebp;
    unsigned long i;

    memset(v19, 0, sizeof(v19));
    for (i = 0; i < len; i++) {
        unsigned long idx = (i + 1) % GCA_MT_N;
        long sc = (long)(signed char)key[i];   /* movsx eax, byte ptr [ecx] */
        unsigned long v = (unsigned long)sc;
        v = (v | (v << 8)) & 0xFFFFFFFFUL;
        v = (v | (v << 16)) & 0xFFFFFFFFUL;    /* c * 0x01010101 */
        v19[idx] = ((v19[idx] << 8) ^ v) & 0xFFFFFFFFUL;
    }

    ebp = len + 1;
    if (ebp > GCA_MT_N)
        ebp = GCA_MT_N;

    s = 4357UL;
    for (i = 0; i < GCA_MT_N; i++) {
        s2 = (69069UL * s + 1UL) & 0xFFFFFFFFUL;
        m = (s & 0xFFFF0000UL) | ((s2 >> 16) & 0xFFFFUL);
        s = (69069UL * s2 + 1UL) & 0xFFFFFFFFUL;
        state_[i] = (m ^ v19[i % ebp]) & 0xFFFFFFFFUL;
    }

    idx_ = GCA_MT_N;
    word_ = 0;
    counter_ = 0;
    twist();
}

/* ---------------------------------------------------------------
 * The keystream word (gcac.exe.lst:650-696, sub_401490)
 *
 *   every four output bytes:
 *       W = ROL32(w0 ^ w2, rot(w1 ^ w3)) ^ ROL32(w4 ^ w6, rot(w5 ^ w7))
 *       rot(x): t = x ^ (x >> 16);  rot = (t & 0x1F) ^ ((t >> 8) & 0x1F)
 *
 *   W is then emitted least significant byte first.
 * --------------------------------------------------------------- */
static unsigned long gcaRol32(unsigned long v, unsigned n)
{
    n &= 31;
    if (n == 0)
        return v;
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFFUL;
}

static unsigned long gcaRotCnt(unsigned long x)
{
    unsigned long t = (x ^ (x >> 16)) & 0xFFFFFFFFUL;
    return ((t & 0x1FUL) ^ ((t >> 8) & 0x1FUL)) & 31UL;
}

unsigned char GcaCrypt::byte()
{
    unsigned long w[8], t;
    int i;
    if ((counter_ & 3) == 0) {
        for (i = 0; i < 8; i++)
            w[i] = next();
        t = gcaRol32(w[0] ^ w[2], gcaRotCnt(w[1] ^ w[3])) ^
            gcaRol32(w[4] ^ w[6], gcaRotCnt(w[5] ^ w[7]));
        word_ = t & 0xFFFFFFFFUL;
    }
    i = (int)(counter_ & 3);
    counter_++;
    return (unsigned char)((word_ >> (i * 8)) & 0xFF);
}

void GcaCrypt::crypt(unsigned char* buf, unsigned long len)
{
    unsigned long i;
    for (i = 0; i < len; i++)
        buf[i] ^= byte();
}

void GcaCrypt::keystream(unsigned char* out, unsigned long len)
{
    unsigned long i;
    for (i = 0; i < len; i++)
        out[i] = byte();
}

/* ---------------------------------------------------------------
 * Key strings
 * --------------------------------------------------------------- */
static std::string gcaSpellOrDefault(const std::string& spell)
{
    if (spell.empty())
        return std::string(GCA_CRYPT_DEFAULT_PASSWORD);
    return spell;
}

std::string gcaCryptPayloadKey(const std::string& spell, const std::string& name)
{
    return gcaSpellOrDefault(spell) + name;
}

std::string gcaCryptDirKey(const std::string& spell)
{
    return gcaSpellOrDefault(spell) + "Header";
}

std::string gcaCryptSolidKey(const std::string& spell)
{
    return gcaSpellOrDefault(spell);
}
