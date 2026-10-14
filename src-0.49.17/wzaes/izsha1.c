/*
 * SHA-1 implementation
 * SPDX-License-Identifier: MIT-0
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 */

#include "izsha1.h"
#include <string.h>
#include "izsha1_ni.h"
#include "iz_arm_crypto.h"

static void
iz_sha1_wipe (void *ptr, size_t n)
{
  volatile unsigned char *p = (volatile unsigned char *)ptr;

  while (n--)
    {
      *p++ = 0;
    }
}

#define IZ_SHA1_M32(v) ((v) & 0xffffffffUL)
#define IZ_SHA1_ROL(v, n) IZ_SHA1_M32 (((v) << (n)) | ((v) >> (32 - (n))))

int
iz_sha1_hardware_active (void)
{
#ifdef IZ_SHA1_NI_ENABLED
  if (iz_sha1_ni_available ())
    return 1;
#endif
#ifdef IZ_ARM_SHA1_ENABLED
  if (iz_arm_has (IZ_ARM_SHA1_BIT))
    return 1;
#endif
  return 0;
}

static void
iz_sha1_transform (iz_sha1 *s, const unsigned char *p)
{
  unsigned long w[80], a, b, c, d, e, t, k, f;
  unsigned i;
#ifdef IZ_SHA1_NI_ENABLED
  if (iz_sha1_ni_available())
    {
      iz_sha1_ni_transform(s, p);
      return;
    }
#endif
#ifdef IZ_ARM_SHA1_ENABLED
  if (iz_arm_has(IZ_ARM_SHA1_BIT))
    {
      iz_arm_sha1_block(s->h, p);
      return;
    }
#endif

  for (i = 0; i < 16; i++)
    {
      w[i] = ((unsigned long)p[i * 4] << 24)
             | ((unsigned long)p[i * 4 + 1] << 16)
             | ((unsigned long)p[i * 4 + 2] << 8) | p[i * 4 + 3];
    }

  for (i = 16; i < 80; i++)
    {
      w[i] = IZ_SHA1_ROL (w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

  a = s->h[0];
  b = s->h[1];
  c = s->h[2];
  d = s->h[3];
  e = s->h[4];

  for (i = 0; i < 80; i++)
    {
      if (i < 20)
        {
          f = (b & c) | ((~b) & d);
          k = 0x5a827999UL;
        }
      else if (i < 40)
        {
          f = b ^ c ^ d;
          k = 0x6ed9eba1UL;
        }
      else if (i < 60)
        {
          f = (b & c) | (b & d) | (c & d);
          k = 0x8f1bbcdcUL;
        }
      else
        {
          f = b ^ c ^ d;
          k = 0xca62c1d6UL;
        }

      t = IZ_SHA1_M32 (IZ_SHA1_ROL (a, 5) + f + e + k + w[i]);
      e = d;
      d = c;
      c = IZ_SHA1_ROL (b, 30);
      b = a;
      a = t;
    }

  s->h[0] = IZ_SHA1_M32 (s->h[0] + a);
  s->h[1] = IZ_SHA1_M32 (s->h[1] + b);
  s->h[2] = IZ_SHA1_M32 (s->h[2] + c);
  s->h[3] = IZ_SHA1_M32 (s->h[3] + d);
  s->h[4] = IZ_SHA1_M32 (s->h[4] + e);
  iz_sha1_wipe (w, sizeof (w));
}

void
iz_sha1_init (iz_sha1 *s)
{
  memset (s, 0, sizeof (*s));
  s->h[0] = 0x67452301UL;
  s->h[1] = 0xefcdab89UL;
  s->h[2] = 0x98badcfeUL;
  s->h[3] = 0x10325476UL;
  s->h[4] = 0xc3d2e1f0UL;
}

void
iz_sha1_update (iz_sha1 *s, const unsigned char *p, size_t n)
{
  unsigned i;
  unsigned long lo = s->low;

  s->low = IZ_SHA1_M32 (lo + ((unsigned long)n << 3));

  if (s->low < lo)
    {
      s->high = IZ_SHA1_M32 (s->high + 1);
    }

  s->high = IZ_SHA1_M32 (s->high + ((unsigned long)n >> 29));
  while (n)
    {
      i = 64U - s->used;

      if ((size_t)i > n)
        {
          i = (unsigned)n;
        }

      memcpy (s->block + s->used, p, i);
      s->used += i;
      p += i;
      n -= i;

      if (s->used == 64)
        {
          iz_sha1_transform (s, s->block);
          s->used = 0;
        }
    }
}

void
iz_sha1_finish (iz_sha1 *s, unsigned char out[20])
{
  unsigned char len[8], pad[64];
  unsigned long lo = s->low, hi = s->high;
  unsigned i;

  for (i = 0; i < 4; i++)
    {
      len[i] = (unsigned char)(hi >> (24 - i * 8));
      len[i + 4] = (unsigned char)(lo >> (24 - i * 8));
    }

  memset (pad, 0, sizeof (pad));
  pad[0] = 0x80;
  iz_sha1_update (s, pad, s->used < 56 ? 56 - s->used : 120 - s->used);
  iz_sha1_update (s, len, 8);

  for (i = 0; i < 20; i++)
    {
      out[i] = (unsigned char)(s->h[i / 4] >> (24 - (i % 4) * 8));
    }

  iz_sha1_wipe (s, sizeof (*s));
}

#undef IZ_SHA1_M32
#undef IZ_SHA1_ROL
