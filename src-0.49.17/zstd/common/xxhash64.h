/* xxhash64.h -- minimal XXH64 for zstd frame checksums (VC6 build).
   Replaces upstream xxhash.h (261 KB, needs ULL/long long/SIMD paths).
   Provides exactly what zstd_decompress.c uses: XXH64(), reset/update/
   digest over an explicit state struct. Standard ECMA-style XXH64.
   Public domain dedication for this file. */
#ifndef XXHASH64_MINI_H
#define XXHASH64_MINI_H

#include <stddef.h>

typedef unsigned __int64 XXH64_hash_t;
typedef struct { unsigned __int64 total; unsigned __int64 v[4];
                 unsigned char buf[32]; unsigned used; } XXH64_state_t;

#define XXH64_P1 (((unsigned __int64)0x9E3779B1 << 32) | 0x85EBCA87)
#define XXH64_P2 (((unsigned __int64)0xC2B2AE3D << 32) | 0x27D4EB4F)
#define XXH64_P3 (((unsigned __int64)0x165667B1 << 32) | 0x9E3779F9)
#define XXH64_P4 (((unsigned __int64)0x85EBCA77 << 32) | 0xC2B2AE63)
#define XXH64_P5 (((unsigned __int64)0x27D4EB2F << 32) | 0x165667C5)

static unsigned __int64 XXH64_rotl(unsigned __int64 x, int r)
{ return (x << r) | (x >> (64 - r)); }

static unsigned __int64 XXH64_readLE64(const unsigned char *p)
{
	unsigned __int64 v = 0;
	int i;
	for( i=7; i>=0; i-- ) v = (v << 8) | p[i];
	return v;
}

static unsigned __int64 XXH64_round(unsigned __int64 acc, unsigned __int64 x)
{
	acc += x * XXH64_P2;
	acc = XXH64_rotl(acc, 31);
	return acc * XXH64_P1;
}

static unsigned __int64 XXH64_avalanche(unsigned __int64 h)
{
	h ^= h >> 33;
	h *= XXH64_P2;
	h ^= h >> 29;
	h *= XXH64_P3;
	h ^= h >> 32;
	return h;
}

static XXH64_hash_t XXH64(const void *input, size_t len, XXH64_hash_t seed);
static void XXH64_reset(XXH64_state_t *s, XXH64_hash_t seed);
static void XXH64_update(XXH64_state_t *s, const void *input, size_t len);
static XXH64_hash_t XXH64_digest(XXH64_state_t *s);
static XXH64_hash_t XXH64(const void *input, size_t len, XXH64_hash_t seed)
{
	XXH64_state_t st;
	XXH64_reset(&st, seed);
	XXH64_update(&st, input, len);
	return XXH64_digest(&st);
}

static void XXH64_reset(XXH64_state_t *s, XXH64_hash_t seed)
{
	s->total = 0;
	s->v[0] = seed + XXH64_P1 + XXH64_P2;
	s->v[1] = seed + XXH64_P2;
	s->v[2] = seed;
	s->v[3] = seed - XXH64_P1;
	s->used = 0;
}

static void XXH64_update(XXH64_state_t *s, const void *input, size_t len)
{
	const unsigned char *p = (const unsigned char *)input;
	unsigned i, fill;
	s->total += (unsigned __int64)len;
	if( s->used + len < 32 )
		{ for( i=0; i<(unsigned)len; i++ ) s->buf[s->used++] = p[i]; return; }
	if( s->used )
		{
		fill = 32 - s->used;
		for( i=0; i<fill; i++ ) s->buf[s->used++] = p[i];
		p += fill; len -= fill;
		s->v[0] = XXH64_round(s->v[0], XXH64_readLE64(s->buf));
		s->v[1] = XXH64_round(s->v[1], XXH64_readLE64(s->buf + 8));
		s->v[2] = XXH64_round(s->v[2], XXH64_readLE64(s->buf + 16));
		s->v[3] = XXH64_round(s->v[3], XXH64_readLE64(s->buf + 24));
		s->used = 0;
		}
	while( len >= 32 )
		{
		s->v[0] = XXH64_round(s->v[0], XXH64_readLE64(p));
		s->v[1] = XXH64_round(s->v[1], XXH64_readLE64(p + 8));
		s->v[2] = XXH64_round(s->v[2], XXH64_readLE64(p + 16));
		s->v[3] = XXH64_round(s->v[3], XXH64_readLE64(p + 24));
		p += 32; len -= 32;
		}
	for( i=0; i<(unsigned)len; i++ ) s->buf[s->used++] = p[i];
}

static XXH64_hash_t XXH64_digest(XXH64_state_t *s)
{
	unsigned __int64 h;
	unsigned char *p = s->buf;
	unsigned left = s->used;
	if( s->total >= 32 )
		{
		h = XXH64_rotl(s->v[0],1) + XXH64_rotl(s->v[1],7) +
		    XXH64_rotl(s->v[2],12) + XXH64_rotl(s->v[3],18);
		h ^= XXH64_round(0, s->v[0]); h = h * XXH64_P1 + XXH64_P4;
		h ^= XXH64_round(0, s->v[1]); h = h * XXH64_P1 + XXH64_P4;
		h ^= XXH64_round(0, s->v[2]); h = h * XXH64_P1 + XXH64_P4;
		h ^= XXH64_round(0, s->v[3]); h = h * XXH64_P1 + XXH64_P4;
		}
	else
		h = s->v[2] + XXH64_P5;
	h += s->total;
	while( left >= 8 )
		{ unsigned __int64 k = XXH64_round(0, XXH64_readLE64(p));
		  h ^= k; h = XXH64_rotl(h,27) * XXH64_P1 + XXH64_P4; p += 8; left -= 8; }
	if( left >= 4 )
		{ h ^= (unsigned __int64)(p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24)) * XXH64_P1;
		  h = XXH64_rotl(h,23) * XXH64_P2 + XXH64_P3; p += 4; left -= 4; }
	while( left-- )
		{ h ^= (unsigned __int64)(*p++) * XXH64_P5; h = XXH64_rotl(h,11) * XXH64_P1; }
	return XXH64_avalanche(h);
}

#endif
