#ifndef AFX_DGCRYPT_H__DGC_CRYPTO_INCLUDED__
#define AFX_DGCRYPT_H__DGC_CRYPTO_INCLUDED__

// The crypto of the DGCA archives (the ".dgc" files of the "DGC
// Archiver"), written from the analysis in dgca/NOTES-dgca-format.md
// section 8.
//
// The cipher is a self inverse XOR stream with one feedback byte.  Its
// key stream comes from an MT19937 variant, which is seeded with a
// SHA-512 that is stretched over 39 rounds:
//
//   key = SHA-512( material | password )  stretched over 39 rounds
//
// Two different materials are used by the writer:
//   key1 = SHA-512( 8 zero bytes | password )      -> the chunk headers
//   key2 = SHA-512( the plaintext header | password ) -> the payloads
//
// The password is the text of the -p switch.  With a key file it is the
// hex text of the SHA-512 of the file's content (128 characters).

// ------------------------------------------------------------------
// SHA-512, as the binary's own hash class computes it: the standard
// algorithm, but the 64 raw bytes of the state have every 64 bit word
// with its low dword first (so the digest is byte reversed per group).
// The length field of the padding is 16 bytes wide (the high dword is
// zero for every message that fits).
// ------------------------------------------------------------------
class DgcSha512
{
public:
	DgcSha512() { Init(); }
	void Init();
	void Update( const unsigned char* data, unsigned long len );
	void Final();
	void State( unsigned char out[64] );
private:
	void Compress( const unsigned char block[128] );
	unsigned __int64	h[8];
	unsigned char		buf[128];
	unsigned long		idx;
	unsigned __int64	bits, bitshi;
};

// ------------------------------------------------------------------
// The MT19937 variant of the binary: the state is seeded directly
// (state[i] = key[i % len]), the twist is the standard one and the
// tempered words are kept in a separate array, so the state keeps the
// untempered values.
// ------------------------------------------------------------------
class DgcMt
{
public:
	void Init( const unsigned long seed[624] );
	unsigned long Next();
private:
	void Twist();
	unsigned long	st[624];
	unsigned long	out[624];
	unsigned long	idx;
};

// ------------------------------------------------------------------
// The cipher object.
// ------------------------------------------------------------------
class DgcCrypt
{
public:
	DgcCrypt() { SetKey( 0, 0, 0, 0 ); }
	// the material and the password
	void SetKey( const unsigned char* material, unsigned long matlen,
				 const unsigned char* password, unsigned long pwlen );
	// the 8 zero bytes: the key of the chunk headers
	void SetKeyHeader( const unsigned char* password, unsigned long pwlen );
	// the plaintext header: the key of a chunk's payload
	void SetKeyPayload( const unsigned char* header, unsigned long hlen,
						const unsigned char* password, unsigned long pwlen );
	void Encrypt( unsigned char* buf, unsigned long n );
	void Decrypt( unsigned char* buf, unsigned long n );
private:
	unsigned char Prng();
	DgcSha512		hash;
	DgcMt			mt;
	unsigned char	keybuf[64];
	unsigned char	rolling[128];
	unsigned long	index, counter;
	unsigned char	feedback;
};

// The hex text of the SHA-512 of a buffer, the way the binary formats
// the state: every 64 bit word with its high dword first, so it is the
// standard digest.  The result is 128 characters and a NUL.
void DgcSha512Hex( const unsigned char* data, unsigned long len, char* out );

#endif
