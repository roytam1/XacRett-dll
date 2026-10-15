#ifndef VC4_WINCRYPT_H
#define VC4_WINCRYPT_H
/* wincrypt.h - minimal stand-in for MSVC 4.x, which ships none.
 *
 * Reached ONLY on the VC4 path (via /I vc4compat); VC6 and later
 * use the real SDK header and never see this file.  wzaes.c names
 * CryptoAPI purely through GetProcAddress, so only the types and
 * constants it spells are provided here (fatal C1083 otherwise). */
#if defined(_MSC_VER) && (_MSC_VER < 1200)

typedef unsigned long HCRYPTPROV;
#define PROV_RSA_FULL 1
#define CRYPT_VERIFYCONTEXT 0xF0000000

#endif /* _MSC_VER < 1200 */
#endif /* VC4_WINCRYPT_H */
