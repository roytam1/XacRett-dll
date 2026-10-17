#include "stdafx.h"
#include "LzDecode.h"

#include <stdlib.h>
#include <string.h>

extern "C" {
#include "lzma/LzmaDec.h"
}


// lzip member header / trailer sizes (see the format spec)
#define LZ_HDR 6
#define LZ_TRL 20
#define LZ_BUF 0x20000

// House dictionary cap, identical to LzmaDecodeStream's: the coded size is
// clamped into 4 KiB .. 64 MiB.  A stream needing more than that cannot be
// decoded here, but the CRC check means a wrong result is rejected rather
// than emitted silently.
#define LZ_DIC_MIN 0x1000
#define LZ_DIC_MAX 0x4000000

// lzip always uses the default LZMA encoder parameters lc=3, lp=0, pb=2,
// which encode to a single byte as (pb*5 + lp)*9 + lc.
#define LZ_PROPS_DEFAULT 0x5D


// ---- ISzAlloc for LzmaDec (malloc backed; no file I/O, no globals) ----

static void *lzDecAlloc( ISzAllocPtr p, size_t size )
	{ (void)p; return malloc(size); }

static void lzDecFree( ISzAllocPtr p, void *addr )
	{ (void)p; free(addr); }

static ISzAlloc g_lzDecAlloc = { lzDecAlloc, lzDecFree };


// ---- CRC-32, reflected 0xEDB88320 (the standard zlib CRC-32) ----
// lzip's CRC is this same CRC; it only differs from zlib in that the trailer
// stores it little-endian.  Verified against a real lzip file.

static DWORD lz_crc_tab[256];
static int  lz_crc_ready = 0;

static void lz_crc_init()
{
	DWORD n, c;
	int  k;
	for( n = 0; n < 256; n++ )
		{
		c = n;
		for( k = 0; k < 8; k++ )
			c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
		lz_crc_tab[n] = c;
		}
	lz_crc_ready = 1;
}

// chaining form, so lz_crc(lz_crc(0,a,n),b,m) == lz_crc(0,a,n+m)
static DWORD lz_crc( DWORD crc, const BYTE *p, size_t n )
{
	crc = ~crc;
	while( n-- )
		crc = lz_crc_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	return ~crc;
}


// ---- little-endian field readers ----

static DWORD lz_get32( const BYTE *p )
{
	return (DWORD)p[0] | ((DWORD)p[1]<<8) | ((DWORD)p[2]<<16) | ((DWORD)p[3]<<24);
}

static LzU64 lz_get64( const BYTE *p )
{
	LzU64 v = 0;
	int i;
	for( i = 7; i >= 0; i-- )
		v = (v << 8) | (LzU64)p[i];
	return v;
}


// ---- pull side: a byte stream that knows exactly how much was consumed ----

struct LzIn
	{
	LzFillFn fill;
	void     *ctx;
	BYTE     *buf;
	int       len, pos, eof;
	LzU64 off;          // bytes handed to the caller so far
	};

static int LzAvail( LzIn *s )
	{ return s->len - s->pos; }

static int LzPull( LzIn *s )
{
	int got;
	if( s->pos )
		{
		memmove( s->buf, s->buf + s->pos, (size_t)(s->len - s->pos) );
		s->len -= s->pos;
		s->pos  = 0;
		}
	if( s->len >= LZ_BUF )
		return 0;
	got = s->fill( s->ctx, s->buf + s->len, LZ_BUF - s->len );
	if( got > 0 )
		s->len += got;
	else
		s->eof = 1;
	return got;
}

// Look at n bytes without consuming them.  Returns NULL if fewer than n are
// available.  The pointer is only valid until the next LzPull.
static const BYTE *LzPeek( LzIn *s, int n )
{
	while( LzAvail(s) < n && !s->eof )
		if( LzPull(s) <= 0 && LzAvail(s) < n )
			break;
	return LzAvail(s) >= n ? s->buf + s->pos : NULL;
}

// Consume exactly n bytes; returns how many were actually available.
static int LzRead( LzIn *s, BYTE *dst, int n )
{
	int a;
	while( LzAvail(s) < n && !s->eof )
		if( LzPull(s) <= 0 )
			break;
	a = LzAvail(s);
	if( a > n )
		a = n;
	if( a > 0 )
		{
		memcpy( dst, s->buf + s->pos, (size_t)a );
		s->pos += a;
		s->off += (LzU64)a;
		}
	return a;
}


// ---- write side, accumulating the per-member CRC and size ----

struct LzOut
	{
	LzWriteFn  write;
	void      *ctx;
	DWORD      crc;         // this member
	LzU64  total;       // this member
	LzU64  grand;       // whole file
	};

static void LzEmit( LzOut *o, const BYTE *p, int n )
{
	if( n <= 0 )
		return;
	o->crc   = lz_crc( o->crc, p, (size_t)n );
	o->total += (LzU64)n;
	o->write( o->ctx, p, n );
}


// DS holds the base-2 log of the dictionary base size in bits 4-0 (legal
// 12..29) and a numerator in bits 7-5 that is subtracted as
// frac * 2^(base-4).  Example from the spec: 0xD3 = 2^19 - 6*2^15 = 320 KiB.
static int LzDictSize( BYTE ds, DWORD *pDic )
{
	int       base_log = ds & 0x1F;
	LzU64 frac     = (LzU64)(ds >> 5);
	LzU64 base, dic;
	if( base_log < 12 || base_log > 29 )
		return 0;
	base = (LzU64)1 << base_log;
	dic  = base - (frac << (base_log - 4));
	if( dic < LZ_DIC_MIN )
		dic = LZ_DIC_MIN;
	if( dic > LZ_DIC_MAX )
		dic = LZ_DIC_MAX;
	*pDic = (DWORD)dic;
	return 1;
}


// Decode one member: 6-byte header, the LZMA stream up to its End Of Stream
// marker, then the 20-byte trailer.  All three trailer factors are checked.
static int LzMember( LzIn *s, LzOut *o )
{
	BYTE      hdr[LZ_HDR];
	BYTE      props[5];
	BYTE      trl[LZ_TRL];
	CLzmaDec  st;
	ELzmaStatus stat;
	SRes      r;
	DWORD     dic = 0;
	LzU64 mstart, mCrc, mTotal, dSize, mSize;
	SizeT     inPos, dicPrev, dicLimit;
	int       ok = 0;

	mstart = s->off;

	if( LzRead(s, hdr, LZ_HDR) != LZ_HDR )
		return 0;
	if( hdr[0]!='L' || hdr[1]!='Z' || hdr[2]!='I' || hdr[3]!='P' )
		return 0;
	if( hdr[4] != 1 )                 // version 1 for now
		return 0;
	if( !LzDictSize(hdr[5], &dic) )
		return 0;

	props[0] = LZ_PROPS_DEFAULT;
	props[1] = (BYTE)(dic);
	props[2] = (BYTE)(dic >> 8);
	props[3] = (BYTE)(dic >> 16);
	props[4] = (BYTE)(dic >> 24);

	LzmaDec_Construct( &st );
	if( LzmaDec_Allocate( &st, props, 5, &g_lzDecAlloc ) != SZ_OK )
		return 0;
	LzmaDec_Init( &st );

	o->crc   = 0;
	o->total = 0;
	stat = LZMA_STATUS_NOT_SPECIFIED;

	for(;;)
		{
		if( LzAvail(s) == 0 )
			{
			if( s->eof || LzPull(s) <= 0 )
				break;                          // truncated stream
			}
		dicLimit = st.dicBufSize - st.dicPos;
		if( dicLimit == 0 )
			{
			LzEmit( o, st.dic, (int)st.dicPos );
			st.dicPos = 0;
			dicLimit = st.dicBufSize;
			}
		dicPrev = st.dicPos;
		// The available input count goes IN through this parameter and
		// comes back as the number of bytes actually consumed -- which is
		// exactly the figure the member-size check depends on.
		inPos   = (SizeT)LzAvail(s);
		r = LzmaDec_DecodeToDic( &st, dicLimit, s->buf + s->pos, &inPos,
		                         LZMA_FINISH_ANY, &stat );
		s->pos += (int)inPos;
		s->off += (LzU64)inPos;
		if( r != SZ_OK )
			break;
		if( stat == LZMA_STATUS_FINISHED_WITH_MARK )
			{
			ok = 1;                            // the EOS marker lzip mandates
			break;
			}
		if( inPos == 0 && st.dicPos == dicPrev )
			break;                              // no progress: corrupt input
		}

	if( st.dicPos > 0 )
		{
		LzEmit( o, st.dic, (int)st.dicPos );
		st.dicPos = 0;
		}
	mCrc   = o->crc;
	mTotal = o->total;
	LzmaDec_Free( &st, &g_lzDecAlloc );

	if( !ok )
		return 0;

	if( LzRead(s, trl, LZ_TRL) != LZ_TRL )
		return 0;
	dSize = lz_get64( trl + 4 );
	mSize = lz_get64( trl + 12 );

	if( lz_get32(trl) != mCrc )              // factor 1: CRC of the data
		return 0;
	if( dSize != mTotal )                    // factor 2: uncompressed size
		return 0;
	if( mSize != (s->off - mstart) )         // factor 3: total member size
		return 0;

	o->grand += mTotal;
	return 1;
}


bool LzDecodeStream( LzFillFn fill, void *ictx, LzWriteFn write, void *octx )
{
	LzIn      s;
	LzOut     o;
	BYTE     *buf;
	DWORD     nmem = 0;
	bool      ok   = true;

	if( !lz_crc_ready )
		lz_crc_init();

	buf = new BYTE[LZ_BUF];
	if( !buf )
		return false;

	s.fill = fill;
	s.ctx  = ictx;
	s.buf  = buf;
	s.len  = s.pos = 0;
	s.eof  = 0;
	s.off  = 0;

	o.write  = write;
	o.ctx    = octx;
	o.crc    = 0;
	o.total  = 0;
	o.grand  = 0;

	for(;;)
		{
		const BYTE *m = LzPeek( &s, 4 );
		if( !m )
			break;                                  // clean end of file
		if( m[0]!='L' || m[1]!='Z' || m[2]!='I' || m[3]!='P' )
			{
			if( nmem > 0 )
				break;                              // trailing data: ignore
			ok = false;                             // not a lzip stream
			break;
			}
		if( !LzMember( &s, &o ) )
			{
			ok = false;
			break;
			}
		nmem++;
		}

	delete [] buf;
	return ok && nmem > 0;
}


// ---- file-backed helpers ----

static int lzFileFill( void *ctx, BYTE *buf, int want )
{
	int n = (int)fread( buf, 1, (size_t)want, (FILE*)ctx );
	return n > 0 ? n : 0;
}

static void lzFileWrite( void *ctx, const BYTE *buf, int len )
{
	fwrite( buf, 1, (size_t)len, (FILE*)ctx );
}

bool LzDecodeFile( const char *src, const char *dst )
{
	FILE *fin, *fout;
	bool  ok;

	fin = fopen( src, "rb" );
	if( !fin )
		return false;
	fout = fopen( dst, "wb" );
	if( !fout )
		{
		fclose( fin );
		return false;
		}
	ok = LzDecodeStream( lzFileFill, fin, lzFileWrite, fout );
	fclose( fin );
	fclose( fout );
	if( !ok )
		DeleteFileA( dst );     // never leave a plausible-looking partial file
	return ok;
}


// Read the member sizes out of the trailer index, without decoding.  Walking
// backwards from the end is exactly how lzip builds its own index, and it is
// the only way to get the sizes cheaply because the compressed length of a
// member is not stored anywhere.
bool LzScanFile( const char *src, LzU64 *pTotal, DWORD *pMembers )
{
	FILE      *fp;
	LzU64  fsize, pos, total = 0;
	DWORD      nmem = 0;
	BYTE       trl[LZ_TRL];
	bool       ok = true;

	*pTotal   = 0;
	*pMembers = 0;

	fp = fopen( src, "rb" );
	if( !fp )
		return false;
	fseek( fp, 0, SEEK_END );
	fsize = (LzU64)ftell( fp );
	pos   = fsize;

	while( pos > 0 )
		{
		LzU64 msize, dsize;
		BYTE       magic[4];

		if( pos < (LzU64)LZ_TRL )
			{
			ok = false;                         // trailing bytes, not a member
			break;
			}
		if( fseek( fp, (long)(pos - LZ_TRL), SEEK_SET ) != 0 ||
		    fread( trl, 1, LZ_TRL, fp ) != LZ_TRL )
			{
			ok = false;
			break;
			}
		msize = lz_get64( trl + 12 );
		dsize = lz_get64( trl + 4);
		if( msize < (LzU64)(LZ_HDR + LZ_TRL) || msize > pos )
			{
			ok = false;
			break;
			}
		// the member must really start where the index claims
		if( fseek( fp, (long)(pos - msize), SEEK_SET ) != 0 ||
		    fread( magic, 1, 4, fp ) != 4 ||
		    magic[0]!='L' || magic[1]!='Z' || magic[2]!='I' || magic[3]!='P' )
			{
			ok = false;
			break;
			}
		total += dsize;
		nmem++;
		pos   -= msize;
		}

	fclose( fp );

	if( ok && nmem > 0 && pos == 0 )
		{
		*pTotal   = total;
		*pMembers = nmem;
		return true;
		}
	*pTotal   = 0;
	*pMembers = 0;
	return false;
}