///////////////////////////////////////////////////////////////////
// gca_crypt.h -- the GCA "GflGCACryptograph" payload cipher.
//
// gcac.exe encrypts archive payloads with a Mersenne-Twister based
// keystream.  Everything in this file was reverse engineered from
// the shipped gcac.exe (SYN, Shin-ichi TSURUTA, 2000/2001, v0.9k);
// the citations refer to the IDA listing / Hex-Rays output in the
// repository root.
//
//   gcac.exe.lst:456-600   sub_401320  key -> 624 dword generator state
//   gcac.exe.lst:650-696   sub_401490  8 generator words -> one 32 bit word
//   gcac.exe.lst:714-750   sub_401560  buf[i] ^= keystream byte
//   gcac.exe.lst:28955-29173 sub_411330/411370/4113A0  the generator
//
// The cipher is symmetric: the same routine encrypts and decrypts.
///////////////////////////////////////////////////////////////////
#ifndef GCA_CRYPT_H
#define GCA_CRYPT_H

#include <string>

/* Keystream generator ("GflGCACryptograph"). */
class GcaCrypt {
public:
    GcaCrypt();
    /* Derive the generator state from `key` (len bytes). */
    void setKey(const unsigned char* key, unsigned long len);
    /* XOR `len` bytes at `buf` with the keystream (in place). */
    void crypt(unsigned char* buf, unsigned long len);
    /* Raw keystream bytes (diagnostics). */
    void keystream(unsigned char* out, unsigned long len);
private:
    void twist();
    unsigned long next();
    unsigned char byte();
    unsigned long state_[624];
    int          idx_;
    unsigned long word_;
    unsigned long counter_;
};

/* Key strings used by gcac.exe.
 *
 *   payload : <password> + <stored file name>
 *   header  : <password> + "Header"
 *   solid   : <password>            (solid archives use one stream)
 *
 * An empty password is replaced by "GflGCACryptograph"
 * (gcac.exe.c:5354).  `spell` is the caller supplied password: the
 * raw -p text, or the hex SHA-1 digest produced from a -k key file.
 */
extern const char* GCA_CRYPT_DEFAULT_PASSWORD;

std::string gcaCryptPayloadKey(const std::string& spell, const std::string& name);
std::string gcaCryptDirKey(const std::string& spell);
std::string gcaCryptSolidKey(const std::string& spell);

#endif
