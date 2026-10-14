/*
 * SHA-1 implementation
 * SPDX-License-Identifier: MIT-0
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 */

#ifndef IZ_SHA1_H
# define IZ_SHA1_H
# include <stddef.h>
# include <string.h>
# ifndef IZ_SHA1_TYPE_DEFINED

typedef struct
{
  unsigned long h[5], low, high;
  unsigned char block[64];
  unsigned used;
} iz_sha1;

#  define IZ_SHA1_TYPE_DEFINED 1
# endif /* ifndef IZ_SHA1_TYPE_DEFINED */

void iz_sha1_init (iz_sha1 *s);
void iz_sha1_update (iz_sha1 *s, const unsigned char *p, size_t n);
void iz_sha1_finish (iz_sha1 *s, unsigned char out[20]);
int iz_sha1_hardware_active (void);

#endif /* IZ_SHA1_H */
