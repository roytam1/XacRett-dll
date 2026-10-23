#ifndef AFX_LEA_H__LEA_BLOCK_CIPHER_INCLUDED__
#define AFX_LEA_H__LEA_BLOCK_CIPHER_INCLUDED__

#include "Tool.h"

// LEA-128 / LEA-256 block cipher (KS X 3246, TTAK.KO-12.0223).
//
// Only the forward direction is needed: EGG uses LEA in counter mode, and
// CTR encrypts the counter block with the forward function.  Transliterated
// from the reference decoder in alzgg/src/egg_lea.c.
class Lea
{
public:
	Lea()
		{ m_rounds = 0; }

	// keylen is 16 (LEA-128, 24 rounds) or 32 (LEA-256, 32 rounds).
	void SetKey( const BYTE* key, int keylen );

	// Encrypts one 16-byte block; in and out may be the same buffer.
	void EncryptBlock( const BYTE in[16], BYTE out[16] ) const;

private:
	DWORD m_rk[32][6];      // one set of six words per round
	int   m_rounds;
};

#endif
