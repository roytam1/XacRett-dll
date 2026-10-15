#include "stdafx.h"
#include "SevenZCore.h"
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

extern "C" {
#include "sevenz/Aes.h"
#include "sevenz/Sha256.h"
#include "sevenz/Bcj2.h"
#include "sevenz/Bra.h"
#include "sevenz/Delta.h"
#include "sevenz/Ppmd7.h"
}

// ---- ISzAlloc (malloc backed, shared by the SDK cores) ----
static void *szcAlloc( ISzAllocPtr p, size_t size )
	{ (void)p; return malloc(size); }
static void szcFree( ISzAllocPtr p, void *addr )
	{ (void)p; free(addr); }
static ISzAlloc g_szcAlloc = { szcAlloc, szcFree };

// =========================================================================
// 7zAES key derivation (SHA-256 based KDF, ported from 7zAes.cpp)
//
// The KDF hashes salt || password || LE64 round-counter, repeated
// 1<<cyclesPower times.  The password must already be UTF-16LE; 7-Zip
// converts its ANSI input with the ACP before hashing, so hashing the
// raw single-byte string yields a key that matches nothing.  The
// cyclesPower == 0x3F short form hashes nothing at all and just takes
// salt || password, zero padded to 32 bytes.
// =========================================================================

// 7-Zip unrolls the buffer up to 64 times to feed SHA-256 in big blocks.
#define SZ_AES_KDF_MAX_UNROLL 6

static void szcSetUi32( Byte* p, UInt32 v )
{
	p[0] = (Byte)v;
	p[1] = (Byte)(v >> 8);
	p[2] = (Byte)(v >> 16);
	p[3] = (Byte)(v >> 24);
}

bool SevenZAesDeriveKey( const BYTE* password, size_t passwordSize,
                         unsigned cyclesPower,
                         const BYTE* salt, unsigned saltSize, BYTE key[32] )
{
	CSha256 sha;
	Byte* buf;
	size_t pwLen, bufSize, unrollSize;
	UInt32 numUnroll, numRounds, r;

	if( cyclesPower > 24 && cyclesPower != 0x3F )
		return false;
	if( saltSize > 16 )
		return false;
	// SHA-256 must be prepared once before any Sha256_Init (see
	// sevenz/KI.README); without it the update blocks are not wired up.
	{
	static BYTE prep = 0;
	if( !prep ) { Sha256Prepare(); prep = 1; }
	}
	pwLen = passwordSize;

	if( cyclesPower == 0x3F )
		{
		// No hashing: salt || password, zero padded / truncated to 32.
		unsigned pos;
		memset( key, 0, 32 );
		for( pos = 0; pos < saltSize; pos++ )
			key[pos] = salt[pos];
		for( pos = 0; pos < pwLen && pos + saltSize < 32; pos++ )
			key[pos + saltSize] = (Byte)password[pos];
		return true;
		}

	bufSize = 8 + (size_t)saltSize + pwLen;
	numUnroll = (UInt32)1 << (cyclesPower <= SZ_AES_KDF_MAX_UNROLL
	                          ? cyclesPower : SZ_AES_KDF_MAX_UNROLL);
	unrollSize = bufSize * numUnroll;
	buf = (Byte*)malloc( bufSize + unrollSize );
	if( !buf )
		return false;

	memcpy( buf, salt, saltSize );
	if( pwLen )
		memcpy( buf + saltSize, password, pwLen );
	memset( buf + bufSize - 8, 0, 8 );

	Sha256_Init( &sha );

	// Pre-fill the unrolled copies so every round writes only the counter.
	{
		Byte* dest = buf;
		UInt32 i;
		for( i = 1; i < numUnroll; i++ )
			{
			dest += bufSize;
			memcpy( dest, buf, bufSize );
			}
	}

	numRounds = (UInt32)1 << cyclesPower;
	r = 0;
	do
		{
		Byte* dest = buf + bufSize - 8;
		UInt32 i = r;
		r += numUnroll;
		do
			{
			szcSetUi32( dest, i ); i++; dest += bufSize;
			}
		while( i < r );
		Sha256_Update( &sha, buf, unrollSize );
		}
	while( r < numRounds );

	Sha256_Final( &sha, key );
	memset( buf, 0, bufSize + unrollSize );
	free( buf );
	return true;
}

// =========================================================================
// AES-256-CBC decrypt
// =========================================================================

// The SDK keeps [iv(4) | keyMode+roundKeys(60)] in one 16-byte aligned
// UInt32 array; the key schedule itself must start at Aes()+4 (see
// sevenz/KI.README - under-sizing or mis-placing this is a hard fault).
static void szcAesBuf( UInt32* buf )
	{ memset( buf, 0, (AES_NUM_IVMRK_WORDS + 3) * sizeof(UInt32) ); }

void SevenZAesDecryptCbc( const BYTE key[32], const BYTE iv[16],
                          BYTE* data, DWORD len )
{
	static BYTE g_aesGenDone = 0;
	UInt32* st;
	if( len == 0 )
		return;
	if( !g_aesGenDone )
		{ AesGenTables(); g_aesGenDone = 1; }
	// The SDK requires the AES state buffer to be 16-byte aligned; the
	// stack only guarantees 8 on this compiler, so align it explicitly.
#if defined(_MSC_VER) && (_MSC_VER < 1200)
	/* No __declspec(align) on VC4: over-allocate and round up. */
	UInt32 unaligned[AES_NUM_IVMRK_WORDS + 7];
	st = (UInt32*)( ((unsigned long)unaligned + 15) & ~(unsigned long)15 );
#else
	__declspec(align(16)) UInt32 local[AES_NUM_IVMRK_WORDS + 3];
	st = local;
#endif
	szcAesBuf( st );
	AesCbc_Init( st, iv );
	Aes_SetKey_Dec( st + 4, key, 32 );
	g_AesCbc_Decode( st, data, len / AES_BLOCK_SIZE );
}

void SevenZSha256( const BYTE* data, DWORD len, BYTE digest[32] )
{
	static BYTE g_shaPrepDone = 0;
	CSha256 sha;
	if( !g_shaPrepDone )
		{ Sha256Prepare(); g_shaPrepDone = 1; }
	Sha256_Init( &sha );
	if( len )
		Sha256_Update( &sha, data, len );
	Sha256_Final( &sha, digest );
}

// =========================================================================
// Branch converters
// =========================================================================

bool SevenZBranchDecode( DWORD coderId, BYTE* data, DWORD size )
{
	if( size == 0 )
		return true;

	// x86 is the only stateful converter; 7-Zip runs it once over the whole
	// folder stream with pc = 0 and the initial state.
	if( coderId == 0x03030103 || coderId == 0x04 )
		{
		UInt32 state = Z7_BRANCH_CONV_ST_X86_STATE_INIT_VAL;
		z7_BranchConvSt_X86_Dec( data, (SizeT)size, 0, &state );
		return true;
		}
	if( coderId == 0x03030501 || coderId == 0x07 ) { z7_BranchConv_ARM_Dec( data, size, 0 ); return true; }
	if( coderId == 0x03030701 || coderId == 0x08 ) { z7_BranchConv_ARMT_Dec( data, size, 0 ); return true; }
	if( coderId == 0x03030205 || coderId == 0x05 ) { z7_BranchConv_PPC_Dec( data, size, 0 ); return true; }
	if( coderId == 0x03030805 || coderId == 0x09 ) { z7_BranchConv_SPARC_Dec( data, size, 0 ); return true; }
	if( coderId == 0x03030401 || coderId == 0x06 ) { z7_BranchConv_IA64_Dec( data, size, 0 ); return true; }
	if( coderId == 0x0A ) { z7_BranchConv_ARM64_Dec( data, size, 0 ); return true; }
	if( coderId == 0x0B ) { z7_BranchConv_RISCV_Dec( data, size, 0 ); return true; }
	return false;
}

// =========================================================================
// BCJ2
// =========================================================================

int SevenZBcj2Decode( const BYTE* main, DWORD mainLen,
                       const BYTE* call, DWORD callLen,
                       const BYTE* jump, DWORD jumpLen,
                       const BYTE* rc,   DWORD rcLen,
                       BYTE* dest, DWORD destLen )
{
	CBcj2Dec p;

	// Bcj2Dec requires the CALL/JUMP buffer sizes to be multiples of 4.
	if( (callLen & 3) || (jumpLen & 3) )
		return 1;
	if( destLen > (mainLen + callLen + jumpLen) )
		return 2;

	memset( &p, 0, sizeof(p) );
	p.bufs[BCJ2_STREAM_MAIN] = main;  p.lims[BCJ2_STREAM_MAIN]  = main  + mainLen;
	p.bufs[BCJ2_STREAM_CALL] = call;  p.lims[BCJ2_STREAM_CALL]  = call  + callLen;
	p.bufs[BCJ2_STREAM_JUMP] = jump;  p.lims[BCJ2_STREAM_JUMP]  = jump  + jumpLen;
	p.bufs[BCJ2_STREAM_RC]   = rc;    p.lims[BCJ2_STREAM_RC]    = rc    + rcLen;
	p.dest    = dest;
	p.destLim = dest + destLen;

	Bcj2Dec_Init( &p );
	// ip is a property of the starting base; Bcj2Dec_Init already set 0.
	if( SZ_OK != Bcj2Dec_Decode( &p ) )
		return 3;
	// Bcj2.h: the mandatory completion check is on the size of the produced
	// ORIG (or consumed MAIN) stream - `state` is NOT a finished indicator.
	if( (DWORD)(p.dest - dest) != destLen )
		return 4;
	if( p.state == BCJ2_DEC_STATE_ERROR )
		return 5;
	return 0;
}

// =========================================================================
// Delta
// =========================================================================

void SevenZDeltaDecode( unsigned delta, BYTE* data, DWORD size )
{
	Byte state[DELTA_STATE_SIZE];
	if( size == 0 )
		return;
	Delta_Init( state );
	Delta_Decode( state, delta, data, (SizeT)size );
}

// =========================================================================
// PPMd var. H (7z range coder)
// =========================================================================

// The SDK IByteIn is a direct-call interface (IByteIn_Read(p) is p->Read(p)),
// so the callback must be a real first member of the object we hand over.
struct SzcPpmdIn
{
	IByteIn in;
	const BYTE* buf;
	DWORD len;
	DWORD pos;
	bool  err;
};

static Byte szcPpmdRead( IByteInPtr p )
{
	SzcPpmdIn* io = (SzcPpmdIn*)(void*)p;
	if( io->pos >= io->len )
		{ io->err = true; return 0; }
	return io->buf[io->pos++];
}

bool SevenZPpmd7Decode( const BYTE* props, DWORD propsSize,
                        const BYTE* src, DWORD srcLen,
                        BYTE* dest, DWORD destLen )
{
	CPpmd7 model;
	SzcPpmdIn in;
	unsigned order;
	UInt32 mem;
	DWORD done = 0;

	if( propsSize != 5 )
		return false;
	order = props[0];
	mem = (UInt32)props[1] | ((UInt32)props[2] << 8) |
	      ((UInt32)props[3] << 16) | ((UInt32)props[4] << 24);
	if( order < PPMD7_MIN_ORDER || order > PPMD7_MAX_ORDER )
		return false;
	if( mem < (1 << 11) )
		return false;

	Ppmd7_Construct( &model );
	if( !Ppmd7_Alloc( &model, mem, &g_szcAlloc ) )
		return false;

	in.in.Read = szcPpmdRead;
	in.buf = src; in.len = srcLen; in.pos = 0; in.err = false;
	model.rc.dec.Stream = (IByteInPtr)&in;
	if( !Ppmd7z_RangeDec_Init( &model.rc.dec ) )
		{ Ppmd7_Free( &model, &g_szcAlloc ); return false; }
	Ppmd7_Init( &model, order );

	while( done < destLen )
		{
		int sym = Ppmd7z_DecodeSymbol( &model );
		if( sym < 0 || in.err )
			break;
		dest[done++] = (BYTE)sym;
		}
	if( done != destLen || in.err )
		{ Ppmd7_Free( &model, &g_szcAlloc ); return false; }

	// A 7z PPMd stream carries no end marker: the encoder only flushes the
	// range coder after the last symbol, so Code ends at whatever that flush
	// produced and any input tail is renormalisation.  The only honest
	// checks are the exact output length and that the range coder never
	// had to read past the packed stream; correctness of the bytes is
	// covered by the entry CRCs (and by the header CRC for an encoded
	// header).

	Ppmd7_Free( &model, &g_szcAlloc );
	return true;
}