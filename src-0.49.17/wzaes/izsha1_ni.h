/* izsha1_ni.h -- local stub (absent from the upstream tree).
   Declares the x86 SHA-NI entry points only when IZ_SHA1_NI_ENABLED
   is defined. Our VC6 build never defines it, so the portable C
   SHA-1 in izsha1.c is always used. */
#ifndef IZ_SHA1_NI_H
#define IZ_SHA1_NI_H
#ifdef IZ_SHA1_NI_ENABLED
#include "izsha1.h"
int iz_sha1_ni_available(void);
void iz_sha1_ni_transform(iz_sha1 *s, const unsigned char *p);
#endif
#endif
