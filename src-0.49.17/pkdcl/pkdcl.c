/*
 * PKDCLX 1.1.2
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 * SPDX-License-Identifier: MIT-0
 */

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "pkdcl.h"

#if CHAR_BIT != 8
# error pkdcl requires 8-bit bytes
#endif

#if UCHAR_MAX != 255U
# error pkdcl requires an unsigned char range of 0..255
#endif

#if USHRT_MAX != 65535U
# error pkdcl requires a 16-bit unsigned short
#endif

#if ULONG_MAX < 0xffffffffUL
# error pkdcl requires unsigned long to hold at least 32 bits
#endif

#define PKDCL_MAX_REP 0x0204U
#define PKDCL_HASH_COUNT 0x0900U
#define PKDCL_U16_NONE 0xffffU

static const unsigned long crc32_table[256] = {
  0x00000000UL, 0x77073096UL, 0xee0e612cUL, 0x990951baUL, 0x076dc419UL,
  0x706af48fUL, 0xe963a535UL, 0x9e6495a3UL, 0x0edb8832UL, 0x79dcb8a4UL,
  0xe0d5e91eUL, 0x97d2d988UL, 0x09b64c2bUL, 0x7eb17cbdUL, 0xe7b82d07UL,
  0x90bf1d91UL, 0x1db71064UL, 0x6ab020f2UL, 0xf3b97148UL, 0x84be41deUL,
  0x1adad47dUL, 0x6ddde4ebUL, 0xf4d4b551UL, 0x83d385c7UL, 0x136c9856UL,
  0x646ba8c0UL, 0xfd62f97aUL, 0x8a65c9ecUL, 0x14015c4fUL, 0x63066cd9UL,
  0xfa0f3d63UL, 0x8d080df5UL, 0x3b6e20c8UL, 0x4c69105eUL, 0xd56041e4UL,
  0xa2677172UL, 0x3c03e4d1UL, 0x4b04d447UL, 0xd20d85fdUL, 0xa50ab56bUL,
  0x35b5a8faUL, 0x42b2986cUL, 0xdbbbc9d6UL, 0xacbcf940UL, 0x32d86ce3UL,
  0x45df5c75UL, 0xdcd60dcfUL, 0xabd13d59UL, 0x26d930acUL, 0x51de003aUL,
  0xc8d75180UL, 0xbfd06116UL, 0x21b4f4b5UL, 0x56b3c423UL, 0xcfba9599UL,
  0xb8bda50fUL, 0x2802b89eUL, 0x5f058808UL, 0xc60cd9b2UL, 0xb10be924UL,
  0x2f6f7c87UL, 0x58684c11UL, 0xc1611dabUL, 0xb6662d3dUL, 0x76dc4190UL,
  0x01db7106UL, 0x98d220bcUL, 0xefd5102aUL, 0x71b18589UL, 0x06b6b51fUL,
  0x9fbfe4a5UL, 0xe8b8d433UL, 0x7807c9a2UL, 0x0f00f934UL, 0x9609a88eUL,
  0xe10e9818UL, 0x7f6a0dbbUL, 0x086d3d2dUL, 0x91646c97UL, 0xe6635c01UL,
  0x6b6b51f4UL, 0x1c6c6162UL, 0x856530d8UL, 0xf262004eUL, 0x6c0695edUL,
  0x1b01a57bUL, 0x8208f4c1UL, 0xf50fc457UL, 0x65b0d9c6UL, 0x12b7e950UL,
  0x8bbeb8eaUL, 0xfcb9887cUL, 0x62dd1ddfUL, 0x15da2d49UL, 0x8cd37cf3UL,
  0xfbd44c65UL, 0x4db26158UL, 0x3ab551ceUL, 0xa3bc0074UL, 0xd4bb30e2UL,
  0x4adfa541UL, 0x3dd895d7UL, 0xa4d1c46dUL, 0xd3d6f4fbUL, 0x4369e96aUL,
  0x346ed9fcUL, 0xad678846UL, 0xda60b8d0UL, 0x44042d73UL, 0x33031de5UL,
  0xaa0a4c5fUL, 0xdd0d7cc9UL, 0x5005713cUL, 0x270241aaUL, 0xbe0b1010UL,
  0xc90c2086UL, 0x5768b525UL, 0x206f85b3UL, 0xb966d409UL, 0xce61e49fUL,
  0x5edef90eUL, 0x29d9c998UL, 0xb0d09822UL, 0xc7d7a8b4UL, 0x59b33d17UL,
  0x2eb40d81UL, 0xb7bd5c3bUL, 0xc0ba6cadUL, 0xedb88320UL, 0x9abfb3b6UL,
  0x03b6e20cUL, 0x74b1d29aUL, 0xead54739UL, 0x9dd277afUL, 0x04db2615UL,
  0x73dc1683UL, 0xe3630b12UL, 0x94643b84UL, 0x0d6d6a3eUL, 0x7a6a5aa8UL,
  0xe40ecf0bUL, 0x9309ff9dUL, 0x0a00ae27UL, 0x7d079eb1UL, 0xf00f9344UL,
  0x8708a3d2UL, 0x1e01f268UL, 0x6906c2feUL, 0xf762575dUL, 0x806567cbUL,
  0x196c3671UL, 0x6e6b06e7UL, 0xfed41b76UL, 0x89d32be0UL, 0x10da7a5aUL,
  0x67dd4accUL, 0xf9b9df6fUL, 0x8ebeeff9UL, 0x17b7be43UL, 0x60b08ed5UL,
  0xd6d6a3e8UL, 0xa1d1937eUL, 0x38d8c2c4UL, 0x4fdff252UL, 0xd1bb67f1UL,
  0xa6bc5767UL, 0x3fb506ddUL, 0x48b2364bUL, 0xd80d2bdaUL, 0xaf0a1b4cUL,
  0x36034af6UL, 0x41047a60UL, 0xdf60efc3UL, 0xa867df55UL, 0x316e8eefUL,
  0x4669be79UL, 0xcb61b38cUL, 0xbc66831aUL, 0x256fd2a0UL, 0x5268e236UL,
  0xcc0c7795UL, 0xbb0b4703UL, 0x220216b9UL, 0x5505262fUL, 0xc5ba3bbeUL,
  0xb2bd0b28UL, 0x2bb45a92UL, 0x5cb36a04UL, 0xc2d7ffa7UL, 0xb5d0cf31UL,
  0x2cd99e8bUL, 0x5bdeae1dUL, 0x9b64c2b0UL, 0xec63f226UL, 0x756aa39cUL,
  0x026d930aUL, 0x9c0906a9UL, 0xeb0e363fUL, 0x72076785UL, 0x05005713UL,
  0x95bf4a82UL, 0xe2b87a14UL, 0x7bb12baeUL, 0x0cb61b38UL, 0x92d28e9bUL,
  0xe5d5be0dUL, 0x7cdcefb7UL, 0x0bdbdf21UL, 0x86d3d2d4UL, 0xf1d4e242UL,
  0x68ddb3f8UL, 0x1fda836eUL, 0x81be16cdUL, 0xf6b9265bUL, 0x6fb077e1UL,
  0x18b74777UL, 0x88085ae6UL, 0xff0f6a70UL, 0x66063bcaUL, 0x11010b5cUL,
  0x8f659effUL, 0xf862ae69UL, 0x616bffd3UL, 0x166ccf45UL, 0xa00ae278UL,
  0xd70dd2eeUL, 0x4e048354UL, 0x3903b3c2UL, 0xa7672661UL, 0xd06016f7UL,
  0x4969474dUL, 0x3e6e77dbUL, 0xaed16a4aUL, 0xd9d65adcUL, 0x40df0b66UL,
  0x37d83bf0UL, 0xa9bcae53UL, 0xdebb9ec5UL, 0x47b2cf7fUL, 0x30b5ffe9UL,
  0xbdbdf21cUL, 0xcabac28aUL, 0x53b39330UL, 0x24b4a3a6UL, 0xbad03605UL,
  0xcdd70693UL, 0x54de5729UL, 0x23d967bfUL, 0xb3667a2eUL, 0xc4614ab8UL,
  0x5d681b02UL, 0x2a6f2b94UL, 0xb40bbe37UL, 0xc30c8ea1UL, 0x5a05df1bUL,
  0x2d02ef8dUL,
};

#define C_DISTANCE 0x0000U
#define C_OUT_BYTES 0x0002U
#define C_OUT_BITS 0x0004U
#define C_DSIZE_BITS 0x0006U
#define C_DSIZE_MASK 0x0008U
#define C_CTYPE 0x000AU
#define C_DSIZE_BYTES 0x000CU
#define C_DIST_BITS 0x000EU
#define C_DIST_CODES 0x004EU
#define C_NCH_BITS 0x008EU
#define C_NCH_CODES 0x0394U
#define C_HASH_INDEX 0x09A8U
#define C_HASH_SENTINEL 0x1BA8U
#define C_OUT_BUFFER 0x1BAAU
#define C_WORK_BUFFER 0x23ACU
#define C_HASH_OFFSETS 0x45B0U

#define D_CTYPE 0x0002U
#define D_OUTPUT_POS 0x0004U
#define D_DSIZE_BITS 0x0006U
#define D_DSIZE_MASK 0x0008U
#define D_BIT_BUFFER 0x000AU
#define D_EXTRA_BITS 0x000CU
#define D_INPUT_POS 0x000EU
#define D_INPUT_BYTES 0x0010U
#define D_OUT_BUFFER 0x001AU
#define D_INPUT_BUFFER 0x221EU
#define D_DIST_POS 0x2A1EU
#define D_LENGTH_CODES 0x2B1EU
#define D_ASC_TAB0 0x2C1EU
#define D_ASC_TAB1 0x2D1EU
#define D_ASC_TAB2 0x2E1EU
#define D_ASC_TAB3 0x2E9EU
#define D_ASC_BITS 0x2F9EU
#define D_DIST_BITS 0x309EU
#define D_LEN_BITS 0x30DEU
#define D_EXTRA_LEN_BITS 0x30EEU
#define D_LEN_BASE 0x30FEU

static const unsigned char dist_bits[64] = {
  0x02, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
  0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x08,
  0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};

static const unsigned char dist_code[64] = {
  0x03, 0x0d, 0x05, 0x19, 0x09, 0x11, 0x01, 0x3e, 0x1e, 0x2e, 0x0e, 0x36, 0x16,
  0x26, 0x06, 0x3a, 0x1a, 0x2a, 0x0a, 0x32, 0x12, 0x22, 0x42, 0x02, 0x7c, 0x3c,
  0x5c, 0x1c, 0x6c, 0x2c, 0x4c, 0x0c, 0x74, 0x34, 0x54, 0x14, 0x64, 0x24, 0x44,
  0x04, 0x78, 0x38, 0x58, 0x18, 0x68, 0x28, 0x48, 0x08, 0xf0, 0x70, 0xb0, 0x30,
  0xd0, 0x50, 0x90, 0x10, 0xe0, 0x60, 0xa0, 0x20, 0xc0, 0x40, 0x80, 0x00,
};

static const unsigned char extra_len_bits[16] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
};

static const unsigned short len_base0[16] = {
  0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
  0x0008, 0x000a, 0x000e, 0x0016, 0x0026, 0x0046, 0x0086, 0x0106,
};

static const unsigned char len_bits[16] = {
  0x03, 0x02, 0x03, 0x03, 0x04, 0x04, 0x04, 0x05,
  0x05, 0x05, 0x05, 0x06, 0x06, 0x06, 0x07, 0x07,
};

static const unsigned char len_code[16] = {
  0x05, 0x03, 0x01, 0x06, 0x0a, 0x02, 0x0c, 0x14,
  0x04, 0x18, 0x08, 0x30, 0x10, 0x20, 0x40, 0x00,
};

static const unsigned char ascii_bits[256] = {
  0x0b, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x08, 0x07, 0x0c, 0x0c,
  0x07, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
  0x0d, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x04, 0x0a, 0x08, 0x0c, 0x0a, 0x0c, 0x0a,
  0x08, 0x07, 0x07, 0x08, 0x09, 0x07, 0x06, 0x07, 0x08, 0x07, 0x06, 0x07, 0x07,
  0x07, 0x07, 0x08, 0x07, 0x07, 0x08, 0x08, 0x0c, 0x0b, 0x07, 0x09, 0x0b, 0x0c,
  0x06, 0x07, 0x06, 0x06, 0x05, 0x07, 0x08, 0x08, 0x06, 0x0b, 0x09, 0x06, 0x07,
  0x06, 0x06, 0x07, 0x0b, 0x06, 0x06, 0x06, 0x07, 0x09, 0x08, 0x09, 0x09, 0x0b,
  0x08, 0x0b, 0x09, 0x0c, 0x08, 0x0c, 0x05, 0x06, 0x06, 0x06, 0x05, 0x06, 0x06,
  0x06, 0x05, 0x0b, 0x07, 0x05, 0x06, 0x05, 0x05, 0x06, 0x0a, 0x05, 0x05, 0x05,
  0x05, 0x08, 0x07, 0x08, 0x08, 0x0a, 0x0b, 0x0b, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d,
  0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
  0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
  0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
  0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
  0x0c, 0x0c, 0x0c, 0x0d, 0x0c, 0x0d, 0x0d, 0x0d, 0x0c, 0x0d, 0x0d, 0x0d, 0x0c,
  0x0d, 0x0d, 0x0d, 0x0d, 0x0c, 0x0d, 0x0d, 0x0d, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d,
  0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
};

static const unsigned short ascii_code[256] = {
  0x0490, 0x0fe0, 0x07e0, 0x0be0, 0x03e0, 0x0de0, 0x05e0, 0x09e0, 0x01e0,
  0x00b8, 0x0062, 0x0ee0, 0x06e0, 0x0022, 0x0ae0, 0x02e0, 0x0ce0, 0x04e0,
  0x08e0, 0x00e0, 0x0f60, 0x0760, 0x0b60, 0x0360, 0x0d60, 0x0560, 0x1240,
  0x0960, 0x0160, 0x0e60, 0x0660, 0x0a60, 0x000f, 0x0250, 0x0038, 0x0260,
  0x0050, 0x0c60, 0x0390, 0x00d8, 0x0042, 0x0002, 0x0058, 0x01b0, 0x007c,
  0x0029, 0x003c, 0x0098, 0x005c, 0x0009, 0x001c, 0x006c, 0x002c, 0x004c,
  0x0018, 0x000c, 0x0074, 0x00e8, 0x0068, 0x0460, 0x0090, 0x0034, 0x00b0,
  0x0710, 0x0860, 0x0031, 0x0054, 0x0011, 0x0021, 0x0017, 0x0014, 0x00a8,
  0x0028, 0x0001, 0x0310, 0x0130, 0x003e, 0x0064, 0x001e, 0x002e, 0x0024,
  0x0510, 0x000e, 0x0036, 0x0016, 0x0044, 0x0030, 0x00c8, 0x01d0, 0x00d0,
  0x0110, 0x0048, 0x0610, 0x0150, 0x0060, 0x0088, 0x0fa0, 0x0007, 0x0026,
  0x0006, 0x003a, 0x001b, 0x001a, 0x002a, 0x000a, 0x000b, 0x0210, 0x0004,
  0x0013, 0x0032, 0x0003, 0x001d, 0x0012, 0x0190, 0x000d, 0x0015, 0x0005,
  0x0019, 0x0008, 0x0078, 0x00f0, 0x0070, 0x0290, 0x0410, 0x0010, 0x07a0,
  0x0ba0, 0x03a0, 0x0240, 0x1c40, 0x0c40, 0x1440, 0x0440, 0x1840, 0x0840,
  0x1040, 0x0040, 0x1f80, 0x0f80, 0x1780, 0x0780, 0x1b80, 0x0b80, 0x1380,
  0x0380, 0x1d80, 0x0d80, 0x1580, 0x0580, 0x1980, 0x0980, 0x1180, 0x0180,
  0x1e80, 0x0e80, 0x1680, 0x0680, 0x1a80, 0x0a80, 0x1280, 0x0280, 0x1c80,
  0x0c80, 0x1480, 0x0480, 0x1880, 0x0880, 0x1080, 0x0080, 0x1f00, 0x0f00,
  0x1700, 0x0700, 0x1b00, 0x0b00, 0x1300, 0x0da0, 0x05a0, 0x09a0, 0x01a0,
  0x0ea0, 0x06a0, 0x0aa0, 0x02a0, 0x0ca0, 0x04a0, 0x08a0, 0x00a0, 0x0f20,
  0x0720, 0x0b20, 0x0320, 0x0d20, 0x0520, 0x0920, 0x0120, 0x0e20, 0x0620,
  0x0a20, 0x0220, 0x0c20, 0x0420, 0x0820, 0x0020, 0x0fc0, 0x07c0, 0x0bc0,
  0x03c0, 0x0dc0, 0x05c0, 0x09c0, 0x01c0, 0x0ec0, 0x06c0, 0x0ac0, 0x02c0,
  0x0cc0, 0x04c0, 0x08c0, 0x00c0, 0x0f40, 0x0740, 0x0b40, 0x0340, 0x0300,
  0x0d40, 0x1d00, 0x0d00, 0x1500, 0x0540, 0x0500, 0x1900, 0x0900, 0x0940,
  0x1100, 0x0100, 0x1e00, 0x0e00, 0x0140, 0x1600, 0x0600, 0x1a00, 0x0e40,
  0x0640, 0x0a40, 0x0a00, 0x1200, 0x0200, 0x1c00, 0x0c00, 0x1400, 0x0400,
  0x1800, 0x0800, 0x1000, 0x0000,
};

struct cmp_state
{
  unsigned char *w;
  pkdcl_read_func read_func;
  pkdcl_write_func write_func;
  void *opaque;
};

struct dec_state
{
  unsigned char *w;
  pkdcl_read_func read_func;
  pkdcl_write_func write_func;
  void *opaque;
};

static unsigned short
get16 (const unsigned char *p)
{
  return (unsigned short)((unsigned short)p[0]
                          | (unsigned short)((unsigned short)p[1] << 8));
}

static void
put16 (unsigned char *p, unsigned short v)
{
  p[0] = (unsigned char)(v & 0xffU);
  p[1] = (unsigned char)((v >> 8) & 0xffU);
}

static unsigned short
c_get (const struct cmp_state *s, unsigned int off)
{
  return get16 (s->w + off);
}

static void
c_put (struct cmp_state *s, unsigned int off, unsigned short v)
{
  put16 (s->w + off, v);
}

static unsigned short
hash_index_get (const struct cmp_state *s, unsigned int hash)
{
  return get16 (s->w + C_HASH_INDEX + hash * 2U);
}

static void
hash_index_put (struct cmp_state const *s, unsigned int hash, unsigned short v)
{
  put16 (s->w + C_HASH_INDEX + hash * 2U, v);
}

static unsigned short
hash_offset_get (struct cmp_state const *s, unsigned int index)
{
  return get16 (s->w + C_HASH_OFFSETS + index * 2U);
}

static void
hash_offset_put (struct cmp_state const *s, unsigned int index,
                 unsigned short v)
{
  put16 (s->w + C_HASH_OFFSETS + index * 2U, v);
}

static unsigned int
pair_hash (const unsigned char *p)
{
  return (unsigned int)p[0] * 8U + (unsigned int)p[1];
}

static unsigned char *
c_stream_buffer (struct cmp_state const *s)
{
  return s->w + C_WORK_BUFFER;
}

static unsigned char *
c_output_buffer (struct cmp_state const *s)
{
  return s->w + C_OUT_BUFFER;
}

static unsigned char
c_nbits (const struct cmp_state *s, unsigned int index)
{
  return s->w[C_NCH_BITS + index];
}

static unsigned short
c_ncode (const struct cmp_state *s, unsigned int index)
{
  return get16 (s->w + C_NCH_CODES + index * 2U);
}

static void
c_set_nbits (struct cmp_state *s, unsigned int index, unsigned char value)
{
  s->w[C_NCH_BITS + index] = value;
}

static void
c_set_ncode (struct cmp_state const *s, unsigned int index,
             unsigned short value)
{
  put16 (s->w + C_NCH_CODES + index * 2U, value);
}

static void
flush_compressed (struct cmp_state *s)
{
  unsigned char *out;
  unsigned short out_bytes;
  unsigned short out_bits;
  unsigned short size;
  unsigned char keep_at_800;
  unsigned char keep_tail;

  out = c_output_buffer (s);
  out_bytes = c_get (s, C_OUT_BYTES);
  out_bits = c_get (s, C_OUT_BITS);
  size = 0x0800U;

  s->write_func (out, &size, s->opaque);

  keep_at_800 = out[0x0800U];
  keep_tail = out[out_bytes];
  out_bytes = (unsigned short)(out_bytes - 0x0800U);
  c_put (s, C_OUT_BYTES, out_bytes);

  (void)memset (out, 0, 0x0802U);

  if (out_bytes != 0U)
    {
      out[0] = keep_at_800;
    }

  if (out_bits != 0U)
    {
      out[out_bytes] = keep_tail;
    }
}

static void
output_bits (struct cmp_state *s, unsigned int nbits, unsigned long bits)
{
  unsigned char *out;
  unsigned short out_bytes;
  unsigned short out_bits;
  unsigned int old_bits;

  if (nbits > 8U)
    {
      output_bits (s, 8U, bits);
      bits >>= 8;
      nbits -= 8U;
    }

  out = c_output_buffer (s);
  out_bytes = c_get (s, C_OUT_BYTES);
  out_bits = c_get (s, C_OUT_BITS);
  old_bits = (unsigned int)out_bits;

  out[out_bytes]
      = (unsigned char)(out[out_bytes] | (unsigned char)(bits << old_bits));
  out_bits = (unsigned short)(out_bits + (unsigned short)nbits);

  if (out_bits > 8U)
    {
      out_bytes++;
      bits >>= (8U - old_bits);
      out[out_bytes] = (unsigned char)bits;
      out_bits = (unsigned short)(out_bits & 7U);
    }
  else
    {
      out_bits = (unsigned short)(out_bits & 7U);

      if (out_bits == 0U)
        {
          out_bytes++;
        }
    }

  c_put (s, C_OUT_BYTES, out_bytes);
  c_put (s, C_OUT_BITS, out_bits);

  if (out_bytes >= 0x0800U)
    {
      flush_compressed (s);
    }
}

static void
sort_buffer (struct cmp_state *s, unsigned int begin, unsigned int end)
{
  unsigned char const *buf;
  unsigned int h;
  unsigned int p;
  unsigned int total;

  buf = c_stream_buffer (s);
  (void)memset (s->w + C_HASH_INDEX, 0, 0x1200U);

  for (p = begin; p < end; p++)
    {
      unsigned short count;

      h = pair_hash (buf + p);
      count = hash_index_get (s, h);
      hash_index_put (s, h, (unsigned short)(count + 1U));
    }

  total = 0U;

  for (h = 0U; h < PKDCL_HASH_COUNT; h++)
    {
      total += (unsigned int)hash_index_get (s, h);
      hash_index_put (s, h, (unsigned short)total);
    }

  put16 (s->w + C_HASH_SENTINEL, (unsigned short)total);

  p = end;

  while (p != begin)
    {
      unsigned short idx;

      p--;
      h = pair_hash (buf + p);
      idx = (unsigned short)(hash_index_get (s, h) - 1U);
      hash_index_put (s, h, idx);
      hash_offset_put (s, (unsigned int)idx, (unsigned short)p);
    }
}

static unsigned int
find_repetition (struct cmp_state *s, unsigned int input_off)
{
  unsigned char const *buf;
  unsigned short prefix[PKDCL_MAX_REP + 1U];
  unsigned int hash;
  unsigned int list_index;
  unsigned int candidate;
  unsigned int limit;
  unsigned int minimum;
  unsigned int rep_length;
  unsigned int equal_count;
  unsigned int candidate_end;
  unsigned int rep2;
  unsigned int prefix_built;
  unsigned int prefix_value;
  unsigned int dsize;

  buf = c_stream_buffer (s);
  dsize = (unsigned int)c_get (s, C_DSIZE_BYTES);
  hash = pair_hash (buf + input_off);
  list_index = (unsigned int)hash_index_get (s, hash);
  minimum = input_off - dsize + 1U;

  while ((unsigned int)hash_offset_get (s, list_index) < minimum)
    {
      list_index++;
    }

  hash_index_put (s, hash, (unsigned short)list_index);

  candidate = (unsigned int)hash_offset_get (s, list_index);
  limit = input_off - 1U;

  if (candidate >= limit)
    {
      return 0U;
    }

  rep_length = 1U;

  for (;;)
    {
      if (buf[input_off] == buf[candidate]
          && buf[input_off + rep_length - 1U]
                 == buf[candidate + rep_length - 1U])
        {
          equal_count = 2U;

          while (equal_count < PKDCL_MAX_REP
                 && buf[candidate + equal_count]
                        == buf[input_off + equal_count])
            {
              equal_count++;
            }

          if (equal_count >= rep_length)
            {
              c_put (s, C_DISTANCE,
                     (unsigned short)(input_off - candidate - 1U));
              rep_length = equal_count;

              if (rep_length > 10U)
                {
                  break;
                }
            }
        }

      list_index++;
      candidate = (unsigned int)hash_offset_get (s, list_index);

      if (candidate >= limit)
        {
          return rep_length >= 2U ? rep_length : 0U;
        }
    }

  if (equal_count == PKDCL_MAX_REP)
    {
      return equal_count;
    }

  if ((unsigned int)hash_offset_get (s, list_index + 1U) >= limit)
    {
      return rep_length;
    }

  prefix[0] = (unsigned short)PKDCL_U16_NONE;
  prefix[1] = 0U;
  prefix_built = 1U;
  prefix_value = 0U;

  while (prefix_built < rep_length)
    {
      if (buf[input_off + prefix_built] != buf[input_off + prefix_value])
        {
          prefix_value = (unsigned int)prefix[prefix_value];

          if (prefix_value != PKDCL_U16_NONE)
            {
              continue;
            }
        }

      prefix_built++;

      if (prefix_value == PKDCL_U16_NONE)
        {
          prefix_value = 0U;
        }
      else
        {
          prefix_value++;
        }

      prefix[prefix_built] = (unsigned short)prefix_value;
    }

  candidate = (unsigned int)hash_offset_get (s, list_index);
  candidate_end = candidate + rep_length;
  rep2 = rep_length;

  for (;;)
    {
      unsigned char prelast;

      rep2 = (unsigned int)prefix[rep2];

      if (rep2 == PKDCL_U16_NONE)
        {
          rep2 = 0U;
        }

      do
        {
          list_index++;
          candidate = (unsigned int)hash_offset_get (s, list_index);

          if (candidate >= limit)
            {
              return rep_length;
            }
        }
      while (candidate + rep2 < candidate_end);

      prelast = buf[input_off + rep_length - 2U];

      if (prelast == buf[candidate + rep_length - 2U])
        {
          if (candidate + rep2 != candidate_end)
            {
              candidate_end = candidate;
              rep2 = 0U;
            }
        }
      else
        {
          do
            {
              list_index++;
              candidate = (unsigned int)hash_offset_get (s, list_index);

              if (candidate >= limit)
                {
                  return rep_length;
                }
            }
          while (buf[candidate + rep_length - 2U] != prelast
                 || buf[candidate] != buf[input_off]);

          candidate_end = candidate + 2U;
          rep2 = 2U;
        }

      while (buf[candidate_end] == buf[input_off + rep2])
        {
          rep2++;

          if (rep2 >= PKDCL_MAX_REP)
            {
              break;
            }

          candidate_end++;
        }

      if (rep2 >= rep_length)
        {
          c_put (s, C_DISTANCE, (unsigned short)(input_off - candidate - 1U));
          rep_length = rep2;

          if (rep_length == PKDCL_MAX_REP)
            {
              return rep_length;
            }

          while (prefix_built < rep2)
            {
              if (buf[input_off + prefix_built]
                  != buf[input_off + prefix_value])
                {
                  prefix_value = (unsigned int)prefix[prefix_value];

                  if (prefix_value != PKDCL_U16_NONE)
                    {
                      continue;
                    }
                }

              prefix_built++;

              if (prefix_value == PKDCL_U16_NONE)
                {
                  prefix_value = 0U;
                }
              else
                {
                  prefix_value++;
                }

              prefix[prefix_built] = (unsigned short)prefix_value;
            }
        }
    }
}

static void
write_compressed_data (struct cmp_state *s)
{
  unsigned char *buf;
  unsigned char *out;
  unsigned int input_off;
  unsigned int input_end;
  unsigned int ended;
  unsigned int phase;
  unsigned int total_loaded;
  unsigned int rep_length;
  unsigned int saved_rep;
  unsigned short saved_distance;
  unsigned short request;
  unsigned short got;
  unsigned short out_size;
  unsigned int dsize;
  unsigned int dist;

  buf = c_stream_buffer (s);
  out = c_output_buffer (s);
  dsize = (unsigned int)c_get (s, C_DSIZE_BYTES);
  input_off = dsize + PKDCL_MAX_REP;
  ended = 0U;
  phase = 0U;

  out[0] = (unsigned char)c_get (s, C_CTYPE);
  out[1] = (unsigned char)c_get (s, C_DSIZE_BITS);
  c_put (s, C_OUT_BYTES, 2U);
  (void)memset (out + 2U, 0, 0x0800U);
  c_put (s, C_OUT_BITS, 0U);

  while (!ended)
    {
      request = 0x1000U;
      total_loaded = 0U;

      while (request != 0U)
        {
          unsigned short ask;
          ask = request;
          got = s->read_func (buf + dsize + PKDCL_MAX_REP + total_loaded, &ask,
                              s->opaque);
          if (got == 0U)
            {
              if (total_loaded == 0U && phase == 0U)
                {
                  goto finish_stream;
                }

              ended = 1U;

              break;
            }

          total_loaded += (unsigned int)got;
          request = (unsigned short)(request - got);
        }

      input_end = dsize + total_loaded;

      if (ended)
        {
          input_end += PKDCL_MAX_REP;
        }

      if (phase == 0U)
        {
          sort_buffer (s, input_off, input_end + 1U);
          phase++;

          if (dsize != 0x1000U)
            {
              phase++;
            }
        }
      else if (phase == 1U)
        {
          sort_buffer (s, input_off - dsize + PKDCL_MAX_REP, input_end + 1U);
          phase++;
        }
      else
        {
          sort_buffer (s, input_off - dsize, input_end + 1U);
        }

      while (input_off < input_end)
        {
          rep_length = find_repetition (s, input_off);

          while (rep_length != 0U)
            {
              dist = (unsigned int)c_get (s, C_DISTANCE);

              if (rep_length == 2U && dist >= 0x0100U)
                {
                  break;
                }

              if (ended && input_off + rep_length > input_end)
                {
                  rep_length = input_end - input_off;

                  if (rep_length < 2U)
                    {
                      break;
                    }

                  dist = (unsigned int)c_get (s, C_DISTANCE);

                  if (rep_length == 2U && dist >= 0x0100U)
                    {
                      break;
                    }

                  goto emit_match;
                }

              if (rep_length >= 8U || input_off + 1U >= input_end)
                {
                  goto emit_match;
                }

              saved_rep = rep_length;
              saved_distance = c_get (s, C_DISTANCE);
              rep_length = find_repetition (s, input_off + 1U);

              if (rep_length > saved_rep)
                {
                  if (rep_length > saved_rep + 1U
                      || (unsigned int)saved_distance > 0x0080U)
                    {
                      output_bits (s,
                                   (unsigned int)c_nbits (s, buf[input_off]),
                                   (unsigned long)c_ncode (s, buf[input_off]));
                      input_off++;

                      continue;
                    }
                }

              rep_length = saved_rep;
              c_put (s, C_DISTANCE, saved_distance);

            emit_match:
              output_bits (s, (unsigned int)c_nbits (s, rep_length + 0x00FEU),
                           (unsigned long)c_ncode (s, rep_length + 0x00FEU));
              dist = (unsigned int)c_get (s, C_DISTANCE);

              if (rep_length == 2U)
                {
                  output_bits (
                      s, (unsigned int)s->w[C_DIST_BITS + (dist >> 2)],
                      (unsigned long)s->w[C_DIST_CODES + (dist >> 2)]);
                  output_bits (s, 2U, (unsigned long)(dist & 3U));
                }
              else
                {
                  unsigned int bits;
                  unsigned int mask;
                  bits = (unsigned int)c_get (s, C_DSIZE_BITS);
                  mask = (unsigned int)c_get (s, C_DSIZE_MASK);
                  output_bits (
                      s, (unsigned int)s->w[C_DIST_BITS + (dist >> bits)],
                      (unsigned long)s->w[C_DIST_CODES + (dist >> bits)]);
                  output_bits (s, bits, (unsigned long)(dist & mask));
                }

              input_off += rep_length;
              goto next_input_position;
            }

          output_bits (s, (unsigned int)c_nbits (s, buf[input_off]),
                       (unsigned long)c_ncode (s, buf[input_off]));
          input_off++;

        next_input_position:;
        }

      if (!ended)
        {
          input_off -= 0x1000U;
          (void)memmove (buf, buf + 0x1000U, dsize + PKDCL_MAX_REP);
        }
    }

finish_stream:
  output_bits (s, (unsigned int)c_nbits (s, 0x0305U),
               (unsigned long)c_ncode (s, 0x0305U));
  if (c_get (s, C_OUT_BITS) != 0U)
    {
      c_put (s, C_OUT_BYTES, (unsigned short)(c_get (s, C_OUT_BYTES) + 1U));
    }

  out_size = c_get (s, C_OUT_BYTES);
  s->write_func (out, &out_size, s->opaque);
}

unsigned short
pkdcl_implode (pkdcl_read_func read_func, pkdcl_write_func write_func,
               unsigned char *work_buffer, void *opaque,
               unsigned short const *type,
               unsigned short const *dictionary_size)
{
  struct cmp_state s;
  unsigned int i;
  unsigned int j;
  unsigned int index;

  if (read_func == NULL || write_func == NULL || work_buffer == NULL
      || type == NULL || dictionary_size == NULL)
    {
      return PKDCL_CMP_BAD_DATA;
    }

  s.w = work_buffer;
  s.read_func = read_func;
  s.write_func = write_func;
  s.opaque = opaque;

  c_put (&s, C_CTYPE, *type);
  c_put (&s, C_DSIZE_BYTES, *dictionary_size);
  c_put (&s, C_DSIZE_BITS, 4U);
  c_put (&s, C_DSIZE_MASK, 0x000FU);

  if (*dictionary_size == 0x1000U)
    {
      c_put (&s, C_DSIZE_BITS, 6U);
      c_put (&s, C_DSIZE_MASK, 0x003FU);
    }
  else if (*dictionary_size == 0x0800U)
    {
      c_put (&s, C_DSIZE_BITS, 5U);
      c_put (&s, C_DSIZE_MASK, 0x001FU);
    }
  else if (*dictionary_size != 0x0400U)
    {
      return PKDCL_CMP_INVALID_DICTSIZE;
    }

  if (*type == PKDCL_CMP_BINARY)
    {
      for (i = 0U; i < 0x0100U; i++)
        {
          c_set_nbits (&s, i, 9U);
          c_set_ncode (&s, i, (unsigned short)(i * 2U));
        }
    }
  else if (*type == PKDCL_CMP_ASCII)
    {
      for (i = 0U; i < 0x0100U; i++)
        {
          c_set_nbits (&s, i, (unsigned char)(ascii_bits[i] + 1U));
          c_set_ncode (&s, i,
                       (unsigned short)((unsigned int)ascii_code[i] * 2U));
        }
    }
  else
    {
      return PKDCL_CMP_INVALID_MODE;
    }

  index = 0x0100U;

  for (i = 0U; i < 16U; i++)
    {
      unsigned int repeats = 1U << extra_len_bits[i];

      for (j = 0U; j < repeats; j++)
        {
          unsigned int bits = (unsigned int)len_bits[i] + 1U
                              + (unsigned int)extra_len_bits[i];
          unsigned int code = (j << len_bits[i]) | (unsigned int)len_code[i];

          code = code * 2U + 1U;
          c_set_nbits (&s, index, (unsigned char)bits);
          c_set_ncode (&s, index, (unsigned short)code);
          index++;
        }
    }

  (void)memcpy (s.w + C_DIST_CODES, dist_code, 64U);
  (void)memcpy (s.w + C_DIST_BITS, dist_bits, 64U);

  write_compressed_data (&s);

  return PKDCL_CMP_NO_ERROR;
}

static unsigned short
d_get (const struct dec_state *s, unsigned int off)
{
  return get16 (s->w + off);
}

static void
d_put (struct dec_state *s, unsigned int off, unsigned short v)
{
  put16 (s->w + off, v);
}

static void
gen_decode_table (unsigned char *table, const unsigned char *codes,
                  const unsigned char *bits, unsigned int count)
{
  int i;

  for (i = (int)count - 1; i >= 0; i--)
    {
      unsigned int step = 1U << bits[(unsigned int)i];
      unsigned int p = (unsigned int)codes[(unsigned int)i];

      while (p <= 0x00ffU)
        {
          table[p] = (unsigned char)i;
          p += step;
        }
    }
}

static void
gen_ascii_tables (struct dec_state const *s)
{
  int symbol;
  unsigned char *abits;
  unsigned char *t0;
  unsigned char *t1;
  unsigned char *t2;
  unsigned char *t3;

  abits = s->w + D_ASC_BITS;
  t0 = s->w + D_ASC_TAB0;
  t1 = s->w + D_ASC_TAB1;
  t2 = s->w + D_ASC_TAB2;
  t3 = s->w + D_ASC_TAB3;

  for (symbol = 255; symbol >= 0; symbol--)
    {
      unsigned int code = (unsigned int)ascii_code[(unsigned int)symbol];
      unsigned int bits = (unsigned int)abits[(unsigned int)symbol];
      unsigned int acc;
      unsigned int step;

      if (bits <= 8U)
        {
          step = 1U << bits;
          acc = code;

          while (acc < 0x0100U)
            {
              t0[acc] = (unsigned char)symbol;
              acc += step;
            }
        }
      else if ((code & 0x00ffU) != 0U)
        {
          acc = code & 0x00ffU;
          t0[acc] = 0xffU;

          if ((code & 0x003fU) != 0U)
            {
              bits -= 4U;
              abits[(unsigned int)symbol] = (unsigned char)bits;
              step = 1U << bits;
              acc = code >> 4;

              while (acc < 0x0100U)
                {
                  t1[acc] = (unsigned char)symbol;
                  acc += step;
                }
            }
          else
            {
              bits -= 6U;
              abits[(unsigned int)symbol] = (unsigned char)bits;
              step = 1U << bits;
              acc = code >> 6;

              while (acc < 0x0080U)
                {
                  t2[acc] = (unsigned char)symbol;
                  acc += step;
                }
            }
        }
      else
        {
          bits -= 8U;
          abits[(unsigned int)symbol] = (unsigned char)bits;
          step = 1U << bits;
          acc = code >> 8;

          while (acc < 0x0100U)
            {
              t3[acc] = (unsigned char)symbol;
              acc += step;
            }
        }
    }
}

static int
waste_bits (struct dec_state *s, unsigned int need)
{
  unsigned int extra;
  unsigned int bitbuf;
  unsigned int pos;
  unsigned int count;
  unsigned int remaining;

  extra = (unsigned int)d_get (s, D_EXTRA_BITS) & 0xffU;
  bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);

  if (need <= extra)
    {
      bitbuf >>= need;
      extra -= need;
      d_put (s, D_BIT_BUFFER, (unsigned short)bitbuf);
      d_put (s, D_EXTRA_BITS, (unsigned short)extra);

      return 0;
    }

  bitbuf >>= extra;
  remaining = need - extra;

  pos = (unsigned int)d_get (s, D_INPUT_POS);
  count = (unsigned int)d_get (s, D_INPUT_BYTES);

  if (pos >= count)
    {
      unsigned short request = 0x0800U;
      unsigned short got
          = s->read_func (s->w + D_INPUT_BUFFER, &request, s->opaque);

      d_put (s, D_INPUT_BYTES, got);

      if (got == 0U)
        {
          return 1;
        }

      pos = 0U;
      count = (unsigned int)got;
    }

  bitbuf |= (unsigned int)s->w[D_INPUT_BUFFER + pos] << 8;
  pos++;
  bitbuf >>= remaining;
  extra = 8U - remaining;

  d_put (s, D_INPUT_POS, (unsigned short)pos);
  d_put (s, D_INPUT_BYTES, (unsigned short)count);
  d_put (s, D_BIT_BUFFER, (unsigned short)bitbuf);
  d_put (s, D_EXTRA_BITS, (unsigned short)extra);

  return 0;
}

static unsigned int
decode_literal (struct dec_state *s)
{
  unsigned int bitbuf;
  unsigned int value;

  bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);

  if ((bitbuf & 1U) != 0U)
    {
      unsigned int code;
      unsigned int ebits;

      if (waste_bits (s, 1U) != 0)
        {
          return 0x0306U;
        }

      bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);
      code = (unsigned int)s->w[D_LENGTH_CODES + (bitbuf & 0xffU)];

      if (waste_bits (s, (unsigned int)s->w[D_LEN_BITS + code]) != 0)
        {
          return 0x0306U;
        }

      ebits = (unsigned int)s->w[D_EXTRA_LEN_BITS + code];

      if (ebits != 0U)
        {
          unsigned int extra;

          bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);
          extra = bitbuf & ((1U << ebits) - 1U);

          if (waste_bits (s, ebits) != 0)
            {
              if (code + extra != 0x010eU)
                {
                  return 0x0306U;
                }
            }

          code = (unsigned int)get16 (s->w + D_LEN_BASE + code * 2U) + extra;
        }

      return code + 0x0100U;
    }

  if (waste_bits (s, 1U) != 0)
    {
      return 0x0306U;
    }

  if (d_get (s, D_CTYPE) == PKDCL_CMP_BINARY)
    {
      bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);
      value = bitbuf & 0xffU;

      if (waste_bits (s, 8U) != 0)
        {
          return 0x0306U;
        }

      return value;
    }

  bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);

  if ((bitbuf & 0xffU) != 0U)
    {
      value = (unsigned int)s->w[D_ASC_TAB0 + (bitbuf & 0xffU)];

      if (value == 0xffU)
        {
          if ((bitbuf & 0x3fU) != 0U)
            {
              if (waste_bits (s, 4U) != 0)
                {
                  return 0x0306U;
                }

              bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);
              value = (unsigned int)s->w[D_ASC_TAB1 + (bitbuf & 0xffU)];
            }
          else
            {
              if (waste_bits (s, 6U) != 0)
                {
                  return 0x0306U;
                }

              bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);
              value = (unsigned int)s->w[D_ASC_TAB2 + (bitbuf & 0x7fU)];
            }
        }
    }
  else
    {
      if (waste_bits (s, 8U) != 0)
        {
          return 0x0306U;
        }

      bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);
      value = (unsigned int)s->w[D_ASC_TAB3 + (bitbuf & 0xffU)];
    }

  if (waste_bits (s, (unsigned int)s->w[D_ASC_BITS + value]) != 0)
    {
      return 0x0306U;
    }

  return value;
}

static unsigned int
decode_distance (struct dec_state *s, unsigned int rep_length)
{
  unsigned int bitbuf;
  unsigned int code;
  unsigned int bits;
  unsigned int distance;

  bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);
  code = (unsigned int)s->w[D_DIST_POS + (bitbuf & 0xffU)];
  bits = (unsigned int)s->w[D_DIST_BITS + code];

  if (waste_bits (s, bits) != 0)
    {
      return 0U;
    }

  bitbuf = (unsigned int)d_get (s, D_BIT_BUFFER);

  if (rep_length == 2U)
    {
      distance = (code << 2) | (bitbuf & 3U);

      if (waste_bits (s, 2U) != 0)
        {
          return 0U;
        }
    }
  else
    {
      bits = (unsigned int)d_get (s, D_DSIZE_BITS);
      distance
          = (code << bits) | (bitbuf & (unsigned int)d_get (s, D_DSIZE_MASK));

      if (waste_bits (s, bits) != 0)
        {
          return 0U;
        }
    }

  return distance + 1U;
}

static unsigned int
expand_stream (struct dec_state *s)
{
  unsigned char *out;
  unsigned int pos;
  unsigned int result;
  unsigned short size;

  out = s->w + D_OUT_BUFFER;
  pos = 0x1000U;
  d_put (s, D_OUTPUT_POS, (unsigned short)pos);

  for (;;)
    {
      unsigned int token;

      token = decode_literal (s);

      if (token >= 0x0305U)
        {
          result = 0x0305U;

          break;
        }

      if (token >= 0x0100U)
        {
          unsigned int length;
          unsigned int distance;
          unsigned int source;

          length = token - 0x00feU;
          distance = decode_distance (s, length);

          if (distance == 0U)
            {
              result = 0x0306U;

              break;
            }

          source = pos - distance;

          while (length != 0U)
            {
              out[pos++] = out[source++];
              length--;
            }
        }
      else
        {
          out[pos++] = (unsigned char)token;
        }

      if (pos >= 0x2000U)
        {
          size = 0x1000U;
          s->write_func (out + 0x1000U, &size, s->opaque);
          (void)memmove (out, out + 0x1000U, pos - 0x1000U);
          pos -= 0x1000U;
          d_put (s, D_OUTPUT_POS, (unsigned short)pos);
        }
    }

  size = (unsigned short)(pos - 0x1000U);
  s->write_func (out + 0x1000U, &size, s->opaque);
  d_put (s, D_OUTPUT_POS, (unsigned short)pos);

  return result;
}

unsigned short
/* cppcheck-suppress staticFunction */
/* cppcheck-suppress unusedFunction */
pkdcl_explode (pkdcl_read_func read_func, pkdcl_write_func write_func,
               unsigned char *work_buffer, void *opaque)
{
  struct dec_state s;
  unsigned short request;
  unsigned short got;
  unsigned int ctype;
  unsigned int dsize_bits;
  unsigned int result;

  if (read_func == NULL || write_func == NULL || work_buffer == NULL)
    {
      return PKDCL_CMP_BAD_DATA;
    }

  s.w = work_buffer;
  s.read_func = read_func;
  s.write_func = write_func;
  s.opaque = opaque;

  request = 0x0800U;
  got = read_func (work_buffer + D_INPUT_BUFFER, &request, opaque);
  d_put (&s, D_INPUT_BYTES, got);

  if (got <= 4U)
    {
      return PKDCL_CMP_BAD_DATA;
    }

  ctype = (unsigned int)work_buffer[D_INPUT_BUFFER + 0U];
  dsize_bits = (unsigned int)work_buffer[D_INPUT_BUFFER + 1U];
  d_put (&s, D_CTYPE, (unsigned short)ctype);
  d_put (&s, D_DSIZE_BITS, (unsigned short)dsize_bits);
  d_put (&s, D_BIT_BUFFER, (unsigned short)work_buffer[D_INPUT_BUFFER + 2U]);
  d_put (&s, D_EXTRA_BITS, 0U);
  d_put (&s, D_INPUT_POS, 3U);

  if (dsize_bits < 4U || dsize_bits > 6U)
    {
      return PKDCL_CMP_INVALID_DICTSIZE;
    }

  d_put (&s, D_DSIZE_MASK, (unsigned short)((1U << dsize_bits) - 1U));

  if (ctype != PKDCL_CMP_BINARY)
    {
      if (ctype != PKDCL_CMP_ASCII)
        {
          return PKDCL_CMP_INVALID_MODE;
        }

      (void)memcpy (work_buffer + D_ASC_BITS, ascii_bits, 0x0100U);
      gen_ascii_tables (&s);
    }

  (void)memcpy (work_buffer + D_LEN_BITS, len_bits, 16U);
  gen_decode_table (work_buffer + D_LENGTH_CODES, len_code, len_bits, 16U);
  (void)memcpy (work_buffer + D_EXTRA_LEN_BITS, extra_len_bits, 16U);

  for (ctype = 0U; ctype < 16U; ctype++)
    {
      put16 (work_buffer + D_LEN_BASE + ctype * 2U, len_base0[ctype]);
    }

  (void)memcpy (work_buffer + D_DIST_BITS, dist_bits, 64U);
  gen_decode_table (work_buffer + D_DIST_POS, dist_code, dist_bits, 64U);

  result = expand_stream (&s);

  if (result == 0x0306U)
    {
      return PKDCL_CMP_ABORT;
    }

  return PKDCL_CMP_NO_ERROR;
}

unsigned long
pkdcl_crc32 (const unsigned char *buffer, const unsigned short *size,
             const unsigned long *old_crc)
{
  unsigned long crc;
  unsigned int count;

  crc = *old_crc & 0xffffffffUL;
  count = (unsigned int)*size;

  while (count != 0U)
    {
      unsigned int index;

      index = (unsigned int)((crc ^ (unsigned long)*buffer) & 0xffUL);
      crc = ((crc >> 8) ^ crc32_table[index]) & 0xffffffffUL;
      buffer++;
      count--;
    }

  return crc;
}

#ifdef PKDCL_ENABLE_LEGACY_API
static pkdcl_legacy_read_func legacy_read_callback;
static pkdcl_legacy_write_func legacy_write_callback;

static unsigned short
legacy_read_adapter (unsigned char *buffer, unsigned short *size, void *opaque)
{
  (void)opaque;

  return legacy_read_callback (buffer, size);
}

static void
legacy_write_adapter (unsigned char *buffer, unsigned short *size,
                      void *opaque)
{
  (void)opaque;
  legacy_write_callback (buffer, size);
}

unsigned short
/* cppcheck-suppress staticFunction */
/* cppcheck-suppress unusedFunction */
implode (pkdcl_legacy_read_func read_func, pkdcl_legacy_write_func write_func,
         unsigned char *work_buffer, unsigned short const *type,
         unsigned short const *dictionary_size)
{
  unsigned short result;

  legacy_read_callback = read_func;
  legacy_write_callback = write_func;
  result = pkdcl_implode (legacy_read_adapter, legacy_write_adapter,
                          work_buffer, NULL, type, dictionary_size);
  legacy_read_callback = NULL;
  legacy_write_callback = NULL;

  return result;
}

unsigned short
/* cppcheck-suppress staticFunction */
/* cppcheck-suppress unusedFunction */
explode (pkdcl_legacy_read_func read_func, pkdcl_legacy_write_func write_func,
         unsigned char *work_buffer)
{
  unsigned short result;

  legacy_read_callback = read_func;
  legacy_write_callback = write_func;
  result = pkdcl_explode (legacy_read_adapter, legacy_write_adapter,
                          work_buffer, NULL);
  legacy_read_callback = NULL;
  legacy_write_callback = NULL;

  return result;
}

unsigned long
/* cppcheck-suppress staticFunction */
/* cppcheck-suppress unusedFunction */
crc32 (const unsigned char *buffer, const unsigned short *size,
       const unsigned long *old_crc)
{
  return pkdcl_crc32 (buffer, size, old_crc);
}
#endif /* ifdef PKDCL_ENABLE_LEGACY_API */

struct xbuf
{
  unsigned char *p;
  unsigned long n, cap, pos; /* cppcheck-suppress unusedStructMember */
};

struct xbits
{
  struct xbuf b;
  unsigned long bitpos;
  int oom;
};

static int
x_ulong_add (unsigned long a, unsigned long b, unsigned long *out)
{
  if (a > ULONG_MAX - b)
    {
      return 0;
    }

  *out = a + b;

  return 1;
}

static int
x_ulong_to_size (unsigned long v, size_t *out)
{
  size_t z = (size_t)v;

  if ((unsigned long)z != v) /* //-V547 */
    {
      return 0;
    }

  *out = z;

  return 1;
}

static void *
x_malloc_array (unsigned long count, size_t elem_size)
{
  size_t n;

  if (!x_ulong_to_size (count, &n))
    {
      return NULL;
    }

  if (elem_size != 0U && n > (size_t)-1 / elem_size)
    {
      return NULL;
    }

  return malloc (n * elem_size);
}

static void *
x_calloc_array (unsigned long count, size_t elem_size)
{
  size_t n;

  if (!x_ulong_to_size (count, &n))
    {
      return NULL;
    }

  if (elem_size != 0U && n > (size_t)-1 / elem_size)
    {
      return NULL;
    }

  return calloc (n, elem_size);
}

static int
xb_grow (struct xbuf *b, unsigned long need)
{
  unsigned long nc;
  unsigned char *p;

  if (need <= b->cap)
    {
      return 1;
    }

  nc = b->cap ? b->cap : 4096UL;

  while (nc < need)
    {
      if (nc > (~0UL) / 2UL)
        {
          nc = need;

          break;
        }

      nc *= 2UL;
    }

  {
    size_t alloc_size;

    if (!x_ulong_to_size (nc, &alloc_size))
      {
        return 0;
      }

    p = (unsigned char *)realloc (b->p, alloc_size);
  }

  if (p == NULL)
    {
      return 0;
    }

  b->p = p;
  b->cap = nc;

  return 1;
}

static int
xb_read_all (pkdcl_read_func read_func, void *opaque, struct xbuf *b)
{
  unsigned char tmp[32768];

  (void)memset (b, 0, sizeof (*b));

  for (;;)
    {
      unsigned short ask, got;

      ask = (unsigned short)sizeof (tmp);
      got = read_func (tmp, &ask, opaque);

      if (got == 0U)
        {
          break;
        }

      {
        unsigned long need;

        if (!x_ulong_add (b->n, (unsigned long)got, &need)
            || !xb_grow (b, need))
          {
            free (b->p);
            b->p = NULL;
            b->n = 0;
            b->cap = 0;

            return 0;
          }
      }

      (void)memcpy (b->p + b->n, tmp, got);
      b->n += (unsigned long)got;
    }

  return 1;
}

static int
xb_put (struct xbits *x, unsigned int n, unsigned long v)
{
  unsigned int i;

  for (i = 0U; i < n; i++)
    {
      unsigned long byte;

      byte = x->bitpos >> 3;

      if (!xb_grow (&x->b, byte + 1UL))
        {
          x->oom = 1;

          return 0;
        }

      if (byte >= x->b.n)
        {
          x->b.p[byte] = 0;
          x->b.n = byte + 1UL;
        }

      if ((v >> i) & 1UL)
        {
          x->b.p[byte] |= (unsigned char)(1U << (x->bitpos & 7UL));
        }

      if (x->bitpos == ULONG_MAX)
        {
          x->oom = 1;

          return 0;
        }

      x->bitpos++;
    }

  return 1;
}

static unsigned int
x_lit_bits (unsigned short type, unsigned int ch)
{
  return type == PKDCL_CMP_BINARY ? 9U : (unsigned int)ascii_bits[ch] + 1U;
}

static void
x_len_code (unsigned int len, unsigned int *nb, unsigned long *code)
{
  unsigned int i, base;

  base = 0U;

  for (i = 0U; i < 16U; i++)
    {
      unsigned int span;

      span = 1U << extra_len_bits[i];

      if (len - 2U < base + span)
        {
          unsigned int j;

          j = (len - 2U) - base;
          *nb = (unsigned int)len_bits[i] + 1U
                + (unsigned int)extra_len_bits[i];
          *code
              = ((unsigned long)j << len_bits[i]) | (unsigned long)len_code[i];
          *code = *code * 2UL + 1UL;

          return;
        }

      base += span;
    }

  *nb = 0U;
  *code = 0UL;
}

static unsigned int
x_match_bits (unsigned int len, unsigned long dist, unsigned int dbits)
{
  unsigned int nb, group, dz;
  unsigned long code;

  x_len_code (len, &nb, &code);
  dz = (unsigned int)(dist - 1UL);

  if (len == 2U)
    {
      group = dz >> 2;
    }
  else
    {
      group = dz >> dbits;
    }

  return nb + (unsigned int)dist_bits[group] + (len == 2U ? 2U : dbits);
}

static void
x_emit_lit (struct xbits *x, unsigned short type, unsigned int ch)
{
  if (type == PKDCL_CMP_BINARY)
    {
      (void)xb_put (x, 9U, (unsigned long)ch * 2UL);
    }
  else
    {
      (void)xb_put (x, (unsigned int)ascii_bits[ch] + 1U,
                    (unsigned long)ascii_code[ch] * 2UL);
    }
}

static void
x_emit_match (struct xbits *x, unsigned int len, unsigned long dist,
              unsigned int dbits)
{
  unsigned int nb, group, dz;
  unsigned long code;

  x_len_code (len, &nb, &code);
  (void)xb_put (x, nb, code);
  dz = (unsigned int)(dist - 1UL);

  if (len == 2U)
    {
      group = dz >> 2;
      (void)xb_put (x, dist_bits[group], dist_code[group]);
      (void)xb_put (x, 2U, dz & 3U);
    }
  else
    {
      group = dz >> dbits;
      (void)xb_put (x, dist_bits[group], dist_code[group]);
      (void)xb_put (x, dbits, dz & ((1U << dbits) - 1U));
    }
}

static unsigned int
x_hash (const unsigned char *p)
{
  return (unsigned int)p[0] * 8U + (unsigned int)p[1];
}

static void
x_best (const unsigned char *in, unsigned long n, unsigned long pos,
        unsigned long dictionary_size, const unsigned long *prev,
        const unsigned long *head, unsigned int dbits, int deep,
        unsigned int *bestlen, unsigned long *bestdist)
{
  unsigned long q;
  unsigned long remaining;
  unsigned int bl, bcost, seen, maxseen, h;

  *bestlen = 0U;
  *bestdist = 0UL;

  if (pos >= n)
    {
      return;
    }

  remaining = n - pos;

  if (remaining < 2UL)
    {
      return;
    }

  h = x_hash (in + pos);
  q = head[h];
  bl = 0U;
  bcost = ~0U;
  seen = 0U;
  maxseen = deep ? 4096U : 96U;

  while (q != 0UL && seen++ < maxseen)
    {
      unsigned long d;
      unsigned int l;

      q--;

      if (q >= pos)
        {
          break;
        }

      d = pos - q;

      if (d > dictionary_size)
        {
          break;
        }

      l = 0U;

      while (l < 518U && (unsigned long)l < remaining
             && in[q + (unsigned long)l] == in[pos + (unsigned long)l])
        {
          l++;
        }

      if (l >= 2U)
        {
          unsigned int cost;

          if (l == 2U && d > 256UL)
            {
              q = prev[q];

              continue;
            }

          cost = x_match_bits (l, d, dbits);

          if (l > bl || (l == bl && cost < bcost))
            {
              bl = l;
              bcost = cost;
              *bestdist = d;
            }
        }

      q = prev[q];
    }

  *bestlen = bl;
}

static int
x_optimal_encode (const unsigned char *in, unsigned long n, unsigned short type,
                  unsigned long dictionary_size, struct xbuf *out)
{
  struct xbits x;
  unsigned long *prev, *head, pos;
  unsigned int dbits, h;
  unsigned long *cost;
  unsigned short *mlen;
  unsigned long *mdist;
  unsigned long *path;
  unsigned long i, p_count, curr;

  (void)memset (&x, 0, sizeof (x));
  dbits = 0U;

  for (i = dictionary_size; i > 64UL; i >>= 1)
    {
      dbits++;
    }

  if (dbits < 4U || dbits > 9U)
    {
      return 0;
    }

  if (n == ULONG_MAX)
    return 0;

  prev = (unsigned long *)x_malloc_array (n ? n : 1UL,
                                           sizeof (unsigned long));
  head = (unsigned long *)x_calloc_array (PKDCL_HASH_COUNT,
                                           sizeof (unsigned long));
  cost = (unsigned long *)x_malloc_array (n + 1UL,
                                           sizeof (unsigned long));
  mlen = (unsigned short *)x_malloc_array (n + 1UL,
                                            sizeof (unsigned short));
  mdist = (unsigned long *)x_malloc_array (n + 1UL,
                                            sizeof (unsigned long));
  path = (unsigned long *)x_malloc_array (n + 1UL,
                                           sizeof (unsigned long));

  if (prev == NULL || head == NULL || cost == NULL || mlen == NULL
      || mdist == NULL || path == NULL)
    {
      free (prev);
      free (head);
      free (cost);
      free (mlen);
      free (mdist);
      free (path);
      free (x.b.p);

      return 0;
    }

  for (pos = 0UL; pos <= n; pos++)
    {
      cost[pos] = ~0UL;
    }

  cost[0] = 0UL;

  for (pos = 0UL; pos < n; pos++)
    {
      unsigned long c;
      unsigned long remaining;

      remaining = n - pos;

      if (cost[pos] == ~0UL)
        {
          continue;
        }

      {
        unsigned long add = (unsigned long)x_lit_bits (type, in[pos]);

        c = cost[pos] > ULONG_MAX - add ? ULONG_MAX : cost[pos] + add;
      }

      if (c < cost[pos + 1UL])
        {
          cost[pos + 1UL] = c;
          mlen[pos + 1UL] = 0U;
        }

      if (remaining > 1UL)
        {
          unsigned long q;
          unsigned int seen;

          h = x_hash (in + pos);
          q = head[h];
          seen = 0U;

          while (q != 0UL && seen++ < 4096U)
            {
              unsigned long d;
              unsigned int l;

              q--;

              if (q >= pos)
                {
                  break;
                }

              d = pos - q;

              if (d > dictionary_size)
                {
                  break;
                }

              l = 0U;

              while (l < 518U && (unsigned long)l < remaining
                     && in[q + (unsigned long)l] == in[pos + (unsigned long)l])
                {
                  l++;
                }

              if (l >= 2U)
                {
                  unsigned int cl;
                  unsigned int min_l = (d > 256UL) ? 3U : 2U;

                  for (cl = min_l; cl <= l; cl++)
                    {
                      unsigned long add =
                          (unsigned long)x_match_bits (cl, d, dbits);
                      unsigned long mcost =
                          cost[pos] > ULONG_MAX - add ? ULONG_MAX
                                                     : cost[pos] + add;

                      if (mcost < cost[pos + cl])
                        {
                          cost[pos + cl] = mcost;
                          mlen[pos + cl] = (unsigned short)cl;
                          mdist[pos + cl] = d;
                        }
                    }

                  if (l >= 518U)
                    {
                      break;
                    }
                }

              q = prev[q];
            }

          prev[pos] = head[h];
          head[h] = pos + 1UL;
        }
      else
        {
          prev[pos] = 0UL;
        }
    }

  p_count = 0UL;
  curr = n;

  while (curr > 0UL)
    {
      path[p_count++] = curr;

      if (mlen[curr] == 0U)
        {
          curr -= 1UL;
        }
      else
        {
          curr -= (unsigned long)mlen[curr];
        }
    }

  (void)xb_put (&x, 8U, type);
  (void)xb_put (&x, 8U, dbits);

  for (i = p_count; i > 0UL; i--)
    {
      unsigned long end_pos = path[i - 1UL];
      unsigned long start_pos;

      if (i == p_count)
        {
          start_pos = 0UL;
        }
      else
        {
          start_pos = path[i];
        }

      if (mlen[end_pos] == 0U)
        {
          x_emit_lit (&x, type, in[start_pos]);
        }
      else
        {
          x_emit_match (&x, (unsigned int)mlen[end_pos], mdist[end_pos], dbits);
        }
    }

  {
    unsigned int lb;
    unsigned long lc;

    x_len_code (519U, &lb, &lc);
    (void)xb_put (&x, lb, lc);
  }

  free (prev);
  free (head);
  free (cost);
  free (mlen);
  free (mdist);
  free (path);

  if (x.oom)
    {
      free (x.b.p);

      return 0;
    }

  *out = x.b;

  return 1;
}

static int
x_encode (const unsigned char *in, unsigned long n, unsigned short type,
          unsigned long dictionary_size, int deep, struct xbuf *out)
{
  struct xbits x;
  unsigned long *prev, *head, pos, i;
  unsigned int dbits, h, l;
  unsigned long d;
  unsigned int lb;
  unsigned long lc;

  (void)memset (&x, 0, sizeof (x));
  dbits = 0U;

  for (i = dictionary_size; i > 64UL; i >>= 1)
    {
      dbits++;
    }

  if (dbits < 4U || dbits > 9U)
    {
      return 0;
    }

  (void)xb_put (&x, 8U, type);
  (void)xb_put (&x, 8U, dbits);
  prev = (unsigned long *)x_malloc_array (n ? n : 1UL,
                                           sizeof (unsigned long));
  head = (unsigned long *)x_calloc_array (PKDCL_HASH_COUNT,
                                           sizeof (unsigned long));

  if (prev == NULL || head == NULL)
    {
      free (prev);
      free (head);
      free (x.b.p);

      return 0;
    }

  pos = 0UL;

  while (pos < n)
    {
      x_best (in, n, pos, dictionary_size, prev, head, dbits, deep, &l, &d);

      if (l >= 2U)
        {
          unsigned int mb;

          lb = 0U;

          for (i = 0UL; i < (unsigned long)l; i++)
            {
              lb += x_lit_bits (type, in[pos + i]);
            }

          mb = x_match_bits (l, d, dbits);

          if (mb >= lb)
            {
              l = 0U;
            }
        }

      if (l >= 2U)
        {
          x_emit_match (&x, l, d, dbits);

          for (i = 0UL; i < (unsigned long)l; i++)
            {
              if (pos + i < n - 1UL)
                {
                  h = x_hash (in + pos + i);
                  prev[pos + i] = head[h];
                  head[h] = pos + i + 1UL;
                }
              else
                {
                  prev[pos + i] = 0UL;
                }
            }

          pos += (unsigned long)l;
        }
      else
        {
          x_emit_lit (&x, type, in[pos]);

          if (pos < n - 1UL)
            {
              h = x_hash (in + pos);
              prev[pos] = head[h];
              head[h] = pos + 1UL;
            }
          else
            {
              prev[pos] = 0UL;
            }

          pos++;
        }

      if (x.oom)
        {
          free (prev);
          free (head);
          free (x.b.p);

          return 0;
        }
    }

  x_len_code (519U, &lb, &lc);
  (void)xb_put (&x, lb, lc);
  free (prev);
  free (head);

  if (x.oom)
    {
      free (x.b.p);
      return 0;
    }

  *out = x.b;

  return 1;
}

struct xmio
{
  const unsigned char *in;
  unsigned long n, pos;
  struct xbuf out;
  int oom;
};

static unsigned short
/* cppcheck-suppress constParameterCallback */
xm_read (unsigned char *p, unsigned short *sz, void *opaque)
{
  struct xmio *m = (struct xmio *)opaque;
  unsigned long left = m->n - m->pos;
  unsigned short k = *sz;

  if (left < (unsigned long)k)
    {
      k = (unsigned short)left;
    }

  if (k)
    {
      (void)memcpy (p, m->in + m->pos, k);
      m->pos += k;
    }

  return k;
}

static void
/* cppcheck-suppress constParameterCallback */
xm_write (unsigned char *p, unsigned short *sz, void *opaque)
{
  struct xmio *m = (struct xmio *)opaque;

  {
    unsigned long need;

    if (!x_ulong_add (m->out.n, (unsigned long)*sz, &need)
        || !xb_grow (&m->out, need))
      {
        m->oom = 1;
        return;
      }

    (void)memcpy (m->out.p + m->out.n, p, *sz);
    m->out.n = need;
  }
}

static int
x_legacy_encode (const unsigned char *in, unsigned long n, unsigned short type,
                 unsigned long dictionary_size, struct xbuf *out)
{
  struct xmio m;
  unsigned char *w;
  unsigned short ds = (unsigned short)dictionary_size, r;

  (void)memset (&m, 0, sizeof (m));
  m.in = in;
  m.n = n;
  w = (unsigned char *)malloc (PKDCL_IMPLODE_WORK_SIZE);

  if (!w)
    {
      return 0;
    }

  r = pkdcl_implode (xm_read, xm_write, w, &m, &type, &ds);
  free (w);

  if (r != 0U || m.oom)
    {
      free (m.out.p);

      return 0;
    }

  *out = m.out;

  return 1;
}

static void
x_write_all (pkdcl_write_func write_func, void *opaque, const unsigned char *p,
             unsigned long n)
{
  while (n)
    {
      unsigned short k = n > 65535UL ? 65535U : (unsigned short)n;
      unsigned short z = k;

      write_func ((unsigned char *)p, &z, opaque);
      p += k;
      n -= k;
    }
}

unsigned short
pkdcl_implode_ex (pkdcl_read_func read_func, pkdcl_write_func write_func,
                  void *opaque, unsigned short type,
                  unsigned long dictionary_size, unsigned int flags)
{
  struct xbuf in, a, b;
  int ok;

  if (!read_func || !write_func)
    {
      return PKDCL_CMP_BAD_DATA;
    }

  if (type > 1U)
    {
      return PKDCL_CMP_INVALID_MODE;
    }

  if (dictionary_size != PKDCL_DICT_AUTO
      && dictionary_size != 1024UL && dictionary_size != 2048UL
      && dictionary_size != 4096UL && dictionary_size != 8192UL
      && dictionary_size != 16384UL && dictionary_size != 32768UL)
    {
      return PKDCL_CMP_INVALID_DICTSIZE;
    }

  if (!xb_read_all (read_func, opaque, &in))
    {
      return PKDCL_CMP_ABORT;
    }

  if (dictionary_size == PKDCL_DICT_AUTO)
    {
      if (in.n <= PKDCL_DICT_1K)
        dictionary_size = PKDCL_DICT_1K;
      else if (in.n <= PKDCL_DICT_2K)
        dictionary_size = PKDCL_DICT_2K;
      else
        dictionary_size = PKDCL_DICT_4K;
    }

  (void)memset (&a, 0, sizeof (a));
  (void)memset (&b, 0, sizeof (b));
  ok = x_encode (in.p, in.n, type, dictionary_size,
                 (flags & PKDCL_FLAG_EXTRA) != 0U, &a);

  if (!ok)
    {
      free (in.p);

      return PKDCL_CMP_ABORT;
    }

  if ((flags & PKDCL_FLAG_EXTRA) != 0U)
    {
      if (dictionary_size <= 4096UL)
        {
          ok = x_legacy_encode (in.p, in.n, type, dictionary_size, &b);
        }
      else
        {
          ok = x_encode (in.p, in.n, type, dictionary_size, 0, &b);
        }

      if (!ok)
        {
          free (in.p);
          free (a.p);

          return PKDCL_CMP_ABORT;
        }

      if (b.n < a.n)
        {
          free (a.p);
          a = b;
        }
      else
        {
          free (b.p);
        }

      (void)memset (&b, 0, sizeof (b));
    }

  if ((flags & PKDCL_FLAG_OPTIMAL) != 0U)
    {
      ok = x_optimal_encode (in.p, in.n, type, dictionary_size, &b);

      if (!ok)
        {
          free (in.p);
          /* cppcheck-suppress doubleFree */
          free (a.p);

          return PKDCL_CMP_ABORT;
        }

      if (b.n < a.n)
        {
          free (a.p);
          a = b;
        }
      else
        {
          free (b.p);
        }
    }

  x_write_all (write_func, opaque, a.p, a.n);
  /* cppcheck-suppress doubleFree */
  free (a.p);
  free (in.p);

  return PKDCL_CMP_NO_ERROR;
}

struct xr
{
  const unsigned char *p;
  unsigned long n, bit;
};

static int
xr_has_bits (const struct xr *r, unsigned long n)
{
  unsigned long end;
  unsigned long byte;

  if (!x_ulong_add (r->bit, n, &end))
    {
      return 0;
    }

  byte = end >> 3;

  if (byte < r->n)
    {
      return 1;
    }

  if (byte > r->n)
    {
      return 0;
    }

  return (end & 7UL) == 0UL;
}

static int
xr_get (struct xr *r, unsigned int n, unsigned long *v)
{
  unsigned int i;
  unsigned long z = 0UL;

  if (!xr_has_bits (r, (unsigned long)n))
    {
      return 0;
    }

  for (i = 0U; i < n; i++)
    {
      if ((r->p[(r->bit + i) >> 3] >> ((r->bit + i) & 7UL)) & 1U)
        {
          z |= 1UL << i;
        }
    }

  r->bit += n;
  *v = z;

  return 1;
}

static int
xr_code (struct xr *r, const unsigned char *bits, const unsigned char *codes,
         unsigned int count, unsigned int *sym)
{
  unsigned int l, i;

  for (l = 1U; l <= 13U; l++)
    {
      unsigned long v;

      if (!xr_has_bits (r, (unsigned long)l))
        {
          return 0;
        }

      v = 0UL;

      for (i = 0U; i < l; i++)
        {
          if ((r->p[(r->bit + i) >> 3] >> ((r->bit + i) & 7UL)) & 1U)
            {
              v |= 1UL << i;
            }
        }

      for (i = 0U; i < count; i++)
        {
          if (bits[i] == l && (unsigned long)codes[i] == v)
            {
              r->bit += l;
              *sym = i;

              return 1;
            }
        }
    }

  return 0;
}

static int
xr_ascii (struct xr *r, unsigned int *ch)
{
  unsigned int l, i;

  for (l = 1U; l <= 13U; l++)
    {
      unsigned long v;

      if (!xr_has_bits (r, (unsigned long)l))
        {
          return 0;
        }

      v = 0UL;

      for (i = 0U; i < l; i++)
        {
          if ((r->p[(r->bit + i) >> 3] >> ((r->bit + i) & 7UL)) & 1U)
            {
              v |= 1UL << i;
            }
        }

      for (i = 0U; i < 256U; i++)
        {
          if (ascii_bits[i] == l && (unsigned long)ascii_code[i] == v)
            {
              r->bit += l;
              *ch = i;

              return 1;
            }
        }
    }

  return 0;
}

unsigned short
pkdcl_explode_ex (pkdcl_read_func read_func, pkdcl_write_func write_func,
                  void *opaque)
{
  struct xbuf in, out;
  struct xr r;
  unsigned int type, dbits;

  if (!read_func || !write_func)
    {
      return PKDCL_CMP_BAD_DATA;
    }

  if (!xb_read_all (read_func, opaque, &in))
    {
      return PKDCL_CMP_ABORT;
    }

  if (in.n < 2UL)
    {
      free (in.p);

      return PKDCL_CMP_BAD_DATA;
    }

  type = in.p[0];
  dbits = in.p[1];

  if (type > 1U)
    {
      free (in.p);

      return PKDCL_CMP_INVALID_MODE;
    }

  if (dbits < 4U || dbits > 9U)
    {
      free (in.p);

      return PKDCL_CMP_INVALID_DICTSIZE;
    }

  (void)memset (&out, 0, sizeof (out));
  r.p = in.p;
  r.n = in.n;
  r.bit = 16UL;

  for (;;)
    {
      unsigned int sym;
      unsigned long v;
      unsigned char flag;

      if (!xr_get (&r, 1U, &v))
        {
          free (in.p);
          free (out.p);

          return PKDCL_CMP_BAD_DATA;
        }

      flag = (unsigned char)v;

      if (!flag)
        {
          if (type == 0U)
            {
              if (!xr_get (&r, 8U, &v))
                {
                  free (in.p);
                  free (out.p);

                  return PKDCL_CMP_BAD_DATA;
                }

              sym = (unsigned int)v;
            }
          else if (!xr_ascii (&r, &sym))
            {
              free (in.p);
              free (out.p);

              return PKDCL_CMP_BAD_DATA;
            }

          {
            unsigned long need;

            if (!x_ulong_add (out.n, 1UL, &need) || !xb_grow (&out, need))
              {
                free (in.p);
                free (out.p);

                return PKDCL_CMP_ABORT;
              }
          }

          out.p[out.n++] = (unsigned char)sym;
        }
      else
        {
          unsigned int len, eb, group;
          unsigned long dist;

          if (!xr_code (&r, len_bits, len_code, 16U, &sym))
            {
              free (in.p);
              free (out.p);

              return PKDCL_CMP_BAD_DATA;
            }

          eb = extra_len_bits[sym];
          v = 0UL;

          if (eb && !xr_get (&r, eb, &v))
            {
              free (in.p);
              free (out.p);

              return PKDCL_CMP_BAD_DATA;
            }

          len = (unsigned int)len_base0[sym] + (unsigned int)v + 2U;

          if (len == 519U)
            {
              break;
            }

          if (!xr_code (&r, dist_bits, dist_code, 64U, &group))
            {
              free (in.p);
              free (out.p);

              return PKDCL_CMP_BAD_DATA;
            }

          if (len == 2U)
            {
              eb = 2U;
            }
          else
            {
              eb = dbits;
            }

          if (!xr_get (&r, eb, &v))
            {
              free (in.p);
              free (out.p);

              return PKDCL_CMP_BAD_DATA;
            }

          dist = ((unsigned long)group << eb) + v + 1UL;

          if (dist > out.n)
            {
              free (in.p);
              free (out.p);

              return PKDCL_CMP_BAD_DATA;
            }

          {
            unsigned long need;

            if (!x_ulong_add (out.n, (unsigned long)len, &need)
                || !xb_grow (&out, need))
              {
                free (in.p);
                free (out.p);

                return PKDCL_CMP_ABORT;
              }
          }

          while (len--)
            {
              unsigned long src;

              src = out.n - dist;
              out.p[out.n++] = out.p[src];
            }
        }
    }

  x_write_all (write_func, opaque, out.p, out.n);
  free (in.p);
  free (out.p);

  return PKDCL_CMP_NO_ERROR;
}
