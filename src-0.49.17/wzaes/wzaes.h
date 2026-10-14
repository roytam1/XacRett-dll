/* WinZip AES (AE-1 and AE-2) primitives, C89, no external crypto library.
 * Internal to Info-ZIP; not PKWARE Strong Encryption.
 */
#ifndef IZ_WZAES_H
#define IZ_WZAES_H
#ifndef NO_AES
#include <stddef.h>
typedef struct {
    unsigned long h[5], low, high;
    unsigned char block[64];
    unsigned used;
} iz_sha1;
#define IZ_SHA1_TYPE_DEFINED 1
typedef struct {
    iz_sha1 inner, outer;
} iz_hmac;
typedef struct {
    unsigned char expanded[240];
    unsigned rounds;
    unsigned char counter[16], stream[16];
    unsigned stream_pos;
    iz_hmac mac;
} iz_wzaes;
/* AE-3 uses PBKDF2-HMAC-SHA256, domain-separated HKDF, AES-256-GCM. */
typedef struct {
    iz_wzaes aes;
    unsigned char aut[32], j0[16], ctr[16], stream[16];
    unsigned char h[16], acc[16], partial[16];
    unsigned stream_n, partial_n;
    unsigned long data_lo, data_hi;
} iz_ae3;
int iz_ae3_init(iz_ae3 *, const char *, const unsigned char [16],
                const unsigned char [4], unsigned long, unsigned char [4]);
void iz_ae3_salt_counter(const unsigned char [16], unsigned char [4]);
int iz_ae3_update(iz_ae3 *, const unsigned char *, size_t);
int iz_ae3_encrypt(iz_ae3 *, unsigned char *, size_t);
void iz_ae3_decrypt(iz_ae3 *, unsigned char *, size_t);
void iz_ae3_final(iz_ae3 *, unsigned char [16], unsigned char [4]);
int iz_ae3_equal(const unsigned char *, const unsigned char *, size_t);
unsigned iz_aes_salt_size(unsigned strength);
int iz_aes_init(iz_wzaes *ctx, const char *password,
                const unsigned char *salt, unsigned strength,
                unsigned char verifier[2]);
void iz_aes_encrypt(iz_wzaes *ctx, unsigned char *buf, size_t len);
void iz_aes_decrypt(iz_wzaes *ctx, unsigned char *buf, size_t len);
void iz_aes_auth(iz_wzaes *ctx, unsigned char tag[10]);
void iz_aes_mac_update(iz_wzaes *ctx, const unsigned char *ciphertext, size_t n);
void iz_aes_wipe(void *ptr, size_t n);
int iz_aes_entropy(unsigned char *salt, size_t length);
#endif
#endif
