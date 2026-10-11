#ifndef LHASABRIDGE_H
#define LHASABRIDGE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decode one archive member with the vendored lhasa framework. method is
   one of "-lh4-", "-lhx-", "-lk7-", "-pm0-", "-pm1-" or "-pm2-". infile
   must be positioned at the first byte of the member data; insize and
   outsize come from the LZH member header. Returns 1 when exactly
   outsize bytes were produced, 0 otherwise. */
int LhasaDecode(const char *method, FILE *infile, unsigned long insize,
                FILE *outfile, unsigned long outsize);

#ifdef __cplusplus
}
#endif

#endif /* LHASABRIDGE_H */
