/*
 * WinZip AE-3 AES-256-GCM support. SPDX-License-Identifier: MIT-0
 * This C89 implementation intentionally shares only the existing AES block
 * cipher, and leaves AE-1/AE-2 PBKDF2-SHA1/CTR/HMAC unchanged.
 * Included at the end of wzaes.c, not a separate translation unit.
 */

#ifndef NO_AES

# define Q32(x) ((x) & 0xffffffffUL)

# define QR(x, n) (((x) >> (n)) | Q32 ((x) << (32 - (n))))

static const unsigned long ae3_k[64]
    = { 0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
        0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
        0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
        0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
        0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
        0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
        0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
        0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
        0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
        0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
        0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
        0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
        0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL };

typedef struct
{
  unsigned long h[8], lo, hi;
  unsigned char block[64];
  unsigned used;
} ae3_sha;

static void
ae3_sha_block (ae3_sha *s, const unsigned char *p)
{
  unsigned long w[64], a, b, c, d, e, f, g, h, x, y, z;
  unsigned i;

#ifdef IZ_SHA256_NI_ENABLED
  if (iz_sha256_ni_available ())
    {
      iz_sha256_ni_block (s->h, p, ae3_k);
      return;
    }
#endif
#ifdef IZ_ARM_SHA256_ENABLED
  if (iz_arm_has (IZ_ARM_SHA2_BIT))
    {
      iz_arm_sha256_block (s->h, p, ae3_k);
      return;
    }
#endif

  for (i = 0; i < 16; i++)
    {
      w[i] = ((unsigned long)p[4 * i] << 24)
             | ((unsigned long)p[4 * i + 1] << 16)
             | ((unsigned long)p[4 * i + 2] << 8) | p[4 * i + 3];
    }

  for (i = 16; i < 64; i++)
    {
      x = QR (w[i - 15], 7) ^ QR (w[i - 15], 18) ^ (w[i - 15] >> 3);
      y = QR (w[i - 2], 17) ^ QR (w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = Q32 (w[i - 16] + x + w[i - 7] + y);
    }

  a = s->h[0];
  b = s->h[1];
  c = s->h[2];
  d = s->h[3];
  e = s->h[4];
  f = s->h[5];
  g = s->h[6];
  h = s->h[7];

  for (i = 0; i < 64; i++)
    {
      x = QR (e, 6) ^ QR (e, 11) ^ QR (e, 25);
      y = (e & f) ^ ((~e) & g);
      z = Q32 (h + x + y + ae3_k[i] + w[i]);
      x = QR (a, 2) ^ QR (a, 13) ^ QR (a, 22);
      y = (a & b) ^ (a & c) ^ (b & c);
      h = g;
      g = f;
      f = e;
      e = Q32 (d + z);
      d = c;
      c = b;
      b = a;
      a = Q32 (z + x + y);
    }

  s->h[0] = Q32 (s->h[0] + a);
  s->h[1] = Q32 (s->h[1] + b);
  s->h[2] = Q32 (s->h[2] + c);
  s->h[3] = Q32 (s->h[3] + d);
  s->h[4] = Q32 (s->h[4] + e);
  s->h[5] = Q32 (s->h[5] + f);
  s->h[6] = Q32 (s->h[6] + g);
  s->h[7] = Q32 (s->h[7] + h);
  iz_aes_wipe (w, sizeof (w));
}

static void
ae3_sha_init (ae3_sha *s)
{
  memset (s, 0, sizeof (*s));
  s->h[0] = 0x6a09e667UL;
  s->h[1] = 0xbb67ae85UL;
  s->h[2] = 0x3c6ef372UL;
  s->h[3] = 0xa54ff53aUL;
  s->h[4] = 0x510e527fUL;
  s->h[5] = 0x9b05688cUL;
  s->h[6] = 0x1f83d9abUL;
  s->h[7] = 0x5be0cd19UL;
}

static void
ae3_sha_update (ae3_sha *s, const unsigned char *p, size_t n)
{
  size_t chunk;
  unsigned long old = s->lo;

  s->lo = Q32 (old + Q32 ((unsigned long)n << 3));
  s->hi = Q32 (s->hi + (s->lo < old) + ((unsigned long)n >> 29));

  while (n)
    {
      chunk = 64U - s->used;

      if (chunk > n)
        {
          chunk = n;
        }

      memcpy (s->block + s->used, p, chunk);
      s->used += (unsigned)chunk;
      p += chunk;
      n -= chunk;

      if (s->used == 64)
        {
          ae3_sha_block (s, s->block);
          s->used = 0;
        }
    }
}

static void
ae3_sha_final (ae3_sha *s, unsigned char d[32])
{
  unsigned char pad[64], len[8];
  unsigned i;

  memset (pad, 0, sizeof (pad));
  pad[0] = 0x80;

  for (i = 0; i < 4; i++)
    {
      len[i] = (unsigned char)(s->hi >> (24 - 8 * i));
      len[i + 4] = (unsigned char)(s->lo >> (24 - 8 * i));
    }

  ae3_sha_update (s, pad, s->used < 56 ? 56U - s->used : 120U - s->used);
  ae3_sha_update (s, len, 8);

  for (i = 0; i < 32; i++)
    {
      d[i] = (unsigned char)(s->h[i / 4] >> (24 - 8 * (i % 4)));
    }

  iz_aes_wipe (s, sizeof (*s));
}

typedef struct
{
  ae3_sha a, b;
} ae3_hmac;

static void
ae3_hmac_init (ae3_hmac *h, const unsigned char *key, size_t n)
{
  unsigned char ip[64], op[64], digest[32];
  unsigned i;

  if (n > 64)
    {
      ae3_sha s;
      ae3_sha_init (&s);
      ae3_sha_update (&s, key, n);
      ae3_sha_final (&s, digest);
      key = digest;
      n = 32;
    }

  memset (ip, 0x36, sizeof (ip));
  memset (op, 0x5c, sizeof (op));

  for (i = 0; i < n; i++)
    {
      ip[i] ^= key[i];
      op[i] ^= key[i];
    }

  ae3_sha_init (&h->a);
  ae3_sha_update (&h->a, ip, 64);
  ae3_sha_init (&h->b);
  ae3_sha_update (&h->b, op, 64);
  iz_aes_wipe (ip, sizeof (ip));
  iz_aes_wipe (op, sizeof (op));
  iz_aes_wipe (digest, sizeof (digest));
}

static void
ae3_hmac_final (ae3_hmac *h, unsigned char d[32])
{
  unsigned char temp[32];

  ae3_sha_final (&h->a, temp);
  ae3_sha_update (&h->b, temp, 32);
  ae3_sha_final (&h->b, d);
  iz_aes_wipe (temp, sizeof (temp));
}

static void
ae3_hmac_run (const unsigned char *key, size_t kn, const unsigned char *a,
              size_t an, const unsigned char *b, size_t bn,
              unsigned char d[32])
{
  ae3_hmac h;

  ae3_hmac_init (&h, key, kn);
  ae3_sha_update (&h.a, a, an);

  if (bn)
    {
      ae3_sha_update (&h.a, b, bn);
    }

  ae3_hmac_final (&h, d);
}

static int
ae3_pbkdf (const char *pass, const unsigned char salt[16],
           unsigned long iterations, unsigned char d[32])
{
  unsigned char s[20], u[32], t[32];
  unsigned long round;
  unsigned i;
  ae3_hmac h, base;
  size_t n;

  if (iterations < 1 || iterations > 64000000UL || pass == NULL)
    {
      return 0;
    }

  n = strlen (pass);
  memcpy (s, salt, 16);
  s[16] = 0;
  s[17] = 0;
  s[18] = 0;
  s[19] = 1;
  ae3_hmac_init (&base, (const unsigned char *)pass, n);
  h = base;
  ae3_sha_update (&h.a, s, 20);
  ae3_hmac_final (&h, u);
  memcpy (t, u, 32);

  for (round = 1; round < iterations; round++)
    {
      h = base;
      ae3_sha_update (&h.a, u, 32);
      ae3_hmac_final (&h, u);

      for (i = 0; i < 32; i++)
        {
          t[i] ^= u[i];
        }
    }

  iz_aes_wipe (&base, sizeof (base));
  memcpy (d, t, 32);
  iz_aes_wipe (s, sizeof (s));
  iz_aes_wipe (t, sizeof (t));
  iz_aes_wipe (u, sizeof (u));

  return 1;
}

static void
ae3_expand (const unsigned char prk[32], const unsigned char *info, size_t n,
            unsigned char *dst, size_t out)
{
  unsigned char t[32], v[64];
  size_t take;
  unsigned char count = 1;
  unsigned prior = 0;

  while (out)
    {
      if (prior)
        {
          memcpy (v, t, prior);
        }

      memcpy (v + prior, info, n);
      v[prior + n] = count++;
      ae3_hmac_run (prk, 32, v, prior + n + 1, NULL, 0, t);
      take = out < 32 ? out : 32;
      memcpy (dst, t, take);
      dst += take;
      out -= take;
      prior = 32;
    }
  iz_aes_wipe (t, sizeof (t));
  iz_aes_wipe (v, sizeof (v));
}

static void
ae3_ghash_block (iz_ae3 *c, const unsigned char p[16])
{
  unsigned char v[16], z[16];
  unsigned i, j, k;
  unsigned carry;

#ifdef IZ_GHASH_PCLMUL_ENABLED
  if (iz_ghash_pclmul_available ())
    {
      iz_ghash_pclmul_block (c->acc, c->h, p);
      return;
    }
#endif
#ifdef IZ_ARM_PMULL_ENABLED
  if (iz_arm_has (IZ_ARM_PMULL_BIT))
    {
      iz_arm_ghash_block (c->acc, c->h, p);
      return;
    }
#endif

  memcpy (v, c->h, 16);
  memset (z, 0, sizeof (z));

  for (i = 0; i < 16; i++)
    {
      unsigned char x = (unsigned char)(c->acc[i] ^ p[i]);

      for (j = 0; j < 8; j++)
        {
          if (x & (0x80U >> j))
            {
              for (k = 0; k < 16; k++)
                {
                  z[k] ^= v[k];
                }
            }

          carry = v[15] & 1U;

          for (k = 15; k > 0; k--)
            {
              v[k] = (unsigned char)((v[k] >> 1) | ((v[k - 1] & 1U) << 7));
            }

          v[0] = (unsigned char)((v[0] >> 1) ^ (carry ? 0xe1U : 0));
        }
    }

  memcpy (c->acc, z, 16);
  iz_aes_wipe (v, sizeof (v));
  iz_aes_wipe (z, sizeof (z));
}

/* NIST SP 800-38D: no more than 2^39-256 ciphertext bits per key. */
static int
ae3_room (iz_ae3 *c, size_t n)
{
  unsigned long add = (unsigned long)n, hi, lo;

  if (n > (size_t)0xffffffffUL)
    {
      return 0;
    }

  lo = Q32 (c->data_lo + add);
  hi = c->data_hi + (lo < c->data_lo);

  return hi < 15UL || (hi == 15UL && lo <= 0xffffffe0UL);
}

int
iz_ae3_update (iz_ae3 *c, const unsigned char *p, size_t n)
{
  size_t take;
  unsigned long old = c->data_lo;

  if (!ae3_room (c, n))
    {
      return 0;
    }

  c->data_lo = Q32 (c->data_lo + (unsigned long)n);
  c->data_hi = Q32 (c->data_hi + (c->data_lo < old));

  while (n)
    {
      take = 16U - c->partial_n;
      if (take > n)
        {
          take = n;
        }

      memcpy (c->partial + c->partial_n, p, take);
      c->partial_n += (unsigned)take;
      p += take;
      n -= take;

      if (c->partial_n == 16)
        {
          ae3_ghash_block (c, c->partial);
          c->partial_n = 0;
        }
    }

  return 1;
}

static void
ae3_next (iz_ae3 *c)
{
  int k;

  for (k = 15; k >= 12; k--)
    {
      if (++c->ctr[k] != 0)
        {
          break;
        }
    }

  aes_encrypt_block (&c->aes, c->ctr, c->stream);
  c->stream_n = 0;
}

int
iz_ae3_encrypt (iz_ae3 *c, unsigned char *p, size_t n)
{
  size_t i;

  if (!ae3_room (c, n))
    {
      return 0;
    }

  for (i = 0; i < n; i++)
    {
      if (c->stream_n == 16)
        {
          ae3_next (c);
        }

      p[i] ^= c->stream[c->stream_n++];
    }

  return iz_ae3_update (c, p, n);
}

void
iz_ae3_decrypt (iz_ae3 *c, unsigned char *p, size_t n)
{
  size_t i;

  iz_ae3_update (c, p, n);

  for (i = 0; i < n; i++)
    {
      if (c->stream_n == 16)
        {
          ae3_next (c);
        }

      p[i] ^= c->stream[c->stream_n++];
    }
}

void
iz_ae3_final (iz_ae3 *c, unsigned char tag[16], unsigned char fin[4])
{
  unsigned char len[16], raw[32];
  unsigned i;

  if (c->partial_n)
    {
      memset (c->partial + c->partial_n, 0, 16 - c->partial_n);
      ae3_ghash_block (c, c->partial);
      c->partial_n = 0;
    }

  memset (len, 0, sizeof (len));

  /* 64-bit ciphertext length in bits; AAD length is zero. */
  for (i = 0; i < 4; i++)
    {
      len[8 + i]
          = (unsigned char)((Q32 ((c->data_hi << 3) | (c->data_lo >> 29)))
                            >> (24 - 8 * i));
      len[12 + i] = (unsigned char)(Q32 (c->data_lo << 3) >> (24 - 8 * i));
    }

  ae3_ghash_block (c, len);
  aes_encrypt_block (&c->aes, c->j0, len);

  for (i = 0; i < 16; i++)
    {
      tag[i] = (unsigned char)(len[i] ^ c->acc[i]);
    }

  ae3_hmac_run (c->aut, 32, (const unsigned char *)"FIN\0", 4, tag, 16, raw);
  memcpy (fin, raw, 4);
  iz_aes_wipe (raw, sizeof (raw));
  iz_aes_wipe (len, sizeof (len));
}

int
iz_ae3_init (iz_ae3 *c, const char *password, const unsigned char salt[16],
             const unsigned char ctr[4], unsigned long iter,
             unsigned char kcv[4])
{
  unsigned char prk[32], dek[32], nonce[12], info[8], raw[32], zero[16];

  if (!ae3_pbkdf (password, salt, iter, prk))
    {
      return 0;
    }

  memset (c, 0, sizeof (*c));
  ae3_expand (prk, (const unsigned char *)"AUT\0", 4, c->aut, 32);
  ae3_hmac_run (c->aut, 32, (const unsigned char *)"KCV\0", 4, NULL, 0, raw);
  memcpy (kcv, raw, 4);
  memcpy (info, "DEK\0", 4);
  memcpy (info + 4, ctr, 4);
  ae3_expand (prk, info, 8, dek, 32);
  memcpy (info, "NCE\0", 4);
  ae3_expand (prk, info, 8, nonce, 12);
  aes_expand (&c->aes, dek, 8);
  memcpy (c->j0, nonce, 12);
  c->j0[15] = 1;
  memcpy (c->ctr, c->j0, 16);
  c->stream_n = 16;
  memset (zero, 0, sizeof (zero));
  aes_encrypt_block (&c->aes, zero, c->h);
  iz_aes_wipe (prk, sizeof (prk));
  iz_aes_wipe (dek, sizeof (dek));
  iz_aes_wipe (nonce, sizeof (nonce));
  iz_aes_wipe (info, sizeof (info));
  iz_aes_wipe (raw, sizeof (raw));
  iz_aes_wipe (zero, sizeof (zero));

  return 1;
}

void
iz_ae3_salt_counter (const unsigned char salt[16], unsigned char ctr[4])
{
  ae3_sha s;
  unsigned char d[32];

  ae3_sha_init (&s);
  ae3_sha_update (&s, salt, 16);
  ae3_sha_final (&s, d);
  memcpy (ctr, d, 4);
  iz_aes_wipe (d, sizeof (d));
}

int
iz_ae3_equal (const unsigned char *a, const unsigned char *b, size_t n)
{
  unsigned char diff = 0;
  size_t i;

  for (i = 0; i < n; i++)
    {
      diff |= (unsigned char)(a[i] ^ b[i]);
    }

  return diff == 0;
}

#endif /* ifndef NO_AES */
