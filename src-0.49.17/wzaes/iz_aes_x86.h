/*
 * x86-64 AES-NI, PCLMULQDQ GHASH, SHA-256.
 * SPDX-License-Identifier: MIT-0.
 */
#ifndef IZ_AES_X86_H
#define IZ_AES_X86_H
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__amd64__)) \
 && (defined(__clang__) || __GNUC__ >= 5)
# define IZ_AES_X86_BASE 1
# include <cpuid.h>
# include <immintrin.h>
# if !defined(NO_AES_NI)
#  define IZ_AES_NI_ENABLED 1
# endif
# if !defined(NO_GHASH_PCLMUL)
#  define IZ_GHASH_PCLMUL_ENABLED 1
# endif
# if !defined(NO_SHA256_NI)
#  define IZ_SHA256_NI_ENABLED 1
# endif
/* CPUID flags are read only on the baseline instruction path. Every
 * accelerated function is kept in a separate, non-inlined target function. */
static int iz_aes_x86_cpu(unsigned int leaf, unsigned int reg,
                          unsigned int bit)
{
    unsigned int a,b,c,d;
    if (__get_cpuid_max(0, (unsigned int *)0) < leaf) return 0;
    __cpuid_count(leaf, 0, a,b,c,d);
    return (((reg == 1 ? b : c) >> bit) & 1U) != 0;
}
# ifdef IZ_AES_NI_ENABLED
static int iz_aes_ni_available(void)
{
    static int seen = 0;
    int s = __atomic_load_n(&seen, __ATOMIC_RELAXED);
    if (!s) {
        s = iz_aes_x86_cpu(1,2,25) ? 2 : 1;
        __atomic_store_n(&seen,s,__ATOMIC_RELAXED);
    }
    return s == 2;
}
/* Expanded round keys already have the exact AES-NI byte representation.
 * AESENC includes SubBytes, ShiftRows, MixColumns, AddRoundKey. */
static __attribute__((target("aes"),noinline)) void
iz_aes_ni_block(const unsigned char *keys, unsigned rounds,
                const unsigned char *input, unsigned char *output)
{
    __m128i v;
    unsigned r;
    v = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(const void *)input),
                      _mm_loadu_si128((const __m128i *)(const void *)keys));
    for (r=1; r<rounds; ++r)
        v = _mm_aesenc_si128(v,_mm_loadu_si128((const __m128i *)(const void *)(keys+16*r)));
    v = _mm_aesenclast_si128(v,_mm_loadu_si128((const __m128i *)(const void *)(keys+16*rounds)));
    _mm_storeu_si128((__m128i *)(void *)output, v);
    v = _mm_setzero_si128();
}
# endif
# ifdef IZ_GHASH_PCLMUL_ENABLED
static int iz_ghash_pclmul_available(void)
{
    static int seen=0;
    int s=__atomic_load_n(&seen,__ATOMIC_RELAXED);
    if(!s) {
        s=iz_aes_x86_cpu(1,2,1) ? 2 : 1;
        __atomic_store_n(&seen,s,__ATOMIC_RELAXED);
    }
    return s==2;
}
/* Reverse bits within each byte, converting GCM's big-endian bit convention
 * to the little-endian polynomial coefficients used by PCLMULQDQ. */
static __attribute__((target("pclmul"))) __m128i
iz_ghash_reverse_bits(__m128i v)
{
    v=_mm_or_si128(_mm_slli_epi16(_mm_and_si128(v,_mm_set1_epi8(0x55)),1),
                   _mm_srli_epi16(_mm_and_si128(v,_mm_set1_epi8((char)0xaa)),1));
    v=_mm_or_si128(_mm_slli_epi16(_mm_and_si128(v,_mm_set1_epi8(0x33)),2),
                   _mm_srli_epi16(_mm_and_si128(v,_mm_set1_epi8((char)0xcc)),2));
    v=_mm_or_si128(_mm_slli_epi16(_mm_and_si128(v,_mm_set1_epi8(0x0f)),4),
                   _mm_srli_epi16(_mm_and_si128(v,_mm_set1_epi8((char)0xf0)),4));
    return v;
}
static __attribute__((target("pclmul"))) __m128i
iz_ghash_shift_left(__m128i v, int k)
{
    return _mm_or_si128(_mm_slli_epi64(v,k),
                         _mm_slli_si128(_mm_srli_epi64(v,64-k),8));
}
/* Multiply modulo x^128+x^7+x^2+x+1. The GHASH input/output remains in
 * standard GCM bit order; no precomputation or context-layout changes. */
static __attribute__((target("pclmul"),noinline)) void
iz_ghash_pclmul_block(unsigned char acc[16], const unsigned char hkey[16],
                      const unsigned char p[16])
{
    __m128i a,b,low,high,mid,al,bl,v,top;
    unsigned overflow;
    a=iz_ghash_reverse_bits(_mm_xor_si128(
       _mm_loadu_si128((const __m128i *)(const void *)acc),
       _mm_loadu_si128((const __m128i *)(const void *)p)));
    b=iz_ghash_reverse_bits(_mm_loadu_si128((const __m128i *)(const void *)hkey));
    low=_mm_clmulepi64_si128(a,b,0x00);
    high=_mm_clmulepi64_si128(a,b,0x11);
    al=_mm_xor_si128(a,_mm_shuffle_epi32(a,0x4e));
    bl=_mm_xor_si128(b,_mm_shuffle_epi32(b,0x4e));
    mid=_mm_xor_si128(_mm_clmulepi64_si128(al,bl,0x00),
                       _mm_xor_si128(low,high));
    low=_mm_xor_si128(low,_mm_slli_si128(mid,8));
    high=_mm_xor_si128(high,_mm_srli_si128(mid,8));
    v=_mm_xor_si128(high,_mm_xor_si128(iz_ghash_shift_left(high,1),
                  _mm_xor_si128(iz_ghash_shift_left(high,2),
                               iz_ghash_shift_left(high,7))));
    top=_mm_srli_si128(high,8);
    overflow=(unsigned)_mm_cvtsi128_si32(_mm_xor_si128(
      _mm_srli_epi64(top,63),
      _mm_xor_si128(_mm_srli_epi64(top,62),_mm_srli_epi64(top,57))));
    overflow ^= (overflow<<1) ^ (overflow<<2) ^ (overflow<<7);
    v=_mm_xor_si128(low,_mm_xor_si128(v,_mm_cvtsi32_si128((int)overflow)));
    v=iz_ghash_reverse_bits(v);
    _mm_storeu_si128((__m128i *)(void *)acc,v);
    /* Only caller-owned buffers retain state; SIMD registers are volatile. */
}
# endif
# ifdef IZ_SHA256_NI_ENABLED
static int iz_sha256_ni_available(void)
{
    static int seen=0;
    int s=__atomic_load_n(&seen,__ATOMIC_RELAXED);
    if(!s) {
        s=(iz_aes_x86_cpu(1,2,9) && iz_aes_x86_cpu(7,1,29)) ? 2 : 1;
        __atomic_store_n(&seen,s,__ATOMIC_RELAXED);
    }
    return s==2;
}
/* Four rolling 128-bit vectors expand the SHA-256 message schedule using
 * SHA256MSG1/SHA256MSG2, with SHA256RNDS2 processing two rounds at a time.
 * Neither the 64-word software schedule nor its erasure is needed on SHA-NI. */
static __attribute__((target("sha,ssse3"),noinline)) void
iz_sha256_ni_block(unsigned long h[8], const unsigned char *p,
                    const unsigned long k[64])
{
    __m128i abef, cdgh, orig0, orig1, msg, w[4], mask, tmp;
    unsigned int s0[4], s1[4];
    unsigned i, j;
    mask=_mm_set_epi8(12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3);
    for(i=0;i<4;i++)
        w[i]=_mm_shuffle_epi8(_mm_loadu_si128(
           (const __m128i *)(const void *)(p+16*i)),mask);
    abef=_mm_set_epi32((int)h[0],(int)h[1],(int)h[4],(int)h[5]);
    cdgh=_mm_set_epi32((int)h[2],(int)h[3],(int)h[6],(int)h[7]);
    orig0=abef;orig1=cdgh;
    for (i=0;i<16;i++) {
        j=i&3U;
        if(i>=4) {
            tmp=_mm_sha256msg1_epu32(w[j],w[(j+1U)&3U]);
            tmp=_mm_add_epi32(tmp,_mm_alignr_epi8(w[(j+3U)&3U],
                                                   w[(j+2U)&3U],4));
            w[j]=_mm_sha256msg2_epu32(tmp,w[(j+3U)&3U]);
        }
        msg=_mm_add_epi32(w[j],
             _mm_set_epi32((int)k[4*i+3],(int)k[4*i+2],
                           (int)k[4*i+1],(int)k[4*i]));
        cdgh=_mm_sha256rnds2_epu32(cdgh,abef,msg);
        msg=_mm_shuffle_epi32(msg,0x0e);
        abef=_mm_sha256rnds2_epu32(abef,cdgh,msg);
    }
    abef=_mm_add_epi32(abef,orig0);
    cdgh=_mm_add_epi32(cdgh,orig1);
    _mm_storeu_si128((__m128i *)(void *)s0,abef);
    _mm_storeu_si128((__m128i *)(void *)s1,cdgh);
    h[0]=s0[3];h[1]=s0[2];h[2]=s1[3];h[3]=s1[2];
    h[4]=s0[1];h[5]=s0[0];h[6]=s1[1];h[7]=s1[0];
    {
        volatile unsigned char *v=(volatile unsigned char *)(void *)s0;
        for(i=0;i<sizeof(s0);++i) v[i]=0;
        v=(volatile unsigned char *)(void *)s1;
        for(i=0;i<sizeof(s1);++i) v[i]=0;
        v=(volatile unsigned char *)(void *)w;
        for(i=0;i<sizeof(w);++i) v[i]=0;
    }
}
# endif
#endif /* IZ_AES_X86_BASE */
#endif /* IZ_AES_X86_H */
