#ifndef AFX_ZIPCRYPTO_H__PKZIP_STREAM_CIPHER_INCLUDED__
#define AFX_ZIPCRYPTO_H__PKZIP_STREAM_CIPHER_INCLUDED__

#include "Tool.h"

// The classic PKZIP stream cipher (ZipCrypto), shared by the ZIP, ALZ and
// EGG providers.  The three-word state starts from the fixed constants and
// every PLAINTEXT byte updates it; the 12-byte password verifier is simply
// the first twelve bytes run through the same loop, which is why consuming
// it leaves the state ready for the member data.
//
// ALZ keeps the verifier at the head of the member data, EGG in the
// Encrypt Info sub-header, and both check the twelfth plaintext byte (or
// the high byte of the CRC) -- the cipher itself is identical.
class ZipCrypto
{
public:
	ZipCrypto()
		{ m_key[0] = m_key[1] = m_key[2] = 0; }

	void Init( const char* password );        // fixed keys + the password bytes
	BYTE Decode( BYTE c );                    // decrypt one byte, update the state
	void DecodeBuffer( BYTE* buf, int len );

	// The ordinary reflected CRC-32 (what zlib's crc32() computes), used by
	// the providers for their output check.
	static DWORD Crc( DWORD crc, const BYTE* p, int len );
	// The same table, one raw step with no pre/post complement: this is what
	// the stream cipher feeds its keys with.
	static DWORD CrcStep( DWORD crc, BYTE b );

private:
	void Update( BYTE c )
		{
			m_key[0] = CrcStep( m_key[0], c );
			m_key[1] = (m_key[1] + (m_key[0] & 0xFF)) * 134775813u + 1u;
			m_key[2] = CrcStep( m_key[2], (BYTE)(m_key[1] >> 24) );
		}

	DWORD m_key[3];
};

#endif
