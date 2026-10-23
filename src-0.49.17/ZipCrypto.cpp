#include "stdafx.h"
#include "ZipCrypto.h"

//-- PKZIP stream cipher -----------------------
//
// The table is the reflected CRC-32 polynomial, built once.  It is shared by
// the cipher (raw steps) and by the ordinary CRC-32 the providers compute on
// their output, so both stay in step with the ZIP family for free.

static DWORD g_zipCrcTable[256];
static int   g_zipCrcReady;

static void ZipCrcInit( void )
{
	DWORD c, n, k;

	for( n = 0; n < 256; n++ )
	{
		c = n;
		for( k = 8; k; k-- )
			c = c & 1 ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
		g_zipCrcTable[n] = c;
	}
	g_zipCrcReady = 1;
}

static void ZipCrcCheck( void )
{
	if( !g_zipCrcReady )
		ZipCrcInit();
}

DWORD ZipCrypto::CrcStep( DWORD crc, BYTE b )
{
	ZipCrcCheck();
	return g_zipCrcTable[(crc ^ b) & 0xFF] ^ (crc >> 8);
}

DWORD ZipCrypto::Crc( DWORD crc, const BYTE* p, int len )
{
	crc ^= 0xFFFFFFFFL;
	while( len-- )
		crc = CrcStep( crc, *p++ );
	return crc ^ 0xFFFFFFFFL;
}

void ZipCrypto::Init( const char* password )
{
	m_key[0] = 305419896L;      // 0x12345678
	m_key[1] = 591751049L;      // 0x23456789
	m_key[2] = 878082192L;      // 0x34567890
	if( !password )
		return;
	while( *password )
		Update( (BYTE)*password++ );
}

BYTE ZipCrypto::Decode( BYTE c )
{
	unsigned temp = (unsigned)(m_key[2] & 0xFFFF) | 2;
	BYTE     k = (BYTE)((temp * (temp ^ 1)) >> 8);

	c = (BYTE)(c ^ k);
	Update( c );
	return c;
}

void ZipCrypto::DecodeBuffer( BYTE* buf, int len )
{
	while( len-- > 0 )
	{
		unsigned temp = (unsigned)(m_key[2] & 0xFFFF) | 2;
		BYTE     c = (BYTE)(*buf ^ (BYTE)((temp * (temp ^ 1)) >> 8));

		*buf++ = c;
		Update( c );
	}
}
