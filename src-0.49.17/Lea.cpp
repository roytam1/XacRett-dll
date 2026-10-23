#include "stdafx.h"
#include "Lea.h"
#include <string.h>

//-- LEA-128 / LEA-256 -----------------------
//
// 32-bit ARX, four state words, six words per round key set.  The constants
// come from the hexadecimal expansion of sqrt(766995); 76/69/95 are the
// ASCII codes of "L", "E", "A".  The EGG CTR mode only needs the forward
// function.

static const DWORD lea_delta[8] = {
	0xC3EFE9DBu, 0x44626B02u, 0x79E27C8Au, 0x78DF30ECu,
	0x715EA49Eu, 0xC785DA0Au, 0xE04EF22Au, 0xE5C40957u
};

// Rotations by position in the round key set, whatever the key size:
// 1, 3, 6, 11, 13, 17 (KS X 3246, 2.2).
static const unsigned lea_rot[6] = { 1, 3, 6, 11, 13, 17 };

static DWORD LeaRol( DWORD x, unsigned n )
{
	n &= 31;
	return n ? (x << n) | (x >> (32 - n)) : x;
}

static DWORD LeaRor( DWORD x, unsigned n )
{
	n &= 31;
	return n ? (x >> n) | (x << (32 - n)) : x;
}

static DWORD LeaGet32( const BYTE* p )
{
	return (DWORD)p[0] | ((DWORD)p[1] << 8) |
	       ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
}

static void LeaPut32( BYTE* p, DWORD v )
{
	p[0] = (BYTE)v;
	p[1] = (BYTE)(v >> 8);
	p[2] = (BYTE)(v >> 16);
	p[3] = (BYTE)(v >> 24);
}

void Lea::SetKey( const BYTE* key, int keylen )
{
	DWORD t[8];
	int   n, i, j;

	memset( m_rk, 0, sizeof(m_rk) );
	m_rounds = 0;
	if( keylen != 16 && keylen != 32 )
		return;
	n = keylen / 4;                     // 4 or 8 words
	m_rounds = keylen == 16 ? 24 : 32;
	for( i = 0; i < n; i++ )
		t[i] = LeaGet32( key + 4 * i );

	for( i = 0; i < m_rounds; i++ )
	{
		if( n == 4 )
		{
			for( j = 0; j < 4; j++ )
				t[j] = LeaRol( t[j] + LeaRol( lea_delta[i % 4],
				               (unsigned)(i + j) ), lea_rot[j] );
			// LEA-128 reuses T1 three times.
			m_rk[i][0] = t[0];
			m_rk[i][1] = t[1];
			m_rk[i][2] = t[2];
			m_rk[i][3] = t[1];
			m_rk[i][4] = t[3];
			m_rk[i][5] = t[1];
		}
		else
		{
			for( j = 0; j < 6; j++ )
			{
				int k = (6 * i + j) % 8;

				t[k] = LeaRol( t[k] + LeaRol( lea_delta[i % 8],
				               (unsigned)(i + j) ), lea_rot[j] );
			}
			for( j = 0; j < 6; j++ )
				m_rk[i][j] = t[(6 * i + j) % 8];
		}
	}
	memset( t, 0, sizeof(t) );
}

void Lea::EncryptBlock( const BYTE in[16], BYTE out[16] ) const
{
	DWORD x[4];
	int   i;

	for( i = 0; i < 4; i++ )
		x[i] = LeaGet32( in + 4 * i );
	for( i = 0; i < m_rounds; i++ )
	{
		const DWORD* k = m_rk[i];
		DWORD t0 = LeaRol( (x[0] ^ k[0]) + (x[1] ^ k[1]), 9 );
		DWORD t1 = LeaRor( (x[1] ^ k[2]) + (x[2] ^ k[3]), 5 );
		DWORD t2 = LeaRor( (x[2] ^ k[4]) + (x[3] ^ k[5]), 3 );

		x[3] = x[0];                // the old X0 becomes X3
		x[0] = t0;
		x[1] = t1;
		x[2] = t2;
	}
	for( i = 0; i < 4; i++ )
		LeaPut32( out + 4 * i, x[i] );
	memset( x, 0, sizeof(x) );
}
