#ifndef AFX_EGGCRYPT_H__EGG_ENCRYPTION_INCLUDED__
#define AFX_EGGCRYPT_H__EGG_ENCRYPTION_INCLUDED__

#include "Tool.h"
#include "ZipCrypto.h"
#include "Lea.h"
#include "wzaes/wzaes.h"

// EGG encryption methods (the first byte of the Encrypt Info).
#define EGG_CRYPT_NONE   (-1)
#define EGG_CRYPT_ZIP    0
#define EGG_CRYPT_AES128 1
#define EGG_CRYPT_AES256 2
#define EGG_CRYPT_LEA128 5
#define EGG_CRYPT_LEA256 6

// EGG's password encoding: the UTF-8 password is converted to the bytes
// ALZip feeds the key derivation (CP1252 -- a code point outside it becomes
// '?'), NUL terminated.  False when the UTF-8 is malformed or too long.
bool EggPwEncode( const char* utf8, char* out, int cap, DWORD* outLen );

// The decryption state of ONE member.  EGG encrypts a whole member as one
// continuous stream: the ZipCrypto keys, the CTR counter and the HMAC span
// all of its blocks, so the state lives here and the blocks are fed in
// archive order.
class EggDecrypt
{
public:
	EggDecrypt();
	~EggDecrypt();

	// `info` is the Encrypt Info DATA (after the method byte).  For
	// ZipCrypto the 12 verifier bytes are consumed here, which continues
	// the key stream into the member data; for AES/LEA the password
	// verifier is compared and the cipher is prepared.
	bool Begin( int method, const BYTE* info, DWORD infoLen,
	            const char* password, char* why );

	// AES/LEA: the HMAC covers every block's CIPHERTEXT, in archive order.
	bool Auth( const BYTE* ciphertext, DWORD n );
	bool Ready( char* why );                  // compares the tag, sets up CTR

	bool Update( BYTE* buf, DWORD n );        // decrypts in place

	bool IsZip() const
		{ return m_method == EGG_CRYPT_ZIP; }
	const BYTE* ZipVerifier() const
		{ return m_ver; }

private:
	int       m_method;
	bool      m_ready;
	ZipCrypto m_zip;
	BYTE      m_ver[12];          // the plaintext ZipCrypto verifier
	iz_wzaes  m_aes;
	bool      m_useAes;
	Lea       m_lea;
	iz_hmac   m_hmac;
	bool      m_useLea;
	BYTE      m_tag[10];          // the expected HMAC tag
	BYTE      m_ctr[16];
};

#endif
