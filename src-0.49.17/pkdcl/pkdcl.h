/*
 * PKDCLX 1.1.2
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 */

#ifndef PKDCL_H_INCLUDED
# define PKDCL_H_INCLUDED

# define PKDCL_CMP_BINARY 0U
# define PKDCL_CMP_ASCII 1U

# define PKDCL_CMP_NO_ERROR 0U
# define PKDCL_CMP_INVALID_DICTSIZE 1U
# define PKDCL_CMP_INVALID_MODE 2U
# define PKDCL_CMP_BAD_DATA 3U
# define PKDCL_CMP_ABORT 4U

# define PKDCL_IMPLODE_WORK_SIZE 35256U
# define PKDCL_EXPLODE_WORK_SIZE 12574U

typedef unsigned short (*pkdcl_read_func) (unsigned char *buffer,
                                           unsigned short *size, void *opaque);

typedef void (*pkdcl_write_func) (unsigned char *buffer, unsigned short *size,
                                  void *opaque);

unsigned short pkdcl_implode (pkdcl_read_func read_func,
                              pkdcl_write_func write_func,
                              unsigned char *work_buffer, void *opaque,
                              unsigned short const *type,
                              unsigned short const *dictionary_size);

unsigned short pkdcl_explode (pkdcl_read_func read_func,
                              pkdcl_write_func write_func,
                              unsigned char *work_buffer, void *opaque);

unsigned long pkdcl_crc32 (const unsigned char *buffer,
                           const unsigned short *size,
                           const unsigned long *old_crc);

# ifdef PKDCL_ENABLE_LEGACY_API
typedef unsigned short (*pkdcl_legacy_read_func) (unsigned char *buffer,
                                                  unsigned short *size);

typedef void (*pkdcl_legacy_write_func) (unsigned char *buffer,
                                         unsigned short *size);

unsigned short implode (pkdcl_legacy_read_func read_func,
                        pkdcl_legacy_write_func write_func,
                        unsigned char *work_buffer, unsigned short *type,
                        unsigned short *dictionary_size);

unsigned short explode (pkdcl_legacy_read_func read_func,
                        pkdcl_legacy_write_func write_func,
                        unsigned char *work_buffer);

unsigned long crc32 (const unsigned char *buffer, const unsigned short *size,
                     const unsigned long *old_crc);
# endif

# define PKDCL_DICT_AUTO 0UL
# define PKDCL_DICT_1K 1024UL
# define PKDCL_DICT_2K 2048UL
# define PKDCL_DICT_4K 4096UL
# define PKDCL_DICT_8K 8192UL
# define PKDCL_DICT_16K 16384UL
# define PKDCL_DICT_32K 32768UL
# define PKDCL_FLAG_EXTRA 1U
# define PKDCL_FLAG_OPTIMAL 2U

unsigned short pkdcl_implode_ex (pkdcl_read_func read_func,
                                 pkdcl_write_func write_func, void *opaque,
                                 unsigned short type,
                                 unsigned long dictionary_size,
                                 unsigned int flags);

unsigned short pkdcl_explode_ex (pkdcl_read_func read_func,
                                 pkdcl_write_func write_func, void *opaque);

#endif
