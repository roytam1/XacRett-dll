/*
 * SPDX-License-Identifier: MIT-0
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 */

/*
 * Xceed stream: ASCII block size '1'..'9', followed by bit-packed block
 * marker 0x31 + the BZip2 RLE1/BWT/MTF/Huffman block payload, repeated,
 * then marker 0x17 and zero padding to the next byte boundary.  Regular
 * BZip2 48-bit per-block markers and block/stream CRCs are excluded.
 */

#include "xceedbwt.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define XB_GROUPS 6
#define XB_SELECTORS 18002
#define XB_SYMBOLS 258
#define XB_OUTBUF 32768U

/* C89 does not mandate 32-bit unsigned long so require at least 32 bits. */
#if ULONG_MAX < 0xffffffffUL || UINT_MAX < 0xffffffffUL                       \
    || INT_MAX < 2147483647L || UCHAR_MAX != 255
# error Xceed BWT decoder requires 8-bit bytes and 32-bit int/long
#endif /* if ULONG_MAX < 0xffffffffUL || UINT_MAX < 0xffffffffUL || INT_MAX < \
          2147483647L || UCHAR_MAX != 255 */

typedef struct xb_huff
{
  int min_len, max_len;
  int count[21];
  int first[21];
  int index[21];
  unsigned short order[XB_SYMBOLS];
} xb_huff;

typedef struct xb_state
{
  int (*reader) (void *);
  int (*writer) (void *, const unsigned char *, size_t);
  void *opaque;
  unsigned long (*crc_update) (void *, unsigned long,
                                const unsigned char *, size_t);
  unsigned int current_byte, available_bits;
  int bad_input, write_failed;
  unsigned int block_max;
  unsigned long expect_hi, expect_lo;
  unsigned long size_hi, size_lo;
  unsigned long crc, expected_crc;
  unsigned char output[XB_OUTBUF];
  size_t output_count;
  int inuse[256], flags[16], symtab[256], mtf[256];
  unsigned char selectors[XB_SELECTORS];
  xb_huff trees[XB_GROUPS];
  unsigned char *bwt;
  unsigned int *tt;
} xb_state;

static int
xb_bit (xb_state *s)
{
  int v;

  if (s->bad_input)
    {
      return 0;
    }

  if (s->available_bits == 0U)
    {
      v = s->reader (s->opaque);

      if (v < 0 || v > 255)
        {
          s->bad_input = 1;
          return 0;
        }

      s->current_byte = (unsigned int)v;
      s->available_bits = 8U;
    }

  s->available_bits--;

  return (int)((s->current_byte >> s->available_bits) & 1U);
}

static unsigned int
xb_bits (xb_state *s, unsigned int n)
{
  unsigned int v = 0U, k;

  for (k = 0U; k < n; k++)
    {
      v = (v << 1) | (unsigned int)xb_bit (s);
    }

  return v;
}

static int
xb_tree (xb_huff *h, const int *lengths, int count)
{
  int i, j, out = 0, code = 0;

  memset (h, 0, sizeof (*h));
  h->min_len = 21;

  for (i = 0; i < count; i++)
    {
      int len = lengths[i];

      if (len < 1 || len > 20)
        {
          return -1;
        }

      h->count[len]++;

      if (len < h->min_len)
        {
          h->min_len = len;
        }

      if (len > h->max_len)
        {
          h->max_len = len;
        }
    }

  for (i = h->min_len; i <= h->max_len; i++)
    {
      for (j = 0; j < count; j++)
        {
          if (lengths[j] == i)
            {
              h->order[out++] = (unsigned short)j;
            }
        }
    }

  out = 0;

  for (i = 1; i <= 20; i++)
    {
      code = (code + h->count[i - 1]) << 1;
      h->first[i] = code;
      h->index[i] = out;
      out += h->count[i];
      if (code + h->count[i] > (1 << i))
        {
          return -1;
        }
    }

  return 0;
}

static int
xb_symbol (xb_state *s, const xb_huff *h)
{
  int code = 0, len;

  for (len = 1; len <= h->max_len; len++)
    {
      code = (code << 1) | xb_bit (s);

      if (s->bad_input)
        {
          return -1;
        }

      if (len >= h->min_len && code >= h->first[len]
          && code < h->first[len] + h->count[len])
        {
          return (int)h->order[h->index[len] + code - h->first[len]];
        }
    }

  s->bad_input = 1;
  return -1;
}

static unsigned long
xb_crc_fallback (unsigned long crc, const unsigned char *buf, size_t n)
{
  unsigned long x = (crc ^ 0xffffffffUL) & 0xffffffffUL;
  size_t i;
  unsigned int j;

  for (i = 0U; i < n; i++)
    {
      x ^= (unsigned long)buf[i];
      for (j = 0U; j < 8U; j++)
        {
          x = (x >> 1) ^ ((x & 1UL) ? 0xedb88320UL : 0UL);
        }
    }

  return (x ^ 0xffffffffUL) & 0xffffffffUL;
}

static void
xb_crc_buffer (xb_state *s)
{
  if (s->crc_update)
    {
      s->crc = s->crc_update (s->opaque, s->crc, s->output,
                              s->output_count) & 0xffffffffUL;
    }
  else
    {
      s->crc = xb_crc_fallback (s->crc, s->output, s->output_count);
    }
}

static int
xb_emit (xb_state *s, unsigned int ch)
{
  if (s->size_lo == 0xffffffffUL)
    {
      if (s->size_hi == 0xffffffffUL)
        {
          return -1;
        }

      s->size_hi++;
      s->size_lo = 0UL;
    }
  else
    {
      s->size_lo++;
    }

  if (s->size_hi > s->expect_hi
      || (s->size_hi == s->expect_hi && s->size_lo > s->expect_lo))
    {
      return -1;
    }

  s->output[s->output_count++] = (unsigned char)ch;

  if (s->output_count == XB_OUTBUF)
    {
      xb_crc_buffer (s);
      if (s->writer (s->opaque, s->output, s->output_count))
        {
          s->write_failed = 1;
          return -1;
        }

      s->output_count = 0U;
    }

  return 0;
}

/*
 * Decode BWT block (caller has consumed 0x31 marker)
 */

static int
xb_block (xb_state *s)
{
  unsigned int origin, block_used = 0U, cumul[257], freq[256], back, j;
  int groups, num_selectors, used = 0, alphabet;
  int i, k, group = -1, remaining = 0, tree_id = 0, mtf_sel[XB_GROUPS];
  int lengths[XB_SYMBOLS], length, symbol, run, run_mul, idx, tmp;
  int last_byte = -1, same_count = 0;

  if (xb_bit (s) != 0)
    {
      return -1; /* randomized BZip2 blocks arent allowed */
    }

  origin = xb_bits (s, 24U);

  if (s->bad_input || origin >= s->block_max)
    {
      return -1;
    }

  memset (s->inuse, 0, sizeof (s->inuse));

  for (i = 0; i < 16; i++)
    {
      s->flags[i] = xb_bit (s);
    }

  for (i = 0; i < 16; i++)
    {
      if (s->flags[i])
        {
          for (k = 0; k < 16; k++)
            {
              s->inuse[i * 16 + k] = xb_bit (s);
            }
        }
    }

  if (s->bad_input)
    {
      return -1;
    }

  for (i = 0; i < 256; i++)
    {
      if (s->inuse[i])
        {
          s->symtab[used++] = i;
        }
    }

  if (!used)
    {
      return -1;
    }

  alphabet = used + 2;
  groups = (int)xb_bits (s, 3U);
  num_selectors = (int)xb_bits (s, 15U);

  if (s->bad_input || groups < 2 || groups > XB_GROUPS || num_selectors < 1
      || num_selectors > XB_SELECTORS)
    {
      return -1;
    }

  for (i = 0; i < groups; i++)
    {
      mtf_sel[i] = i;
    }

  for (i = 0; i < num_selectors; i++)
    {
      j = 0U;

      while (xb_bit (s))
        {
          j++;
          if (j >= (unsigned int)groups || s->bad_input)
            {
              return -1;
            }
        }

      if (s->bad_input)
        {
          return -1;
        }

      tmp = mtf_sel[j];

      while (j > 0U)
        {
          mtf_sel[j] = mtf_sel[j - 1U];
          j--;
        }

      mtf_sel[0] = tmp;
      s->selectors[i] = (unsigned char)tmp;
    }

  for (i = 0; i < groups; i++)
    {
      length = (int)xb_bits (s, 5U);

      for (k = 0; k < alphabet; k++)
        {
          while (xb_bit (s))
            {
              if (xb_bit (s))
                {
                  length--;
                }
              else
                {
                  length++;
                }

              if (s->bad_input || length < 1 || length > 20)
                {
                  return -1;
                }
            }

          if (s->bad_input)
            {
              return -1;
            }

          lengths[k] = length;
        }

      if (xb_tree (&s->trees[i], lengths, alphabet))
        {
          return -1;
        }
    }

  for (i = 0; i < used; i++)
    {
      s->mtf[i] = i;
    }

  for (;;)
    {
      if (remaining == 0)
        {
          group++;

          if (group >= num_selectors)
            {
              return -1;
            }

          remaining = 50;
          tree_id = s->selectors[group];
        }

      remaining--;
      symbol = xb_symbol (s, &s->trees[tree_id]);

      if (symbol < 0)
        {
          return -1;
        }

      if (symbol == alphabet - 1)
        {
          break;
        }

      if (symbol == 0 || symbol == 1)
        {
          run = -1;
          run_mul = 1;

          do
            {
              if ((unsigned int)run_mul > s->block_max)
                {
                  return -1;
                }

              run += (symbol == 0) ? run_mul : 2 * run_mul;
              run_mul *= 2;

              if (remaining == 0)
                {
                  group++;

                  if (group >= num_selectors)
                    {
                      return -1;
                    }

                  remaining = 50;
                  tree_id = s->selectors[group];
                }

              remaining--;
              symbol = xb_symbol (s, &s->trees[tree_id]);

              if (symbol < 0)
                {
                  return -1;
                }
            }
          while (symbol == 0 || symbol == 1);

          run++;

          if (run < 0 || (unsigned int)run > s->block_max - block_used)
            {
              return -1;
            }

          memset (s->bwt + block_used, s->symtab[s->mtf[0]], (size_t)run);
          block_used += (unsigned int)run;

          if (symbol == alphabet - 1)
            {
              break;
            }
        }

      idx = symbol - 1;

      if (idx < 0 || idx >= used || block_used >= s->block_max)
        {
          return -1;
        }

      tmp = s->mtf[idx];

      while (idx > 0)
        {
          s->mtf[idx] = s->mtf[idx - 1];
          idx--;
        }

      s->mtf[0] = tmp;
      s->bwt[block_used++] = (unsigned char)s->symtab[tmp];
    }

  if (origin >= block_used)
    {
      return -1;
    }

  memset (freq, 0, sizeof (freq));

  for (j = 0U; j < block_used; j++)
    {
      freq[s->bwt[j]]++;
    }

  cumul[0] = 0U;

  for (j = 0U; j < 256U; j++)
    {
      cumul[j + 1U] = cumul[j] + freq[j];
    }

  for (j = 0U; j < block_used; j++)
    {
      unsigned int ch = s->bwt[j];
      s->tt[cumul[ch]++] = j;
    }

  back = s->tt[origin];

  for (j = 0U; j < block_used; j++)
    {
      unsigned int ch = s->bwt[back];
      back = s->tt[back];

      if (same_count == 4)
        {
          unsigned int n;

          for (n = 0U; n < ch; n++)
            {
              if (xb_emit (s, (unsigned int)last_byte))
                {
                  return -1;
                }
            }

          last_byte = -1;
          same_count = 0;
        }
      else
        {
          if (xb_emit (s, ch))
            {
              return -1;
            }

          if ((int)ch == last_byte)
            {
              same_count++;
            }
          else
            {
              last_byte = (int)ch;
              same_count = 1;
            }
        }
    }

  if (same_count == 4)
    {
      return -1; /* missing RLE1 count byte */
    }

  return 0;
}

int
iz_xceedbwt_decode_crc (int (*read_byte) (void *),
                         int (*write_data) (void *, const unsigned char *,
                                            size_t),
                         unsigned long (*crc_update) (void *, unsigned long,
                                                      const unsigned char *,
                                                      size_t),
                         void *opaque, unsigned long usize_hi,
                         unsigned long usize_lo, unsigned long expected_crc)
{
  xb_state *s;
  unsigned int first, marker, nblocks = 0U;
  int rc = -1;

  if (!read_byte || !write_data || usize_hi > 0xffffffffUL
      || usize_lo > 0xffffffffUL || expected_crc > 0xffffffffUL)
    {
      return -1;
    }

  s = (xb_state *)malloc (sizeof (*s));

  if (!s)
    {
      return -2;
    }

  memset (s, 0, sizeof (*s));
  s->reader = read_byte;
  s->writer = write_data;
  s->opaque = opaque;
  s->crc_update = crc_update;
  s->expect_hi = usize_hi;
  s->expect_lo = usize_lo;
  s->expected_crc = expected_crc;
  s->crc = 0UL;
  first = xb_bits (s, 8U);

  if (s->bad_input || first < (unsigned int)'1' || first > (unsigned int)'9')
    {
      goto done;
    }

  s->block_max = (first - (unsigned int)'0') * 100000U;

  if ((size_t)-1 / sizeof (unsigned int) < 900000UL)
    {
      rc = -2;
      goto done;
    }

  s->bwt = (unsigned char *)malloc ((size_t)s->block_max);
  s->tt
      = (unsigned int *)malloc ((size_t)s->block_max * sizeof (unsigned int));
  if (!s->bwt || !s->tt)
    {
      rc = -2;
      goto done;
    }

  for (;;)
    {
      marker = xb_bits (s, 8U);
      if (s->bad_input)
        {
          goto done;
        }

      if (marker == 0x17U)
        {

          /* Unused trailing bits must be zero */
          if (s->available_bits != 0U
              && (s->current_byte & ((1U << s->available_bits) - 1U)))
            {
              goto done;
            }

          if (s->reader (s->opaque) != -1)
            {
              goto done;
            }

          if (s->output_count)
            {
              xb_crc_buffer (s);
            }

          if (s->crc != s->expected_crc
              || s->size_hi != s->expect_hi || s->size_lo != s->expect_lo)
            {
              goto done;
            }

          if (s->output_count
              && s->writer (s->opaque, s->output, s->output_count))
            {
              s->write_failed = 1;

              goto done;
            }

          rc = 0;

          goto done;
        }

      if (marker != 0x31U || ++nblocks > 500000U)
        {
          goto done;
        }

      if (xb_block (s))
        {
          goto done;
        }
    }

done:
  if (s->write_failed)
    {
      rc = -3;
    }

  free (s->bwt);
  free (s->tt);
  free (s);

  return rc;
}

int
iz_xceedbwt_decode (int (*read_byte) (void *),
                    int (*write_data) (void *, const unsigned char *, size_t),
                    void *opaque, unsigned long usize_hi,
                    unsigned long usize_lo, unsigned long expected_crc)
{
  return iz_xceedbwt_decode_crc (read_byte, write_data, NULL, opaque,
                                 usize_hi, usize_lo, expected_crc);
}
