/*
 * SPDX-License-Identifier: MIT-0
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 */

#ifndef IZ_XCEEDBWT_H
# define IZ_XCEEDBWT_H

# include <stddef.h>

/*
 * Input: 0..255, or -1 at the end of the compressed member.
 * Output: 0 success, nonzero error.
 * Bytes passed to the writer must remain valid until the writer returns.
 * Expected size is split into two 32-bit halves for C89/32-bit compilers.
 * Return 0 on a fully framed, size and CRC-validated Xceed members.
 * -1 on invalid, truncated or non-Xceed input, -2 on allocation failure,
 * -3 if the output callback failed.  -1 does NOT identify IBM TERSE!
 */

int iz_xceedbwt_decode (int (*read_byte) (void *),
                        int (*write_data) (void *, const unsigned char *,
                                           size_t),
                        void *opaque, unsigned long usize_hi,
                        unsigned long usize_lo, unsigned long expected_crc);

int iz_xceedbwt_decode_crc (int (*read_byte) (void *),
                            int (*write_data) (void *, const unsigned char *,
                                               size_t),
                            unsigned long (*crc_update) (void *, unsigned long,
                                                         const unsigned char *,
                                                         size_t),
                            void *opaque, unsigned long usize_hi,
                            unsigned long usize_lo, unsigned long expected_crc);

#endif /* ifndef IZ_XCEEDBWT_H */
