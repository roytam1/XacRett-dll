/*
 * SPDX-License-Identifier: MIT-0
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 */

#ifndef IZ_ARM_CRYPTO_H
# define IZ_ARM_CRYPTO_H
# include "iz_arm_caps.h"
# ifdef IZ_ARM_CRYPTO_SUPPORTED
#  include <arm_neon.h>
#  define IZ_ARM_TARGET __attribute__ ((target ("+crypto"), noinline))

#  ifndef NO_ARM_AES
#   define IZ_ARM_AES_ENABLED 1
static IZ_ARM_TARGET __attribute__ ((unused)) void
iz_arm_aes_block (const unsigned char *keys, unsigned rounds,
                  const unsigned char *in, unsigned char *out)
{
  unsigned i;
  uint8x16_t s = vld1q_u8 (in);

  for (i = 0; i < rounds - 1; i++)
    {
      s = vaeseq_u8 (s, vld1q_u8 (keys + 16 * i));
      s = vaesmcq_u8 (s);
    }

  s = vaeseq_u8 (s, vld1q_u8 (keys + 16 * (rounds - 1)));
  s = veorq_u8 (s, vld1q_u8 (keys + 16 * rounds));
  vst1q_u8 (out, s);
}
#  endif /* ifndef NO_ARM_AES */

#  ifndef NO_ARM_SHA1
#   define IZ_ARM_SHA1_ENABLED 1
/* SHA1 SHA1C/SHA1P/SHA1M and SHA1SU0/SHA1SU1. */
static IZ_ARM_TARGET __attribute__ ((unused)) void
iz_arm_sha1_block (unsigned long h[5], const unsigned char *p)
{
  uint32x4_t w[20], abcd, orig, t;
  unsigned int state[4], e, save_e, new_e;
  unsigned i;

  for (i = 0; i < 4; i++)
    {
      w[i] = vreinterpretq_u32_u8 (vrev32q_u8 (vld1q_u8 (p + 16 * i)));
    }

  for (i = 4; i < 20; i++)
    {
      w[i] = vsha1su1q_u32 (vsha1su0q_u32 (w[i - 4], w[i - 3], w[i - 2]),
                            w[i - 1]);
    }

  for (i = 0; i < 4; i++)
    {
      state[i] = (unsigned int)h[i];
    }

  abcd = vld1q_u32 (state);
  orig = abcd;
  e = save_e = (unsigned int)h[4];
  for (i = 0; i < 20; i++)
    {
      unsigned int k = i < 5 ? 0x5a827999U
                             : (i < 10 ? 0x6ed9eba1U
                                       : (i < 15 ? 0x8f1bbcdcU : 0xca62c1d6U));
      t = vaddq_u32 (w[i], vdupq_n_u32 (k));
      new_e = vsha1h_u32 (vgetq_lane_u32 (abcd, 0));
      if (i < 5)
        {
          abcd = vsha1cq_u32 (abcd, e, t);
        }
      else if (i < 10 || i >= 15)
        {
          abcd = vsha1pq_u32 (abcd, e, t);
        }
      else
        {
          abcd = vsha1mq_u32 (abcd, e, t);
        }

      e = new_e;
    }

  abcd = vaddq_u32 (abcd, orig);
  vst1q_u32 (state, abcd);
  for (i = 0; i < 4; i++)
    {
      h[i] = (unsigned long)state[i];
    }

  h[4] = (unsigned long)(e + save_e);
  {
    volatile unsigned char *z = (volatile unsigned char *)(void *)w;
    for (i = 0; i < sizeof (w); i++)
      {
        z[i] = 0;
      }
  }
}
#  endif /* ifndef NO_ARM_SHA1 */

#  ifndef NO_ARM_SHA256
#   define IZ_ARM_SHA256_ENABLED 1
static IZ_ARM_TARGET __attribute__ ((unused)) void
iz_arm_sha256_block (unsigned long h[8], const unsigned char *p,
                     const unsigned long k[64])
{
  uint32x4_t w[16], abcd, efgh, old_a, old_e, ta, te, t;
  unsigned int state[8];
  unsigned i;

  for (i = 0; i < 4; i++)
    {
      w[i] = vreinterpretq_u32_u8 (vrev32q_u8 (vld1q_u8 (p + 16 * i)));
    }

  for (i = 4; i < 16; i++)
    {
      w[i] = vsha256su1q_u32 (vsha256su0q_u32 (w[i - 4], w[i - 3]), w[i - 2],
                              w[i - 1]);
    }

  for (i = 0; i < 8; i++)
    {
      state[i] = (unsigned int)h[i];
    }

  abcd = vld1q_u32 (state);
  efgh = vld1q_u32 (state + 4);
  old_a = abcd;
  old_e = efgh;
  for (i = 0; i < 16; i++)
    {
      unsigned int kw[4];
      unsigned j;
      for (j = 0; j < 4; j++)
        {
          kw[j] = (unsigned int)k[i * 4 + j];
        }

      t = vaddq_u32 (w[i], vld1q_u32 (kw));
      ta = vsha256hq_u32 (abcd, efgh, t);
      te = vsha256h2q_u32 (efgh, abcd, t);
      abcd = ta;
      efgh = te;
    }

  abcd = vaddq_u32 (abcd, old_a);
  efgh = vaddq_u32 (efgh, old_e);
  vst1q_u32 (state, abcd);
  vst1q_u32 (state + 4, efgh);
  for (i = 0; i < 8; i++)
    {
      h[i] = (unsigned long)state[i];
    }

  {
    volatile unsigned char *z = (volatile unsigned char *)(void *)w;
    for (i = 0; i < sizeof (w); i++)
      {
        z[i] = 0;
      }

    z = (volatile unsigned char *)(void *)state;
    for (i = 0; i < sizeof (state); i++)
      {
        z[i] = 0;
      }
  }
}
#  endif /* ifndef NO_ARM_SHA256 */

#  ifndef NO_ARM_PMULL
#   define IZ_ARM_PMULL_ENABLED 1
/* Polynomial multiplication in little-endian coefficient order. */
static IZ_ARM_TARGET __attribute__ ((unused)) void
iz_arm_ghash_block (unsigned char acc[16], const unsigned char hkey[16],
                    const unsigned char p[16])
{
  uint8x16_t ax = vrbitq_u8 (veorq_u8 (vld1q_u8 (acc), vld1q_u8 (p)));
  uint8x16_t bx = vrbitq_u8 (vld1q_u8 (hkey));
  unsigned long long a[2], b[2];
  poly128_t r;
  unsigned long long low[2], hi[2], cross[2], v[2], carry;
  unsigned i;

  vst1q_u64 ((uint64_t *)(void *)a, vreinterpretq_u64_u8 (ax));
  vst1q_u64 ((uint64_t *)(void *)b, vreinterpretq_u64_u8 (bx));
  r = vmull_p64 ((poly64_t)a[0], (poly64_t)b[0]);
  __builtin_memcpy (low, &r, 16);
  r = vmull_p64 ((poly64_t)a[1], (poly64_t)b[1]);
  __builtin_memcpy (hi, &r, 16);
  r = vmull_p64 ((poly64_t)(a[0] ^ a[1]), (poly64_t)(b[0] ^ b[1]));
  __builtin_memcpy (cross, &r, 16);
  cross[0] ^= low[0] ^ hi[0];
  cross[1] ^= low[1] ^ hi[1];
  low[1] ^= cross[0];
  hi[0] ^= cross[1];
  /* x^128 = x^7+x^2+x+1, then reduce the 7 overflow bits. */
  v[0] = low[0] ^ hi[0] ^ (hi[0] << 1) ^ (hi[0] << 2) ^ (hi[0] << 7);
  v[1] = low[1] ^ hi[1] ^ (hi[1] << 1) ^ (hi[1] << 2) ^ (hi[1] << 7)
         ^ (hi[0] >> 63) ^ (hi[0] >> 62) ^ (hi[0] >> 57);
  carry = (hi[1] >> 63) ^ (hi[1] >> 62) ^ (hi[1] >> 57);
  v[0] ^= carry ^ (carry << 1) ^ (carry << 2) ^ (carry << 7);
  vst1q_u8 (acc, vrbitq_u8 (vreinterpretq_u8_u64 (
                     vld1q_u64 ((const uint64_t *)(const void *)v))));
  /* Erase intermediate GHASH values as the portable implementation does. */
  {
    volatile unsigned char *z;
    z = (volatile unsigned char *)(void *)a;
    for (i = 0; i < sizeof (a); i++)
      {
        z[i] = 0;
      }

    z = (volatile unsigned char *)(void *)b;
    for (i = 0; i < sizeof (b); i++)
      {
        z[i] = 0;
      }

    z = (volatile unsigned char *)(void *)low;
    for (i = 0; i < sizeof (low); i++)
      {
        z[i] = 0;
      }

    z = (volatile unsigned char *)(void *)hi;
    for (i = 0; i < sizeof (hi); i++)
      {
        z[i] = 0;
      }

    z = (volatile unsigned char *)(void *)cross;
    for (i = 0; i < sizeof (cross); i++)
      {
        z[i] = 0;
      }

    z = (volatile unsigned char *)(void *)v;
    for (i = 0; i < sizeof (v); i++)
      {
        z[i] = 0;
      }
  }
}
#  endif /* ifndef NO_ARM_PMULL */
#  undef IZ_ARM_TARGET
# endif /* IZ_ARM_CRYPTO_SUPPORTED */
#endif /* IZ_ARM_CRYPTO_H */
