#include "stdafx.h"
#include "EggCrypt.h"
#include <string.h>

//-- EGG password encoding -----------------------
//
// ALZip converts the password (UTF-16) to the system ANSI code page before
// key derivation; a character missing from that page becomes '?'.  Measured
// against the oracle (ALZipCon 8.0.0.18, code page 1252): e-acute -> E9,
// sharp s -> DF, euro -> 80, Hangul -> 3F.  The 27 positions CP1252 fills
// outside Latin-1 are therefore needed for 0x80-0x9F; the five undefined
// positions and anything above U+00FF become '?'.

static const WORD cp1252_hi[32] = {
	0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0000, 0x017D, 0x0000,
	0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178
};

static BYTE Cp1252Byte( DWORD cp )
{
	int i;

	if( cp < 0x80 || (cp >= 0xA0 && cp <= 0xFF) )
		return (BYTE)cp;
	for( i = 0; i < 32; i++ )
	{
		if( cp1252_hi[i] && cp1252_hi[i] == cp )
			return (BYTE)(0x80 + i);
	}
	return '?';
}

// Strict UTF-8 -> CP1252 bytes.  Returns 1 on success, 0 on invalid UTF-8,
// -1 when the output does not fit.
static int EggPwEncodeUtf8( const char* utf8, char* out, int cap, DWORD* outLen )
{
	const BYTE* s = (const BYTE*)utf8;
	int n = 0;

	while( *s )
	{
		DWORD cp;

		if( *s < 0x80 )
			cp = *s++;
		else if( (*s & 0xE0) == 0xC0 )
		{
			if( (s[1] & 0xC0) != 0x80 || (*s & 0x1E) == 0 )
				return 0;
			cp = ((DWORD)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
			if( cp < 0x80 )
				return 0;
			s += 2;
		}
		else if( (*s & 0xF0) == 0xE0 )
		{
			if( (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 )
				return 0;
			cp = ((DWORD)(s[0] & 0x0F) << 12) |
			     ((DWORD)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
			if( cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF) )
				return 0;
			s += 3;
		}
		else if( (*s & 0xF8) == 0xF0 )
		{
			if( (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 ||
			    (s[3] & 0xC0) != 0x80 )
				return 0;
			cp = ((DWORD)(s[0] & 0x07) << 18) |
			     ((DWORD)(s[1] & 0x3F) << 12) |
			     ((DWORD)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
			if( cp < 0x10000 || cp > 0x10FFFF )
				return 0;
			s += 4;
		}
		else
			return 0;
		if( n + 1 >= cap )
			return -1;
		out[n++] = (char)Cp1252Byte( cp );
	}
	out[n] = '\0';
	if( outLen )
		*outLen = (DWORD)n;
	return 1;
}

bool EggPwEncode( const char* utf8, char* out, int cap, DWORD* outLen )
{
	int r;

	if( outLen )
		*outLen = 0;
	if( !utf8 )
		return false;
	r = EggPwEncodeUtf8( utf8, out, cap, outLen );
	if( r > 0 )
		return true;
	if( r < 0 )
		return false;
	// Not UTF-8: the caller already handed us system-ANSI bytes (the modal
	// password dialog returns the edit control's text in the system code
	// page).  Pass them through unchanged; the reference would reject them,
	// but that would make a Latin-1 accented password typed in the dialog
	// unusable while ALZip itself accepts it.
	{
		int n = lstrlenA( utf8 );

		if( n + 1 >= cap )
			return false;
		memcpy( out, utf8, (size_t)n + 1 );
		if( outLen )
			*outLen = (DWORD)n;
		return true;
	}
}

//-- one member's decryption state -----------------------

EggDecrypt::EggDecrypt()
{
	m_method = EGG_CRYPT_NONE;
	m_ready = false;
	m_useAes = false;
	m_useLea = false;
	memset( m_ver, 0, sizeof(m_ver) );
	memset( m_tag, 0, sizeof(m_tag) );
	memset( m_ctr, 0, sizeof(m_ctr) );
	memset( &m_aes, 0, sizeof(m_aes) );
	memset( &m_hmac, 0, sizeof(m_hmac) );
}

EggDecrypt::~EggDecrypt()
{
	memset( &m_aes, 0, sizeof(m_aes) );
	memset( &m_hmac, 0, sizeof(m_hmac) );
}

bool EggDecrypt::Begin( int method, const BYTE* info, DWORD infoLen,
                        const char* password, char* why )
{
	char  pw[256];
	DWORD pwlen = 0;
	int   klen, i;

	m_method = method;
	m_ready = false;
	m_useAes = false;
	m_useLea = false;

	if( !password || !*password )
	{
		strcpy( why, "password required" );
		return false;
	}
	if( !EggPwEncode( password, pw, sizeof(pw), &pwlen ) )
	{
		strcpy( why, "invalid password encoding" );
		return false;
	}

	if( method == EGG_CRYPT_ZIP )
	{
		if( infoLen < 12 )
		{
			strcpy( why, "ZipCrypto verifier too short" );
			return false;
		}
		m_zip.Init( pw );
		for( i = 0; i < 12; i++ )
			m_ver[i] = m_zip.Decode( info[i] );
		m_ready = true;
		return true;
	}

	klen = (method == EGG_CRYPT_AES128 || method == EGG_CRYPT_LEA128)
	       ? 16 : 32;
	{
		DWORD saltlen = (DWORD)klen / 2;
		const BYTE* salt;
		const BYTE* ver;
		const BYTE* mac;

		if( infoLen < saltlen + 12 )
		{
			strcpy( why, "Encrypt Info too short" );
			return false;
		}
		salt = info;
		ver = info + saltlen;
		mac = info + saltlen + 2;
		memcpy( m_tag, mac, 10 );

		if( method == EGG_CRYPT_AES128 || method == EGG_CRYPT_AES256 )
		{
			BYTE v2[2];

			// iz_aes_init derives PBKDF2-HMAC-SHA1(pw, salt, 1000,
			// 2*klen+2), expands AES with the first key, seeds the
			// HMAC with the second, and returns the 2-byte verifier.
			if( !iz_aes_init( &m_aes, pw, salt, (unsigned)(klen * 8), v2 ) )
			{
				strcpy( why, "AES setup failed" );
				return false;
			}
			if( v2[0] != ver[0] || v2[1] != ver[1] )
			{
				strcpy( why, "incorrect password" );
				return false;
			}
			m_useAes = true;
		}
		else
		{
			BYTE dk[66];

			if( !iz_pbkdf2_hmac_sha1( (const BYTE*)pw, pwlen, salt, saltlen,
			                          1000, dk, (DWORD)klen * 2 + 2 ) )
			{
				strcpy( why, "key derivation failed" );
				return false;
			}
			if( dk[klen * 2] != ver[0] || dk[klen * 2 + 1] != ver[1] )
			{
				iz_aes_wipe( dk, sizeof(dk) );
				strcpy( why, "incorrect password" );
				return false;
			}
			m_lea.SetKey( dk, klen );
			iz_hmac_sha1_init( &m_hmac, dk + klen, (size_t)klen );
			iz_aes_wipe( dk, sizeof(dk) );
			memset( m_ctr, 0, sizeof(m_ctr) );
			m_useLea = true;
		}
	}
	iz_aes_wipe( pw, sizeof(pw) );
	return true;
}

bool EggDecrypt::Auth( const BYTE* ciphertext, DWORD n )
{
	if( m_useAes )
	{
		iz_aes_mac_update( &m_aes, ciphertext, n );
		return true;
	}
	if( m_useLea )
	{
		iz_hmac_sha1_update( &m_hmac, ciphertext, n );
		return true;
	}
	return true;
}

bool EggDecrypt::Ready( char* why )
{
	BYTE tag[20];

	if( m_useAes )
	{
		// iz_aes_auth finalises the HMAC and gives the first 10 bytes.
		iz_aes_auth( &m_aes, tag );
		if( memcmp( tag, m_tag, 10 ) != 0 )
		{
			strcpy( why, "authentication mismatch (HMAC-SHA1 of the ciphertext)" );
			return false;
		}
		m_ready = true;
		return true;
	}
	if( m_useLea )
	{
		iz_hmac_sha1_finish( &m_hmac, tag );
		if( memcmp( tag, m_tag, 10 ) != 0 )
		{
			iz_aes_wipe( tag, sizeof(tag) );
			strcpy( why, "authentication mismatch (HMAC-SHA1 of the ciphertext)" );
			return false;
		}
		iz_aes_wipe( tag, sizeof(tag) );
		m_ready = true;
		return true;
	}
	m_ready = true;         // ZipCrypto has no stream authentication
	return true;
}

bool EggDecrypt::Update( BYTE* buf, DWORD n )
{
	if( m_method == EGG_CRYPT_ZIP )
	{
		m_zip.DecodeBuffer( buf, (int)n );
		return true;
	}
	if( m_useAes )
	{
		iz_aes_decrypt( &m_aes, buf, (size_t)n );
		return true;
	}
	if( m_useLea )
	{
		BYTE ks[16];
		DWORD off, i;

		for( off = 0; off < n; off += 16 )
		{
			DWORD take = n - off < 16 ? n - off : 16;

			m_lea.EncryptBlock( m_ctr, ks );
			for( i = 0; i < take; i++ )
				buf[off + i] ^= ks[i];
			// LEA increments its 128-bit counter big-endian.
			for( i = 16; i-- > 0; )
			{
				if( ++m_ctr[i] )
					break;
			}
		}
		return true;
	}
	return false;
}
