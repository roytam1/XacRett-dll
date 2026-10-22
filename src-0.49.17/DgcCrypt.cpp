
#include "stdafx.h"
#include "DgcCrypt.h"

// ------------------------------------------------------------------
// SHA-512
// ------------------------------------------------------------------

// The 64 bit constants are built from their two halves: the "ULL" and
// the "UI64" suffixes do not exist in every compiler of the era.
#define DGC_C64(hi,lo) ( ((unsigned __int64)(hi) << 32) | (unsigned __int64)(lo) )

static const unsigned __int64 DgcSha512K[80] =
{
	DGC_C64(0x428a2f98,0xd728ae22), DGC_C64(0x71374491,0x23ef65cd), DGC_C64(0xb5c0fbcf,0xec4d3b2f), DGC_C64(0xe9b5dba5,0x8189dbbc),
	DGC_C64(0x3956c25b,0xf348b538), DGC_C64(0x59f111f1,0xb605d019), DGC_C64(0x923f82a4,0xaf194f9b), DGC_C64(0xab1c5ed5,0xda6d8118),
	DGC_C64(0xd807aa98,0xa3030242), DGC_C64(0x12835b01,0x45706fbe), DGC_C64(0x243185be,0x4ee4b28c), DGC_C64(0x550c7dc3,0xd5ffb4e2),
	DGC_C64(0x72be5d74,0xf27b896f), DGC_C64(0x80deb1fe,0x3b1696b1), DGC_C64(0x9bdc06a7,0x25c71235), DGC_C64(0xc19bf174,0xcf692694),
	DGC_C64(0xe49b69c1,0x9ef14ad2), DGC_C64(0xefbe4786,0x384f25e3), DGC_C64(0x0fc19dc6,0x8b8cd5b5), DGC_C64(0x240ca1cc,0x77ac9c65),
	DGC_C64(0x2de92c6f,0x592b0275), DGC_C64(0x4a7484aa,0x6ea6e483), DGC_C64(0x5cb0a9dc,0xbd41fbd4), DGC_C64(0x76f988da,0x831153b5),
	DGC_C64(0x983e5152,0xee66dfab), DGC_C64(0xa831c66d,0x2db43210), DGC_C64(0xb00327c8,0x98fb213f), DGC_C64(0xbf597fc7,0xbeef0ee4),
	DGC_C64(0xc6e00bf3,0x3da88fc2), DGC_C64(0xd5a79147,0x930aa725), DGC_C64(0x06ca6351,0xe003826f), DGC_C64(0x14292967,0x0a0e6e70),
	DGC_C64(0x27b70a85,0x46d22ffc), DGC_C64(0x2e1b2138,0x5c26c926), DGC_C64(0x4d2c6dfc,0x5ac42aed), DGC_C64(0x53380d13,0x9d95b3df),
	DGC_C64(0x650a7354,0x8baf63de), DGC_C64(0x766a0abb,0x3c77b2a8), DGC_C64(0x81c2c92e,0x47edaee6), DGC_C64(0x92722c85,0x1482353b),
	DGC_C64(0xa2bfe8a1,0x4cf10364), DGC_C64(0xa81a664b,0xbc423001), DGC_C64(0xc24b8b70,0xd0f89791), DGC_C64(0xc76c51a3,0x0654be30),
	DGC_C64(0xd192e819,0xd6ef5218), DGC_C64(0xd6990624,0x5565a910), DGC_C64(0xf40e3585,0x5771202a), DGC_C64(0x106aa070,0x32bbd1b8),
	DGC_C64(0x19a4c116,0xb8d2d0c8), DGC_C64(0x1e376c08,0x5141ab53), DGC_C64(0x2748774c,0xdf8eeb99), DGC_C64(0x34b0bcb5,0xe19b48a8),
	DGC_C64(0x391c0cb3,0xc5c95a63), DGC_C64(0x4ed8aa4a,0xe3418acb), DGC_C64(0x5b9cca4f,0x7763e373), DGC_C64(0x682e6ff3,0xd6b2b8a3),
	DGC_C64(0x748f82ee,0x5defb2fc), DGC_C64(0x78a5636f,0x43172f60), DGC_C64(0x84c87814,0xa1f0ab72), DGC_C64(0x8cc70208,0x1a6439ec),
	DGC_C64(0x90befffa,0x23631e28), DGC_C64(0xa4506ceb,0xde82bde9), DGC_C64(0xbef9a3f7,0xb2c67915), DGC_C64(0xc67178f2,0xe372532b),
	DGC_C64(0xca273ece,0xea26619c), DGC_C64(0xd186b8c7,0x21c0c207), DGC_C64(0xeada7dd6,0xcde0eb1e), DGC_C64(0xf57d4f7f,0xee6ed178),
	DGC_C64(0x06f067aa,0x72176fba), DGC_C64(0x0a637dc5,0xa2c898a6), DGC_C64(0x113f9804,0xbef90dae), DGC_C64(0x1b710b35,0x131c471b),
	DGC_C64(0x28db77f5,0x23047d84), DGC_C64(0x32caab7b,0x40c72493), DGC_C64(0x3c9ebe0a,0x15c9bebc), DGC_C64(0x431d67c4,0x9c100d4c),
	DGC_C64(0x4cc5d4be,0xcb3e42b6), DGC_C64(0x597f299c,0xfc657e2a), DGC_C64(0x5fcb6fab,0x3ad6faec), DGC_C64(0x6c44198c,0x4a475817)
};

static const unsigned __int64 DgcSha512IV[8] =
{
	DGC_C64(0x6a09e667,0xf3bcc908), DGC_C64(0xbb67ae85,0x84caa73b), DGC_C64(0x3c6ef372,0xfe94f82b), DGC_C64(0xa54ff53a,0x5f1d36f1),
	DGC_C64(0x510e527f,0xade682d1), DGC_C64(0x9b05688c,0x2b3e6c1f), DGC_C64(0x1f83d9ab,0xfb41bd6b), DGC_C64(0x5be0cd19,0x137e2179)
};

static unsigned __int64 DgcRotr64( unsigned __int64 x, int n )
{
	return (x >> n) | (x << (64 - n));
}

static unsigned __int64 DgcRd64BE( const unsigned char* p )
{
	return ((unsigned __int64)p[0] << 56) | ((unsigned __int64)p[1] << 48) |
		   ((unsigned __int64)p[2] << 40) | ((unsigned __int64)p[3] << 32) |
		   ((unsigned __int64)p[4] << 24) | ((unsigned __int64)p[5] << 16) |
		   ((unsigned __int64)p[6] << 8)  | (unsigned __int64)p[7];
}

static void DgcWr64BE( unsigned char* p, unsigned __int64 v )
{
	p[0] = (unsigned char)(v >> 56); p[1] = (unsigned char)(v >> 48);
	p[2] = (unsigned char)(v >> 40); p[3] = (unsigned char)(v >> 32);
	p[4] = (unsigned char)(v >> 24); p[5] = (unsigned char)(v >> 16);
	p[6] = (unsigned char)(v >> 8);  p[7] = (unsigned char)v;
}

static void DgcWr64LE( unsigned char* p, unsigned __int64 v )
{
	p[0] = (unsigned char)v;         p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
	p[4] = (unsigned char)(v >> 32); p[5] = (unsigned char)(v >> 40);
	p[6] = (unsigned char)(v >> 48); p[7] = (unsigned char)(v >> 56);
}

void DgcSha512::Init()
{
	int i;
	for( i=0; i<8; i++ ) h[i] = DgcSha512IV[i];
	idx = 0;
	bits = 0;
	bitshi = 0;
}

void DgcSha512::Compress( const unsigned char block[128] )
{
	unsigned __int64 w[80], a, b, c, d, e, f, g, hh, t1, t2;
	int i;

	for( i=0; i<16; i++ ) w[i] = DgcRd64BE( block + i*8 );
	for( i=16; i<80; i++ ) {
		unsigned __int64 x = w[i-15], y = w[i-2];
		unsigned __int64 s0 = DgcRotr64(x,1) ^ DgcRotr64(x,8) ^ (x >> 7);
		unsigned __int64 s1 = DgcRotr64(y,19) ^ DgcRotr64(y,61) ^ (y >> 6);
		w[i] = w[i-16] + s0 + w[i-7] + s1;
	}
	a = h[0]; b = h[1]; c = h[2]; d = h[3];
	e = h[4]; f = h[5]; g = h[6]; hh = h[7];
	for( i=0; i<80; i++ ) {
		t1 = hh + (DgcRotr64(e,14) ^ DgcRotr64(e,18) ^ DgcRotr64(e,41)) +
			 ((e & f) ^ ((~e) & g)) + DgcSha512K[i] + w[i];
		t2 = (DgcRotr64(a,28) ^ DgcRotr64(a,34) ^ DgcRotr64(a,39)) +
			 ((a & b) ^ (a & c) ^ (b & c));
		hh = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d;
	h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void DgcSha512::Update( const unsigned char* data, unsigned long len )
{
	unsigned __int64 old = bits;
	bits += (unsigned __int64)len * 8;
	if( bits < old ) bitshi++;

	while( len ) {
		unsigned long k = 128 - idx;
		if( k > len ) k = len;
		memcpy( buf + idx, data, k );
		idx += k;
		data += k;
		len -= k;
		if( idx == 128 ) {
			Compress( buf );
			idx = 0;
		}
	}
}

void DgcSha512::Final()
{
	unsigned __int64 b = bits, bh = bitshi;
	unsigned char pad[256];
	unsigned long padlen;

	memset( pad, 0, sizeof(pad) );
	pad[0] = 0x80;
	padlen = ( idx < 112 ) ? (112 - idx) : (240 - idx);
	Update( pad, padlen );
	DgcWr64BE( pad, bh );
	Update( pad, 8 );
	DgcWr64BE( pad, b );
	Update( pad, 8 );
	bits = b;
	bitshi = bh;
}

void DgcSha512::State( unsigned char out[64] )
{
	int i;
	for( i=0; i<8; i++ ) DgcWr64LE( out + i*8, h[i] );
}

// ------------------------------------------------------------------
// the MT19937 variant
// ------------------------------------------------------------------

void DgcMt::Init( const unsigned long seed[624] )
{
	memcpy( st, seed, sizeof(st) );
	idx = 624;
	Twist();
}

void DgcMt::Twist()
{
	unsigned long y;
	int i;

	for( i=0; i<624-397; i++ ) {
		y = (st[i] & 0x80000000ul) | (st[i+1] & 0x7FFFFFFful);
		st[i] = st[i+397] ^ (y >> 1) ^ ((y & 1) ? 0x9908B0DFul : 0);
	}
	for( ; i<623; i++ ) {
		y = (st[i] & 0x80000000ul) | (st[i+1] & 0x7FFFFFFful);
		st[i] = st[i+(397-624)] ^ (y >> 1) ^ ((y & 1) ? 0x9908B0DFul : 0);
	}
	y = (st[623] & 0x80000000ul) | (st[0] & 0x7FFFFFFful);
	st[623] = st[396] ^ (y >> 1) ^ ((y & 1) ? 0x9908B0DFul : 0);

	for( i=0; i<624; i++ ) {
		y = st[i];
		y ^= (y >> 11);
		y ^= (y << 7) & 0x9D2C5680ul;
		y ^= (y << 15) & 0xEFC60000ul;
		y ^= (y >> 18);
		out[i] = y;
	}
	idx = 0;
}

unsigned long DgcMt::Next()
{
	if( idx >= 624 ) Twist();
	return out[idx++];
}

// ------------------------------------------------------------------
// the cipher
// ------------------------------------------------------------------

void DgcCrypt::SetKey( const unsigned char* material, unsigned long matlen,
					   const unsigned char* password, unsigned long pwlen )
{
	unsigned char hist[39*64], cur[64];
	unsigned long seed[624];
	int i;

	hash.Init();
	hash.Update( material, matlen );
	hash.Update( password, pwlen );
	hash.Final();
	hash.State( cur );

	for( i=0; i<39; i++ ) {
		memcpy( hist + i*64, cur, 64 );
		hash.Update( cur, 64 );
		hash.Update( cur, 64 );
		hash.State( cur );
	}
	seed[0] = 0xFFFFFFFFul;
	for( i=0; i<623; i++ ) {
		seed[1+i] = (unsigned long)hist[i*4] | ((unsigned long)hist[i*4+1] << 8) |
					((unsigned long)hist[i*4+2] << 16) | ((unsigned long)hist[i*4+3] << 24);
	}
	mt.Init( seed );

	// the hash is reset after the stretch
	hash.Init();
	memset( keybuf, 0, sizeof(keybuf) );
	memset( rolling, 0, sizeof(rolling) );
	index = 0;
	counter = 0;
	feedback = 0;
}

void DgcCrypt::SetKeyHeader( const unsigned char* password, unsigned long pwlen )
{
	static const unsigned char zero[8] = { 0,0,0,0,0,0,0,0 };
	SetKey( zero, 8, password, pwlen );
}

void DgcCrypt::SetKeyPayload( const unsigned char* header, unsigned long hlen,
							   const unsigned char* password, unsigned long pwlen )
{
	SetKey( header, hlen, password, pwlen );
}

unsigned char DgcCrypt::Prng()
{
	unsigned char fb, ret;

	if( index == 0 || counter < 64 ) {
		unsigned char tmp[128];
		int i;
		for( i=0; i<32; i++ ) {
			unsigned long v = mt.Next();
			tmp[i*4+0] = (unsigned char)v;
			tmp[i*4+1] = (unsigned char)(v >> 8);
			tmp[i*4+2] = (unsigned char)(v >> 16);
			tmp[i*4+3] = (unsigned char)(v >> 24);
		}
		hash.Update( tmp, 128 );
		hash.Update( rolling, 128 );
		hash.State( keybuf );
		if( counter < 64 ) counter++;
	}
	fb = feedback;
	ret = (unsigned char)(keybuf[index] ^ fb);
	// the scratch buffer overlaps the upper half of the rolling state,
	// so every feedback byte is stored twice
	rolling[index] = fb;
	rolling[64+index] = fb;
	feedback = 0;
	index = (index + 1) & 0x3F;
	return ret;
}

void DgcCrypt::Encrypt( unsigned char* buf, unsigned long n )
{
	unsigned long i;
	for( i=0; i<n; i++ ) {
		unsigned char in = buf[i], k1, k2;
		k1 = Prng();
		k2 = Prng();
		buf[i] = (unsigned char)(in ^ k1 ^ k2);
		feedback = (unsigned char)(in ^ k1);
	}
}

void DgcCrypt::Decrypt( unsigned char* buf, unsigned long n )
{
	unsigned long i;
	for( i=0; i<n; i++ ) {
		unsigned char in = buf[i], k1, k2;
		k1 = Prng();
		k2 = Prng();
		buf[i] = (unsigned char)(in ^ k1 ^ k2);
		feedback = (unsigned char)(in ^ k2);
	}
}

// The hex text of the SHA-512 of a buffer: the binary formats the state
// word by word, the high dword first, so it is the standard digest.
void DgcSha512Hex( const unsigned char* data, unsigned long len, char* out )
{
	DgcSha512 h;
	unsigned char st[64];
	int g, i, j;
	char* p = out;

	h.Init();
	h.Update( data, len );
	h.Final();
	h.State( st );
	for( g=0; g<8; g++ ) {
		for( i=0; i<8; i++ ) {
			j = g*8 + i;
			sprintf( p + j*2, "%02x", st[g*8+7-i] );
		}
	}
	out[128] = 0;
}
