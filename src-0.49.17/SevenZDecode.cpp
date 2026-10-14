#include "stdafx.h"
#include "SevenZDecode.h"
#include "SevenZCore.h"
#include "kiutil.h"
#include "libbzip2/bzlib.h"
#include "zlib/zlib.h"
#include "zstd/zstd.h"
#include <stdlib.h>
#include <string.h>
extern "C" {
#include "lzma/LzmaDec.h"
#include "lzma/Lzma2Dec.h"
}

// ---- limits (all refused loudly, never trusted) ----
#define SZ_MAX_PACK		0x10000UL
#define SZ_MAX_FOLDERS	0x10000UL
#define SZ_MAX_FILES	0x20000UL
#define SZ_MAX_SUBS		0x40000UL
#define SZ_MAX_CODERS	16

// Dictionary / KDF ceilings for a 32-bit DLL.
#define SZ_MAX_LZMA2_PROP	28			// (2|(p&1)) << (p/2+11) <= 64 MB

// ---- property ids (7zFormat.txt) ----
#define SZ_kEnd				0x00
#define SZ_kHeader			0x01
#define SZ_kArchiveProps	0x02
#define SZ_kAddStreamsInfo	0x03
#define SZ_kMainStreamsInfo	0x04
#define SZ_kFilesInfo		0x05
#define SZ_kPackInfo		0x06
#define SZ_kUnpackInfo		0x07
#define SZ_kSubStreamsInfo	0x08
#define SZ_kSize			0x09
#define SZ_kCRC				0x0A
#define SZ_kFolder			0x0B
#define SZ_kCodersUnpackSz	0x0C
#define SZ_kNumUnpackStream	0x0D
#define SZ_kEmptyStream		0x0E
#define SZ_kEmptyFile		0x0F
#define SZ_kAnti			0x10
#define SZ_kName			0x11
#define SZ_kCTime			0x12
#define SZ_kATime			0x13
#define SZ_kMTime			0x14
#define SZ_kWinAttrib		0x15
#define SZ_kEncodedHeader	0x17

// ---- coder ids (Methods.txt) ----
#define SZ_COPY			0x00000000UL
#define SZ_DELTA		0x00000003UL
#define SZ_LZMA2		0x00000021UL
#define SZ_LZMA			0x030101UL
#define SZ_BCJ			0x03030103UL
#define SZ_BCJ2			0x0303011BUL
#define SZ_PPMD7		0x030401UL
#define SZ_DEFLATE		0x040108UL
#define SZ_BZIP2		0x040202UL
#define SZ_ZSTD		0x04F71101UL
#define SZ_AES256		0x06F10701UL

static const BYTE SZ_SIG[6] = { '7','z',0xBC,0xAF,0x27,0x1C };

// ---- ISzAlloc for the lzma cores (malloc backed) ----
static void *szAlloc( ISzAllocPtr p, size_t size )
	{ (void)p; return malloc(size); }
static void szFree( ISzAllocPtr p, void *addr )
	{ (void)p; free(addr); }
static ISzAlloc g_szAlloc = { szAlloc, szFree };

static DWORD szCrcTab[256];
static bool szCrcTabReady = false;

static void szCrcInit()
{
	DWORD i, j, c;
	if( szCrcTabReady ) return;
	for( i = 0; i < 256; i++ )
		{
		c = i;
		for( j = 0; j < 8; j++ )
			c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
		szCrcTab[i] = c;
		}
	szCrcTabReady = true;
}

static DWORD szCrc32( const BYTE* b, DWORD n )
{
	DWORD c = 0xFFFFFFFFUL;
	szCrcInit();
	while( n-- )
		c = szCrcTab[(c ^ *b++) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFUL;
}

// =========================================================================
// primitive readers
// =========================================================================

// Where the last low-level read failed, so an empty m_err still says
// something useful instead of nothing at all.
static DWORD g_szFailPos = 0;
static bool  g_szFailSet = false;
static void szFail( SzRdr* r )
	{ g_szFailPos = r->p; g_szFailSet = true; }


static bool szRdrU8( SzRdr* r, DWORD* v )
{
	if( r->err || r->p >= r->end ) { r->err = true; szFail(r); return false; }
	*v = r->b[r->p++];
	return true;
}

// UINT64 variable-length number. The first byte carries (7 - continuation
// bits) value bits in its HIGH positions; following bytes are little-endian
// 7-bit groups. Verified against real archives - the "obvious" 7-bits-first
// LEB128 reading is wrong here and yields absurd sizes.
static bool szRdrNum( SzRdr* r, unsigned __int64* v )
{
	unsigned __int64 value = 0;
	DWORD b, mask = 0x80;
	int i;
	if( !szRdrU8(r, &b) ) return false;
	for( i = 0; i < 8; i++ )
		{
		if( !(b & mask) )
			{
			*v = value + (((unsigned __int64)(b & (mask - 1))) << (8 * i));
			return true;
			}
		if( r->p >= r->end ) { r->err = true; szFail(r); return false; }
		value |= ((unsigned __int64)r->b[r->p++]) << (8 * i);
		mask >>= 1;
		}
	*v = value;
	return true;
}

// Every number in this format is a size or an index, so 32 bits is the real
// ceiling; anything wider is refused rather than truncated.
static bool szNum32( SzRdr* r, DWORD* v )
{
	unsigned __int64 t;
	if( !szRdrNum(r, &t) ) return false;
	if( t > 0xFFFFFFFFUL ) { r->err = true; szFail(r); return false; }
	*v = (DWORD)t;
	return true;
}

static bool szRdrU32( SzRdr* r, DWORD* v )
{
	DWORD a, c, d, e;
	if( r->err || r->p + 4 > r->end ) { r->err = true; szFail(r); return false; }
	a = r->b[r->p]; c = r->b[r->p+1]; d = r->b[r->p+2]; e = r->b[r->p+3];
	r->p += 4;
	*v = a | (c << 8) | (d << 16) | (e << 24);
	return true;
}

static bool szRdrU64( SzRdr* r, unsigned __int64* v )
{
	DWORD lo, hi;
	if( !szRdrU32(r, &lo) || !szRdrU32(r, &hi) ) return false;
	*v = ((unsigned __int64)hi << 32) | lo;
	return true;
}

// Bit vector, MSB first within each byte.
// Writes exactly n bits (0/1) into out[0..n-1]. A plain byte array on
// purpose: MSVC6's std::vector<bool> is broken enough to crash here, and
// an exact-length result is what every caller actually wants.
static bool szRdrBits( SzRdr* r, DWORD n, BYTE* out )
{
	DWORD i;
	DWORD b = 0;
	for( i = 0; i < n; i++ )
		{
		if( (i & 7) == 0 )
			{
			if( !szRdrU8(r, &b) ) return false;
			}
		out[i] = (BYTE)((b & 0x80) != 0);
		b = (b << 1) & 0xFF;
		}
	return true;
}

// "AllAreDefined" byte, optional bit vector, then a CRC per defined slot.
bool CSevenZ::readDigests( SzRdr* r, DWORD n, DWORD* out, bool* known )
{
	BYTE* bits;
	DWORD i, all, nbits;

	for( i = 0; i < n; i++ ) { known[i] = false; out[i] = 0; }
	if( !szRdrU8(r, &all) ) return false;
	nbits = 0;
	bits = NULL;
	if( all )
		{
		for( i = 0; i < n; i++ ) known[i] = true;
		}
	else
		{
		nbits = n;
		bits = new BYTE[nbits ? nbits : 1];
		if( !szRdrBits(r, nbits, bits) ) { delete [] bits; return false; }
		}
	for( i = 0; i < n; i++ )
		{
		if( bits && !bits[i] ) continue;
		if( !szRdrU32(r, &out[i]) ) { delete [] bits; return false; }
		known[i] = true;
		}
	delete [] bits;
	return true;
}

// =========================================================================
// construction
// =========================================================================

CSevenZ::CSevenZ()
	: m_fp(NULL), m_fileSize(0), m_packPos(0), m_numPack(0),
	  m_packSizes(NULL), m_packCRC(NULL), m_packCRCKnown(NULL),
	  m_folders(NULL), m_numFolders(0),
	  m_cacheFolder(0xFFFFFFFFUL), m_cache(NULL), m_cacheLen(0),
	  m_password(NULL)
{
	m_err[0] = 0;
}

CSevenZ::~CSevenZ()
{
	Close();
}

void CSevenZ::setErr( const char* e )
{
	if( !m_err[0] )
		lstrcpyn( m_err, e, sizeof(m_err) );
}

void CSevenZ::releaseStreams()
{
	if( m_packSizes ) { delete [] m_packSizes; m_packSizes = NULL; }
	if( m_packCRC ) { delete [] m_packCRC; m_packCRC = NULL; }
	if( m_packCRCKnown ) { delete [] m_packCRCKnown; m_packCRCKnown = NULL; }
	if( m_folders ) { delete [] m_folders; m_folders = NULL; }
	m_numPack = 0;
	m_numFolders = 0;
}

void CSevenZ::Close()
{
	if( m_fp ) { fclose( m_fp ); m_fp = NULL; }
	releaseStreams();
	if( m_cache ) { delete [] m_cache; m_cache = NULL; }
	m_cacheLen = 0;
	m_cacheFolder = 0xFFFFFFFFUL;
	m_entries.clear();
	m_subs.clear();
	if( m_password ) { delete [] m_password; m_password = NULL; }
	// NB: m_err deliberately survives Close() - Open() calls it on the way out
	// and the caller reads Error() afterwards. Open() clears it on entry.
}

void CSevenZ::SetPassword( const char* pw )
{
	if( m_password ) { delete [] m_password; m_password = NULL; }
	if( pw && *pw )
		{
		m_password = new char[strlen(pw) + 1];
		strcpy( m_password, pw );
		}
}

bool CSevenZ::NeedsPassword() const
{
	DWORD i, j;
	for( i = 0; i < m_numFolders; i++ )
		for( j = 0; j < m_folders[i].numCoders; j++ )
			if( m_folders[i].coders[j].id == SZ_AES256 )
				return true;
	return false;
}

bool CSevenZ::Open( const char* fname, const char* password )
{
	BYTE sig[32];
	unsigned __int64 off, size;
	DWORD ncrc;
	BYTE* hdr;
	bool ok;

	Close();
	m_err[0] = 0;
	g_szFailSet = false;

	m_fp = fopen( fname, "rb" );
	if( !m_fp ) { setErr("cannot open"); return false; }
	if( password && *password )
		{
		m_password = new char[strlen(password) + 1];
		strcpy( m_password, password );
		}

	fseek( m_fp, 0, SEEK_END );
	m_fileSize = (DWORD)ftell( m_fp );
	if( m_fileSize < 32 ) { setErr("too small"); Close(); return false; }

	fseek( m_fp, 0, SEEK_SET );
	if( 32 != fread(sig, 1, 32, m_fp) ) { setErr("short read"); Close(); return false; }
	if( memcmp(sig, SZ_SIG, 6) != 0 ) { setErr("bad signature"); Close(); return false; }
	if( sig[6] != 0 ) { setErr("bad version"); Close(); return false; }
	if( szCrc32(sig + 12, 20) != (DWORD)(sig[8]|(sig[9]<<8)|(sig[10]<<16)|((DWORD)sig[11]<<24)) )
		{ setErr("start header crc"); Close(); return false; }

	off  = ((unsigned __int64)sig[12] | ((unsigned __int64)sig[13]<<8) |
	        ((unsigned __int64)sig[14]<<16) | ((unsigned __int64)sig[15]<<24) |
	        ((unsigned __int64)sig[16]<<32) | ((unsigned __int64)sig[17]<<40) |
	        ((unsigned __int64)sig[18]<<48) | ((unsigned __int64)sig[19]<<56));
	size = ((unsigned __int64)sig[20] | ((unsigned __int64)sig[21]<<8) |
	        ((unsigned __int64)sig[22]<<16) | ((unsigned __int64)sig[23]<<24) |
	        ((unsigned __int64)sig[24]<<32) | ((unsigned __int64)sig[25]<<40) |
	        ((unsigned __int64)sig[26]<<48) | ((unsigned __int64)sig[27]<<56));
	ncrc = (DWORD)(sig[28]|(sig[29]<<8)|(sig[30]<<16)|((DWORD)sig[31]<<24));

	// PackPos and NextHeaderOffset are both relative to the end of the
	// 32-byte signature header.
	if( off > 0xFFFFFFFFUL || size == 0 || size > SZ_MAX_HEADER_SIZE ||
	    32 + (DWORD)off + (DWORD)size > m_fileSize )
		{ setErr("bad next header"); Close(); return false; }

	hdr = new BYTE[(DWORD)size];
	fseek( m_fp, (long)(32 + off), SEEK_SET );
	if( (DWORD)size != fread(hdr, 1, (DWORD)size, m_fp) )
		{ delete [] hdr; setErr("header short"); Close(); return false; }
	if( szCrc32(hdr, (DWORD)size) != ncrc )
		{ delete [] hdr; setErr("header crc"); Close(); return false; }

	m_packPos = 32;
	g_szFailSet = false;
	ok = parseHeader( hdr, (DWORD)size, 0 );
	delete [] hdr;
	if( !ok )
		{
		if( !m_err[0] && g_szFailSet )
			wsprintf( m_err, "header parse error at %lu", (unsigned long)g_szFailPos );
		Close();
		return false;
		}
	if( m_entries.empty() ) { setErr("no entries"); Close(); return false; }
	return true;
}

// =========================================================================
// header parsing
// =========================================================================

bool CSevenZ::parseHeader( const BYTE* buf, DWORD len, int depth )
{
	SzRdr r;
	DWORD t, i, nstream;

	r.b = buf; r.p = 0; r.end = len; r.err = false;

	if( depth > 2 ) { setErr("header nesting"); return false; }
	if( !szNum32(&r, &t) ) return false;

	if( t == SZ_kEncodedHeader )
		{
		BYTE* dec;
		DWORD declen;
		if( !decodeEncodedHeader( r.b + r.p, r.end - r.p, &dec, &declen) )
			return false;
		releaseStreams();
		{
		bool ok2 = parseHeader( dec, declen, depth + 1 );
		delete [] dec;
		return ok2;
		}
		}
	if( t != SZ_kHeader ) { setErr("not a header"); return false; }

	if( !szNum32(&r, &t) ) return false;
	if( t == SZ_kArchiveProps )
		{
		DWORD ptype, psz;
		for(;;)
			{
			if( !szNum32(&r, &ptype) ) return false;
			if( ptype == SZ_kEnd ) break;
			if( !szNum32(&r, &psz) || psz > (r.end - r.p) ) return false;
			r.p += psz;
			}
		if( !szNum32(&r, &t) ) return false;
		}
	if( t == SZ_kAddStreamsInfo ) { setErr("additional streams"); return false; }
	if( t == SZ_kMainStreamsInfo )
		{
		if( !readStreamsInfo(&r) ) return false;
		if( !szNum32(&r, &t) ) return false;
		}
	if( t == SZ_kFilesInfo )
		{
		if( !readFilesInfo(&r) ) return false;
		if( !szNum32(&r, &t) ) return false;
		}
	if( t != SZ_kEnd ) { setErr("bad header layout"); return false; }

	// Every file with a data stream must line up 1:1 with a substream.
	nstream = 0;
	for( i = 0; i < (DWORD)m_entries.size(); i++ )
		if( m_entries[i].nFolder != (DWORD)-1 )
			nstream++;
	if( nstream != (DWORD)m_subs.size() )
		{ setErr("file/substream mismatch"); return false; }
	return true;
}

bool CSevenZ::readStreamsInfo( SzRdr* r )
{
	DWORD t;
	if( !szNum32(r, &t) ) return false;
	if( t == SZ_kPackInfo )
		{
		if( !readPackInfo(r) ) return false;
		if( !szNum32(r, &t) ) return false;
		}
	if( t == SZ_kUnpackInfo )
		{
		if( !readUnpackInfo(r) ) return false;
		if( !szNum32(r, &t) ) return false;
		}
	if( t == SZ_kSubStreamsInfo )
		{
		if( !readSubStreamsInfo(r) ) return false;
		if( !szNum32(r, &t) ) return false;
		}
	if( t != SZ_kEnd ) { setErr("bad streams info"); return false; }
	return true;
}

bool CSevenZ::readPackInfo( SzRdr* r )
{
	DWORD packPos, n, i, t;
	unsigned __int64 total = 0;

	if( !szNum32(r, &packPos) || !szNum32(r, &n) ) return false;
	if( n > SZ_MAX_PACK ) { setErr("too many pack streams"); return false; }
	if( !szNum32(r, &t) || t != SZ_kSize ) { setErr("pack: no kSize"); return false; }

	m_numPack = n;
	m_packSizes = new DWORD[n ? n : 1];
	m_packCRC = new DWORD[n ? n : 1];
	m_packCRCKnown = new bool[n ? n : 1];
	for( i = 0; i < n; i++ )
		{
		if( !szNum32(r, &m_packSizes[i]) ) return false;
		total += m_packSizes[i];
		if( total > 0xFFFFFFFFUL ) { setErr("pack too big"); return false; }
		}
	if( 32 + (DWORD)packPos + total > m_fileSize )
		{ setErr("pack past eof"); return false; }
	m_packPos = 32 + packPos;

	if( r->p < r->end && r->b[r->p] == SZ_kCRC )
		{
		r->p++;
		if( !readDigests(r, n, m_packCRC, m_packCRCKnown) ) return false;
		}
	if( !szNum32(r, &t) || t != SZ_kEnd ) { setErr("pack: no kEnd"); return false; }
	return true;
}

bool CSevenZ::readFolder( SzRdr* r, SzFolder* fo )
{
	DWORD i, k, t, nc, flags, idsize;
	bool used[16], packed[16];

	memset( fo, 0, sizeof(SzFolder) );

	if( !szNum32(r, &nc) ) return false;
	if( nc == 0 || nc > SZ_MAX_CODERS ) { setErr("bad coder count"); return false; }
	fo->numCoders = nc;

	for( i = 0; i < nc; i++ )
		{
		SzCoder* c = &fo->coders[i];
		unsigned __int64 id = 0;
		if( !szRdrU8(r, &flags) ) return false;
		idsize = flags & 0x0F;
		for( k = 0; k < idsize; k++ )
			{
			DWORD bb;
			if( !szRdrU8(r, &bb) ) return false;
			id = (id << 8) | bb;
			}
		if( id > 0xFFFFFFFFUL ) { setErr("coder id too wide"); return false; }
		c->id = (DWORD)id;
		if( flags & 0x10 )
			{
			if( !szNum32(r, &c->nin) || !szNum32(r, &c->nout) ) return false;
			}
		else
			{
			c->nin = 1;
			c->nout = 1;
			}
		if( c->nin == 0 || c->nin > 4 || c->nout == 0 || c->nout > 4 )
			{ setErr("bad coder arity"); return false; }
		if( flags & 0x20 )
			{
			if( !szNum32(r, &c->propsSize) ) return false;
			if( c->propsSize > sizeof(c->props) || c->propsSize > (r->end - r->p) )
				{ setErr("coder props too big"); return false; }
			memcpy( c->props, r->b + r->p, c->propsSize );
			r->p += c->propsSize;
			}
		}

	fo->tin = 0; fo->tout = 0;
	for( i = 0; i < nc; i++ )
		{
		fo->coders[i].inBase = fo->tin;
		fo->coders[i].outBase = fo->tout;
		fo->tin += fo->coders[i].nin;
		fo->tout += fo->coders[i].nout;
		}
	if( fo->tin > 16 || fo->tout > 16 ) { setErr("folder too wide"); return false; }
	if( fo->tout < 1 ) { setErr("folder with no output"); return false; }

	// NumBindPairs is always NumOutStreamsTotal - 1.
	fo->numBindPairs = fo->tout - 1;
	for( i = 0; i < fo->numBindPairs; i++ )
		{
		if( !szNum32(r, &fo->bindIn[i]) || !szNum32(r, &fo->bindOut[i]) ) return false;
		if( fo->bindIn[i] >= fo->tin || fo->bindOut[i] >= fo->tout )
			{ setErr("bind pair out of range"); return false; }
		}
	if( fo->tin < fo->numBindPairs ) { setErr("bad bind pairs"); return false; }
	fo->numPacked = fo->tin - fo->numBindPairs;
	if( fo->numPacked > 16 ) { setErr("too many packed streams"); return false; }

	// PackedIndices are IN-STREAM indices of the unbound inputs, NOT pack
	// stream ordinals - proved against four real BCJ2 archives, where the
	// values are 0,1,2,3 / 0,2,3,4 / 0,3,4,5 depending purely on how many
	// coders precede BCJ2. They are only written when there is more than one.
	if( fo->numPacked > 1 )
		{
		for( i = 0; i < fo->numPacked; i++ )
			if( !szNum32(r, &fo->packedIdx[i]) ) return false;
		}
	else
		{
		if( fo->numPacked != 1 ) { setErr("folder with no input"); return false; }
		fo->packedIdx[0] = 0;
		}

	// in-stream -> pack slot, and prove the partition is exact. Bound and
	// packed need SEPARATE marks: reusing one array wipes the bind-pair set.
	memset( used, 0, sizeof(used) );
	for( i = 0; i < fo->numBindPairs; i++ )
		{
		if( used[fo->bindIn[i]] ) { setErr("duplicate bind in"); return false; }
		used[fo->bindIn[i]] = true;
		}
	memset( packed, 0, sizeof(packed) );
	for( i = 0; i < fo->numPacked; i++ )
		{
		DWORD gi = fo->packedIdx[i];
		if( gi >= fo->tin ) { setErr("packed index range"); return false; }
		if( packed[gi] ) { setErr("duplicate packed index"); return false; }
		if( used[gi] ) { setErr("packed index also bound"); return false; }
		packed[gi] = true;
		fo->packSlot[gi] = i;
		}
	t = 0;
	for( i = 0; i < fo->tin; i++ )
		{
		if( !used[i] && !packed[i] )
			{ setErr("in-stream neither bound nor packed"); return false; }
		t++;
		}
	if( t != fo->tin || fo->numPacked != (DWORD)(fo->tin - fo->numBindPairs) )
		{ setErr("packed count"); return false; }
	return true;
}

static const char* szModeForCoder( DWORD id )
{
	switch( id )
	{
	case SZ_COPY:		return "Copy";
	case SZ_DELTA:	return "Delta";
	case SZ_LZMA2:	return "LZMA2";
	case SZ_LZMA:	return "LZMA";
	case SZ_BCJ:		return "BCJ";
	case SZ_BCJ2:	return "BCJ2";
	case SZ_PPMD7:	return "PPMd";
	case SZ_DEFLATE:	return "Deflate";
	case SZ_BZIP2:	return "BZip2";
	case SZ_ZSTD:	return "Zstd";
	case SZ_AES256:	return "7zAES";
	default:			return NULL;
	}
}

// Reporting mode must not depend on decoding, so pick it from the coder
// list: 7zAES wins, otherwise the coder feeding the folder output.
void CSevenZ::szFolderMode( const SzFolder* fo, char* out )
{
	const char* nm = NULL;
	DWORD ci, last = 0, oi;
	for( ci = 0; ci < fo->numCoders; ci++ )
		{
		if( fo->coders[ci].id == SZ_AES256 )
			{
			nm = "7zAES";
			break;
			}
		last = ci;
		}
	if( !nm )
		{
		oi = fo->tout - 1;
		for( ci = 0; ci < fo->numCoders; ci++ )
			if( oi >= fo->coders[ci].outBase &&
			    oi < fo->coders[ci].outBase + fo->coders[ci].nout )
				{
				last = ci;
				break;
				}
		nm = szModeForCoder( fo->coders[last].id );
		}
	strcpy( out, nm ? nm : "?" );
}

bool CSevenZ::readUnpackInfo( SzRdr* r )
{
	DWORD t, n, i, j;
	DWORD crcTmp[1];
	bool kTmp[1];

	if( !szNum32(r, &t) || t != SZ_kFolder ) { setErr("no kFolder"); return false; }
	if( !szNum32(r, &n) ) return false;
	if( n == 0 || n > SZ_MAX_FOLDERS ) { setErr("folder count"); return false; }
	if( !szRdrU8(r, &t) ) return false;
	if( t != 0 ) { setErr("external folders"); return false; }

	m_folders = new SzFolder[n];
	m_numFolders = n;
	memset(m_folders, 0, n * sizeof(SzFolder));
	for( i = 0; i < n; i++ )
		if( !readFolder(r, &m_folders[i]) ) return false;

	if( !szNum32(r, &t) || t != SZ_kCodersUnpackSz ) { setErr("no kCodersUnPackSize"); return false; }
	for( i = 0; i < n; i++ )
		{
		for( j = 0; j < m_folders[i].tout; j++ )
			if( !szNum32(r, &m_folders[i].outSize[j]) ) return false;
		{
		DWORD j2;
		for( j2 = 0; j2 < m_folders[i].tout; j2++ )
			{
			DWORD ci2, src2 = m_folders[i].outSize[j2];
			for( ci2 = 0; ci2 < m_folders[i].numCoders; ci2++ )
				{
				DWORD ob = m_folders[i].coders[ci2].outBase;
				if( j2 >= ob && j2 < ob + m_folders[i].coders[ci2].nout )
					{
					if( m_folders[i].coders[ci2].id == SZ_AES256 && src2 > 16 )
						src2 -= 16;   // strip the trailing SHA-256
					break;
					}
				}
			m_folders[i].outData[j2] = src2;
			m_folders[i].outCap[j2] = m_folders[i].outSize[j2];
			}
		// A 7zAES coder decrypts its whole packed input, which can exceed the
		// outSize recorded for it, so size its buffer from the pack stream.
		{
		DWORD c3;
		for( c3 = 0; c3 < m_folders[i].numCoders; c3++ )
			{
			DWORD ob = m_folders[i].coders[c3].outBase, j3;
			if( m_folders[i].coders[c3].id != SZ_AES256 ) continue;
			for( j3 = 0; j3 < m_folders[i].coders[c3].nout; j3++ )
				{
				DWORD gi = m_folders[i].coders[c3].inBase, bnd = 0, q3;
				for( q3 = 0; q3 < m_folders[i].numBindPairs; q3++ )
					if( m_folders[i].bindIn[q3] == gi ) { bnd = m_folders[i].bindOut[q3]; break; }
				if( bnd ) continue;          // fed by another coder, not a pack
				if( m_folders[i].packSlot[gi] + m_folders[i].firstPack < m_numPack )
					{
					DWORD ord = m_folders[i].packSlot[gi] + m_folders[i].firstPack;
					if( m_packSizes[ord] > m_folders[i].outCap[ob + j3] )
						m_folders[i].outCap[ob + j3] = m_packSizes[ord];
					}
				}
			}
		}
		szFolderMode( &m_folders[i], m_folders[i].szMode );
		// The folder's unpacked size is its final output stream's payload.
		m_folders[i].unpackSize = m_folders[i].outData[m_folders[i].tout - 1];
		}
		}

	if( r->p < r->end && r->b[r->p] == SZ_kCRC )
		{
		r->p++;
		for( i = 0; i < n; i++ )
			{
			if( !readDigests(r, 1, crcTmp, kTmp) ) return false;
			m_folders[i].crc = crcTmp[0];
			m_folders[i].crcKnown = kTmp[0];
			}
		}
	if( !szNum32(r, &t) || t != SZ_kEnd ) { setErr("unpack: no kEnd"); return false; }

	// Folders consume pack streams sequentially, in folder order.
	{
		DWORD acc = 0;
		for( i = 0; i < n; i++ )
			{
			m_folders[i].firstPack = acc;
			m_folders[i].numPackStreams = m_folders[i].numPacked;
			m_folders[i].packSize = 0;
			if( acc + m_folders[i].numPacked > m_numPack )
				{ setErr("folder pack range"); return false; }
			acc += m_folders[i].numPacked;
			{
			DWORD q3;
			for( q3 = 0; q3 < m_folders[i].numPacked; q3++ )
				m_folders[i].packSize += m_packSizes[m_folders[i].firstPack + q3];
			}
			}
		}
	return true;
}

bool CSevenZ::readSubStreamsInfo( SzRdr* r )
{
	DWORD t, i, j, total, si;
	DWORD* counts;
	SzSub* s;
	BYTE* bits = NULL;
	bool haveSizes, ok;

	total = 0;
	counts = new DWORD[m_numFolders];
	for( i = 0; i < m_numFolders; i++ )
		counts[i] = 1;

	if( r->p < r->end && r->b[r->p] == SZ_kNumUnpackStream )
		{
		r->p++;
		for( i = 0; i < m_numFolders; i++ )
			{
			if( !szNum32(r, &counts[i]) ) { delete [] counts; return false; }
			if( counts[i] > SZ_MAX_SUBS ) { delete [] counts; setErr("substream count"); return false; }
			}
		}
	for( i = 0; i < m_numFolders; i++ )
		total += counts[i];
	if( total > SZ_MAX_SUBS || total == 0 )
		{ delete [] counts; setErr("substream count"); return false; }

	m_subs.resize( total );
	s = &m_subs[0];
	si = 0;

	haveSizes = (r->p < r->end && r->b[r->p] == SZ_kSize);
	if( haveSizes )
		{
		r->p++;
		}
	else
		{
		for( i = 0; i < m_numFolders; i++ )
			if( counts[i] != 1 )
				{ delete [] counts; setErr("sizes required"); return false; }
		}

	for( i = 0; i < m_numFolders; i++ )
		{
		DWORD sum = 0;
		for( j = 0; j + 1 < counts[i]; j++ )
			{
			if( !szNum32(r, &s[si].size) ) { delete [] counts; return false; }
			s[si].folder = i;
			s[si].offset = sum;
			sum += s[si].size;
			s[si].crc = 0;
			s[si].crcKnown = false;
			si++;
			}
		if( sum > m_folders[i].unpackSize )
			{ delete [] counts; setErr("substream over folder"); return false; }
		// the last substream takes whatever is left of the folder
		s[si].size = m_folders[i].unpackSize - sum;
		s[si].folder = i;
		s[si].offset = sum;
		s[si].crc = 0;
		s[si].crcKnown = false;
		si++;
		}

	// A folder with a single substream inherits the folder CRC. Every OTHER
	// slot is "unknown" and must be filled from the kCRC block - and the bit
	// vector in there covers only the UNKNOWN slots, not all substreams.
	{
	DWORD need = 0, all, di;

	for( i = 0; i < m_numFolders; i++ )
		{
		if( counts[i] == 1 && m_folders[i].crcKnown ) continue;
		need += counts[i];
		}
	si = 0;
	for( i = 0; i < m_numFolders; i++ )
		for( j = 0; j < counts[i]; j++, si++ )
			if( counts[i] == 1 && m_folders[i].crcKnown )
				{
				s[si].crc = m_folders[i].crc;
				s[si].crcKnown = true;
				}

	if( need > 0 )
		{
		if( r->p >= r->end || r->b[r->p] != SZ_kCRC )
			{ delete [] counts; setErr("substream crcs missing"); return false; }
		r->p++;
		if( !szRdrU8(r, &all) ) { delete [] counts; return false; }
		if( all )
			{
			bits = new BYTE[need ? need : 1];
			for( i = 0; i < need; i++ ) bits[i] = 1;
			}
		else
			{
			bits = new BYTE[need ? need : 1];
			if( !szRdrBits(r, need, bits) )
				{ delete [] bits; delete [] counts; return false; }
			}
		di = 0;
		si = 0;
		for( i = 0; i < m_numFolders; i++ )
			for( j = 0; j < counts[i]; j++, si++, di++ )
				{
				if( counts[i] == 1 && m_folders[i].crcKnown ) continue;
				if( di < need && !bits[di] ) continue;
				if( !szRdrU32(r, &s[si].crc) )
					{ delete [] bits; delete [] counts; return false; }
				s[si].crcKnown = true;
				}
		delete [] bits;
		bits = NULL;
		}
	}

	ok = ( szNum32(r, &t) && t == SZ_kEnd );
	delete [] counts;
	if( !ok ) { setErr("substreams: no kEnd"); return false; }
	return true;
}

// -------------------------------------------------------------------------
// FilesInfo
// -------------------------------------------------------------------------

// UTF-16LE (with surrogate pairs) -> UTF-8 -> ACP, then sanitise into a
// relative path so a hostile archive cannot escape the output directory.
static bool szNameFromUtf16( const BYTE* b, DWORD avail, char* out, DWORD outMax )
{
	char utf8[FNAME_MAX32 * 4 + 8];
	DWORD wp = 0, up = 0;
	DWORD last = 0;

	while( wp + 1 < avail )
		{
		unsigned c = (unsigned)b[wp] | ((unsigned)b[wp+1] << 8);
		wp += 2;
		if( c == 0 ) break;
		last = up;
		if( c >= 0xD800 && c < 0xDC00 && wp + 1 < avail )
			{
			unsigned lo = (unsigned)b[wp] | ((unsigned)b[wp+1] << 8);
			if( lo >= 0xDC00 && lo < 0xE000 )
				{
				wp += 2;
				c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
				}
			}
		if( c < 0x80 )
			{
			if( up < sizeof(utf8) - 1 ) utf8[up++] = (char)c;
			}
		else if( c < 0x800 )
			{
			if( up + 1 < sizeof(utf8) ) { utf8[up++] = (char)(0xC0 | (c >> 6)); utf8[up++] = (char)(0x80 | (c & 0x3F)); }
			}
		else if( c < 0x10000 )
			{
			if( up + 2 < sizeof(utf8) )
				{
				utf8[up++] = (char)(0xE0 | (c >> 12));
				utf8[up++] = (char)(0x80 | ((c >> 6) & 0x3F));
				utf8[up++] = (char)(0x80 | (c & 0x3F));
				}
			}
		else
			{
			if( up + 3 < sizeof(utf8) )
				{
				utf8[up++] = (char)(0xF0 | (c >> 18));
				utf8[up++] = (char)(0x80 | ((c >> 12) & 0x3F));
				utf8[up++] = (char)(0x80 | ((c >> 6) & 0x3F));
				utf8[up++] = (char)(0x80 | (c & 0x3F));
				}
			}
		}
	utf8[up] = 0;
	if( up == 0 ) return false;
	if( !kiutil::utf8toAnsi(out, (int)outMax, utf8) )
		lstrcpyn( out, utf8, outMax );

	{
		// normalise separators, drop drive/leading slashes and any ".."
		char clean[FNAME_MAX32 + 1];
		DWORD o = 0, s;
		char* seps = out;
		for( s = 0; seps[s] && o < FNAME_MAX32; s++ )
			{
			if( seps[s] == '/' || seps[s] == '\\' )
				{
				if( o == 0 || clean[o-1] == '\\' ) continue;	// collapse / drop leading
				clean[o++] = '\\';
				continue;
				}
			clean[o++] = seps[s];
			}
		while( o > 0 && clean[o-1] == '\\' ) o--;
		clean[o] = 0;
		if( o == 0 ) return false;
		// strip a leading "C:" style drive specifier
		if( clean[1] == ':' ) memmove( clean, clean + 2, o - 1 );
		// reject/scrub parent traversal
		{
		DWORD w = 0;
		s = 0;
		while( s < o )
			{
			DWORD e = s;
			while( e < o && clean[e] != '\\' ) e++;
			if( !(e - s == 2 && clean[s] == '.' && clean[s+1] == '.') )
				{
				if( w ) clean[w++] = '\\';
				memmove( clean + w, clean + s, e - s );
				w += e - s;
				}
			s = e + 1;
			}
		clean[w] = 0;
		if( w == 0 ) return false;
		lstrcpyn( out, clean, outMax );
		}
	(void)last;
	}
	return true;
}

bool CSevenZ::readFilesInfo( SzRdr* r )
{
	DWORD nfiles, t, psize, i, nstream = 0, nempty = 0;
	DWORD next = 0, lastFolder = (DWORD)-1;
	const BYTE* names = NULL;
	DWORD namesLen = 0, namesPos = 0;
	BYTE* isEmpty = NULL;
	BYTE* isDir = NULL;
	bool haveMTime = false, haveAttrib = false;

	if( !szNum32(r, &nfiles) ) { setErr("files: no count"); return false; }
	if( nfiles == 0 || nfiles > SZ_MAX_FILES ) { setErr("file count"); return false; }

	m_entries.resize( nfiles );
	for( i = 0; i < nfiles; i++ )
		{
		CSevenZEntry& e = m_entries[i];
		memset( &e, 0, sizeof(e) );
		e.nFolder = (DWORD)-1;
		e.szMode = "";
		e.szName[0] = 0;
		}
	// always sized: kEmptyStream is optional but we index it either way
	isEmpty = new BYTE[nfiles];
	memset( isEmpty, 0, nfiles );

	for(;;)
		{
		const BYTE* pb;
		SzRdr pr;

		if( !szNum32(r, &t) ) { setErr("files: prop id"); return false; }
		if( t == SZ_kEnd ) break;
		if( !szNum32(r, &psize) || psize > r->end - r->p ) { setErr("bad property"); return false; }

		pb = r->b + r->p;
		r->p += psize;
		pr.b = pb; pr.p = 0; pr.end = psize; pr.err = false;

		switch( t )
			{
			case SZ_kEmptyStream:
				if( !szRdrBits(&pr, nfiles, isEmpty) ) { setErr("files: emptyStream"); return false; }
				break;

			case SZ_kEmptyFile:
				{
				DWORD k;
				for( i = 0; i < nfiles; i++ ) if( isEmpty[i] ) nempty++;
				delete [] isDir;
				isDir = new BYTE[nempty ? nempty : 1];
				memset( isDir, 0, nempty ? nempty : 1 );
				// a BIT vector over the empty streams, not one byte each
				if( nempty && !szRdrBits(&pr, nempty, isDir) )
				{ setErr("files: emptyFile"); return false; }
				// bit set => empty file, clear => directory
				for( k = 0; k < nempty; k++ )
					isDir[k] = (BYTE)(isDir[k] == 0);
				break;
				}

			case SZ_kName:
				{
				DWORD ext;
				if( !szRdrU8(&pr, &ext) ) { setErr("files: name ext"); return false; }
				if( ext != 0 ) { setErr("external names"); return false; }
				names = pb + pr.p;
				namesLen = psize - pr.p;
				namesPos = 0;
				break;
				}

			case SZ_kWinAttrib:
				{
				DWORD all, ext, k;
				BYTE* def;
				if( !szRdrU8(&pr, &all) || !szRdrU8(&pr, &ext) ) { setErr("files: attrib hdr"); return false; }
				if( ext != 0 ) { setErr("external attribs"); return false; }
				def = new BYTE[nfiles];
				if( all ) memset( def, 1, nfiles );
				else if( !szRdrBits(&pr, nfiles, def) ) { delete [] def; setErr("files: attrib bits"); return false; }
				for( i = 0; i < nfiles; i++ )
					{
					DWORD a = 0;
					if( def[i] && !szRdrU32(&pr, &a) ) { delete [] def; setErr("files: attrib value"); return false; }
					m_entries[i].dwAttrib = a;
					}
				delete [] def;
				haveAttrib = true;
				break;
				}

			case SZ_kMTime:
			case SZ_kCTime:
			case SZ_kATime:
				{
				DWORD all, ext, k;
				BYTE* def;
				if( !szRdrU8(&pr, &all) || !szRdrU8(&pr, &ext) ) { setErr("files: attrib hdr"); return false; }
				if( ext != 0 ) { setErr("external times"); return false; }
				def = new BYTE[nfiles];
				if( all ) memset( def, 1, nfiles );
				else if( !szRdrBits(&pr, nfiles, def) ) { delete [] def; setErr("files: attrib bits"); return false; }
				for( i = 0; i < nfiles; i++ )
					{
					unsigned __int64 v = 0;
					if( !def[i] ) continue;
					if( !szRdrU64(&pr, &v) ) { delete [] def; setErr("files: time value"); return false; }
					if( t == SZ_kMTime )
						{
						m_entries[i].ftTime = v;
						m_entries[i].bHasTime = true;
						haveMTime = true;
						}
					}
				delete [] def;
				break;
				}

			default:
				// kStartPos / kDummy / kComment / kAnti / anything newer
				break;
			}
		}

	// names are required: a nameless 7z is not something we can extract
	if( !names ) { delete [] isEmpty; delete [] isDir; setErr("no names"); return false; }
	for( i = 0; i < nfiles; i++ )
		{
		CSevenZEntry& e = m_entries[i];
		if( namesPos + 1 >= namesLen ||
		    !szNameFromUtf16( names + namesPos, namesLen - namesPos,
		                      e.szName, FNAME_MAX32 + 1 ) )
			{ delete [] isEmpty; delete [] isDir; setErr("bad name"); return false; }
		{
		DWORD q = namesPos;
		while( q + 1 < namesLen )
			{
			if( names[q] == 0 && names[q+1] == 0 ) { q += 2; break; }
			q += 2;
			}
		namesPos = q;
		}
		}

	// directories and empty files
	nstream = 0;
	for( i = 0; i < nfiles; i++ )
		{
		CSevenZEntry& e = m_entries[i];
		if( !isEmpty[i] ) continue;
		e.bDir = ( nstream < nempty ) ? (isDir[nstream] != 0) : true;
		nstream++;
		e.nFolder = (DWORD)-1;
		e.dwSize = 0;
		e.dwCRC = 0;
		e.bCRCKnown = false;
		e.szMode = "";
		}

	// attach substreams, in order, to the files that have a data stream
	(void)haveMTime;
	(void)haveAttrib;
	next = 0;
	for( i = 0; i < nfiles; i++ )
		{
		CSevenZEntry& e = m_entries[i];
		if( isEmpty[i] ) continue;
		if( next >= (DWORD)m_subs.size() )
			{ delete [] isEmpty; delete [] isDir; setErr("more files than substreams"); return false; }
		e.nFolder = m_subs[next].folder;
		e.dwOffset = m_subs[next].offset;
		e.dwSize = m_subs[next].size;
		e.dwCRC = m_subs[next].crc;
		e.bCRCKnown = m_subs[next].crcKnown;
		// 7-Zip reports the whole folder packed size on the folder FIRST
		// member only; the rest of the folder shows 0.
		e.dwPackSize = ( e.nFolder != lastFolder ) ? m_folders[e.nFolder].packSize : 0;
		lastFolder = e.nFolder;
		e.szMode = m_folders[e.nFolder].szMode;
		next++;
		}

	delete [] isEmpty;
	delete [] isDir;
	return true;
}

// =========================================================================
// pack stream access + folder decoding
// =========================================================================

bool CSevenZ::readPackStream( DWORD ordinal, BYTE** buf, DWORD* len )
{
	DWORD off = 0, i;

	*buf = NULL;
	*len = 0;
	if( ordinal >= m_numPack ) { setErr("pack ordinal"); return false; }
	for( i = 0; i < ordinal; i++ )
		off += m_packSizes[i];
	if( (unsigned __int64)m_packPos + off + m_packSizes[ordinal] > m_fileSize )
		{ setErr("pack stream past eof"); return false; }

	*len = m_packSizes[ordinal];
	*buf = new BYTE[*len ? *len : 1];
	if( *len )
		{
		fseek( m_fp, (long)(m_packPos + off), SEEK_SET );
	if( *len != fread(*buf, 1, *len, m_fp) )
			{ delete [] *buf; *buf = NULL; setErr("pack short"); return false; }
		}
	return true;
}


bool CSevenZ::decodeOut( DWORD fi, DWORD oi, BYTE* dst, DWORD dstLen,
                         DWORD* produced )
{
	SzFolder* fo;
	SzCoder* c;
	BYTE* src[4];
	DWORD srcLen[4];
	DWORD ci, i, k, ok = 0;
	*produced = 0;

	if( fi >= m_numFolders ) { setErr("folder range"); return false; }
	fo = &m_folders[fi];
	if( oi >= fo->tout ) { setErr("out stream range"); return false; }
	if( dstLen < fo->outCap[oi] ) { setErr("out buffer too small"); return false; }

	for( ci = 0; ci < fo->numCoders; ci++ )
		if( oi >= fo->coders[ci].outBase && oi < fo->coders[ci].outBase + fo->coders[ci].nout )
			break;
	if( ci == fo->numCoders ) { setErr("no coder for stream"); return false; }
	c = &fo->coders[ci];

	for( k = 0; k < 4; k++ ) { src[k] = NULL; srcLen[k] = 0; }

	for( k = 0; k < c->nin; k++ )
		{
		DWORD gi = c->inBase + k;
		DWORD bound = (DWORD)-1;
		for( i = 0; i < fo->numBindPairs; i++ )
			if( fo->bindIn[i] == gi ) { bound = fo->bindOut[i]; break; }
		if( bound != (DWORD)-1 )
			{
			src[k] = new BYTE[fo->outCap[bound] ? fo->outCap[bound] : 1];
			if( !decodeOut( fi, bound, src[k], fo->outCap[bound], &srcLen[k] ) )
				{ ok = 1; break; }
			}
		else
			{
			if( !readPackStream( fo->firstPack + fo->packSlot[gi], &src[k], &srcLen[k] ) )
				{ ok = 1; break; }
			}
		}

	if( !ok )
		{
		DWORD made = 0;
		DWORD want = fo->outSize[oi];
		if( c->id == SZ_AES256 && srcLen[0] > 0 )
			want = srcLen[0];      // 7zAES decrypts its whole packed input
		if( want > dstLen )
			{
			setErr("out buffer too small");
			ok = 1;
			}
		else if( runCoder( fo, c, src, srcLen, dst, want, &made ) )
			{
			*produced = made;
			ok = 0;
			}
		else
			ok = 1;
		}

	for( k = 0; k < 4; k++ )
		if( src[k] ) delete [] src[k];
	return ok == 0;
}

bool CSevenZ::decodeFolder( DWORD fi )
{
	SzFolder* fo;
	BYTE* buf;
	DWORD usz;

	if( m_cacheFolder == fi && m_cache ) return true;
	if( fi >= m_numFolders ) { setErr("folder range"); return false; }
	fo = &m_folders[fi];
	usz = fo->unpackSize;
	if( usz > SZ_MAX_FOLDER_OUT ) { setErr("folder too big"); return false; }

	buf = new BYTE[fo->outCap[fo->tout - 1] ? fo->outCap[fo->tout - 1] : 1];
	{
		DWORD made = 0;
		if( !decodeOut( fi, fo->tout - 1, buf, fo->outCap[fo->tout - 1], &made ) )
			{ delete [] buf; return false; }
		if( made != usz ) { delete [] buf; setErr("folder size"); return false; }
	}

	if( m_cache ) delete [] m_cache;
	m_cache = buf;
	m_cacheLen = usz;
	m_cacheFolder = fi;
	return true;
}

// -------------------------------------------------------------------------
// coder runners
// -------------------------------------------------------------------------

bool CSevenZ::runCoder( SzFolder* fo, const SzCoder* c,
                        BYTE** src, DWORD* srcLen, BYTE* dst, DWORD dstLen,
                        DWORD* produced )
{
	*produced = 0;


	switch( c->id )
	{
	case SZ_COPY:
		if( srcLen[0] < dstLen ) { setErr("copy short"); return false; }
		if( dstLen ) memcpy( dst, src[0], dstLen );
		{ *produced = dstLen; return true; }

	case SZ_LZMA:
		{
		CLzmaDec st;
		CLzmaProps pp;
		ELzmaStatus stat;
		DWORD done = 0;
		if( c->propsSize != 5 ) { setErr("lzma props"); return false; }
		if( LzmaProps_Decode(&pp, c->props, 5) != SZ_OK ) { setErr("lzma props"); return false; }
		if( pp.dicSize < 0x1000 ) pp.dicSize = 0x1000;
		if( pp.dicSize > 0x4000000 ) { setErr("lzma dict too big"); return false; }
		LzmaDec_Construct(&st);
		if( LzmaDec_Allocate(&st, c->props, 5, &g_szAlloc) != SZ_OK )
			{ setErr("lzma alloc"); return false; }
		LzmaDec_Init(&st);
		stat = LZMA_STATUS_NOT_SPECIFIED;
		while( done < dstLen )
			{
			SizeT dl = dstLen - done;
			SizeT sl = srcLen[0];
			SizeT prev = sl;
			SRes r = LzmaDec_DecodeToBuf(&st, dst + done, &dl, src[0], &sl,
			                              LZMA_FINISH_ANY, &stat);
			if( r != SZ_OK ) { LzmaDec_Free(&st, &g_szAlloc); setErr("lzma data"); return false; }
			done += (DWORD)dl;
			if( sl == prev && dl == 0 )
				{ LzmaDec_Free(&st, &g_szAlloc); setErr("lzma stalled"); return false; }
			}
		LzmaDec_Free(&st, &g_szAlloc);
		{ *produced = dstLen; return true; }
		}

	case SZ_LZMA2:
		{
		DWORD prop;
		if( c->propsSize != 1 ) { setErr("lzma2 props"); return false; }
		prop = c->props[0];
		if( prop > SZ_MAX_LZMA2_PROP ) { setErr("lzma2 dict too big"); return false; }
		if( dstLen == 0 ) return true;
		{
		SizeT dlen = dstLen, slen = srcLen[0];
		ELzmaStatus stat = LZMA_STATUS_NOT_SPECIFIED;
		// One shot: 7z streams carry no end marker, so FINISH_ANY plus an
		// exact output length is the only reliable termination test.
		if( Lzma2Decode(dst, &dlen, src[0], &slen, (Byte)prop,
		                LZMA_FINISH_ANY, &stat, &g_szAlloc) != SZ_OK )
			{ setErr("lzma2 data"); return false; }
		if( (DWORD)dlen != dstLen ) { setErr("lzma2 short"); return false; }
		}
		{ *produced = dstLen; return true; }
		}

	case SZ_DELTA:
		{
		unsigned d;
		if( c->propsSize != 1 ) { setErr("delta props"); return false; }
		if( srcLen[0] < dstLen ) { setErr("delta input short"); return false; }
		d = (unsigned)c->props[0] + 1;
		if( dstLen )
			{
			memcpy( dst, src[0], dstLen );     // in-place converter: stage input
			SevenZDeltaDecode( d, dst, dstLen );
			}
		{ *produced = dstLen; return true; }
		}

	case SZ_PPMD7:
		if( !SevenZPpmd7Decode( c->props, c->propsSize, src[0], srcLen[0], dst, dstLen ) )
			return false;
		{ *produced = dstLen; return true; }

	case SZ_BCJ2:
		if( c->nin != 4 || c->nout != 1 ) { setErr("bcj2 arity"); return false; }
		{
		int rc = SevenZBcj2Decode( src[0], srcLen[0], src[1], srcLen[1],
			                            src[2], srcLen[2], src[3], srcLen[3], dst, dstLen );
		if( rc != 0 )
			{
			char m[64];
			wsprintf( m, "bcj2 rc=%d", rc );
			setErr( m );
			return false;
			}
		}
		{ *produced = dstLen; return true; }

	case SZ_DEFLATE:
		{
		z_stream_s zs;
		int r = Z_OK;
		DWORD prevOut;
		if( dstLen == 0 ) return true;
		memset( &zs, 0, sizeof(zs) );
		if( inflateInit2(&zs, -15) != Z_OK ) { setErr("inflate init"); return false; }
		zs.next_in = (Bytef*)src[0];
		zs.avail_in = srcLen[0];
		zs.next_out = (Bytef*)dst;
		zs.avail_out = dstLen;
		while( zs.total_out < dstLen )
			{
			prevOut = zs.total_out;
			r = inflate(&zs, Z_NO_FLUSH);
			if( r != Z_OK && r != Z_STREAM_END ) break;
			if( r == Z_STREAM_END ) break;
			if( zs.total_out == prevOut && zs.avail_in == 0 ) break;
			}
		inflateEnd(&zs);
		if( zs.total_out != dstLen ) { setErr("deflate short"); return false; }
		{ *produced = dstLen; return true; }
		}

	case SZ_BZIP2:
		{
		bz_stream bs;
		int r;
		if( dstLen == 0 ) return true;
		memset( &bs, 0, sizeof(bs) );
		if( BZ2_bzDecompressInit(&bs, 0, 0) != BZ_OK ) { setErr("bz2 init"); return false; }
		bs.next_in = (char*)src[0];
		bs.avail_in = srcLen[0];
		bs.next_out = (char*)dst;
		bs.avail_out = dstLen;
		r = BZ2_bzDecompress(&bs);
		BZ2_bzDecompressEnd(&bs);
		if( r != BZ_STREAM_END || bs.total_out_lo32 != dstLen )
			{ setErr("bz2 short"); return false; }
		{ *produced = dstLen; return true; }
		}

	case SZ_ZSTD:
		{
		size_t got;
		// 7z ZSTD carries 5 (historically 3) property bytes: the zstd
		// major/minor that wrote the archive plus the encoder level. There is
		// no dictionary or window size in there - those live in the zstd frame
		// header - so the payload is one plain zstd frame, exactly like the
		// ZIP method 93 payload. A newer writer version must not be refused:
		// the frame format is forward compatible, so only the length is
		// checked here (entry CRCs cover the bytes themselves).
		if( c->propsSize != 3 && c->propsSize != 5 )
			{ setErr("zstd props"); return false; }
		if( dstLen == 0 ) return true;
		got = ZSTD_decompress( dst, dstLen, src[0], srcLen[0] );
		if( ZSTD_isError(got) || got != (size_t)dstLen )
			{ setErr("zstd data"); return false; }
		{ *produced = dstLen; return true; }
		}

	case SZ_AES256:
		{
		BYTE key[32], iv[16];
		const BYTE* p = c->props;
		DWORD ps = c->propsSize;
		DWORD b0, b1 = 0, saltSize = 0, ivSize = 0, cycles;

		if( ps < 1 ) { setErr("aes props"); return false; }
		b0 = p[0];
		cycles = b0 & 0x3F;
		if( (b0 & 0xC0) == 0 )
			{
			if( ps != 1 ) { setErr("aes props"); return false; }
			memset( iv, 0, 16 );
			}
		else
			{
			if( ps < 2 ) { setErr("aes props"); return false; }
			b1 = p[1];
			saltSize = ((b0 >> 7) & 1) + (b1 >> 4);
			ivSize   = ((b0 >> 6) & 1) + (b1 & 0x0F);
			if( saltSize > 16 || ivSize > 16 ) { setErr("aes props"); return false; }
			if( ps != 2 + saltSize + ivSize ) { setErr("aes props size"); return false; }
			memset( iv, 0, 16 );
			memcpy( iv, p + 2 + saltSize, ivSize );
			}
		if( !m_password ) { setErr("encrypted, no password"); return false; }
		{
		// 7zAES hashes the password as UTF-16LE, so convert from the ACP
		// exactly the way 7-Zip does for its own ANSI input. Hashing the
		// raw bytes here produces a key that decrypts nothing.
		BYTE pw[512];
		wchar_t wbuf[256];
		DWORD pwLen = 0, i;
		int wc = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, m_password, -1,
		                               wbuf, 256 );
		if( wc <= 0 || wc >= 256 ) wc = 1;
		for( i = 0; i < (DWORD)wc - 1 && pwLen < sizeof(pw) - 2; i++ )
			{
			pw[pwLen++] = (BYTE)wbuf[i];
			pw[pwLen++] = (BYTE)((wbuf[i] >> 8) & 0xFF);
			}
		if( !SevenZAesDeriveKey( pw, pwLen, cycles, p + 2, saltSize, key ) )
			{ ZeroMemory( pw, sizeof(pw) ); setErr("aes key"); return false; }
		ZeroMemory( pw, sizeof(pw) );
		ZeroMemory( wbuf, sizeof(wbuf) );
		}
		// 7zAES encrypts payload+16-byte SHA-256 and 7-Zip records the coder's
		// outSize INCLUDING that digest, so the encrypted input is exactly
		// dstLen and the plaintext handed downstream is dstLen-16.
		if( srcLen[0] != dstLen )
			{
			char m[64];
			wsprintf( m, "aes in=%lu out=%lu", srcLen[0], dstLen );
			setErr( m );
			return false;
			}
		if( dstLen < 16 || (dstLen & 15) != 0 ) { setErr("aes payload align"); return false; }
		// A 7zAES payload carries NO embedded digest. The encoder rounds
		// the plaintext up to a 16-byte boundary with ZEROS (the p7zip
		// "AES filters need 16-bytes alignment" rule), so the tail of the
		// ciphertext is padding, not a SHA-256 of anything, and the coder
		// outSize recorded in the header is that padded size. The consumer
		// downstream knows its own real output length and simply ignores the
		// padding. Integrity comes from the entry CRCs (Extract deletes the
		// output on mismatch), the same trust model py7zr and our ZIP
		// WinZip-AES path use.
		// SevenZAesDecryptCbc works in place, and dst is a freshly
		// allocated (uninitialised) output buffer, so stage the ciphertext.
		memcpy( dst, src[0], dstLen );
		SevenZAesDecryptCbc( key, iv, dst, dstLen );
		{ *produced = dstLen; return true; }
		}

	default:
		break;
	}

	// Branch converters (x86, ARM, ARMT, PPC, SPARC, IA64, ARM64, RISCV) are
	// in-place, so stage the packed input into dst first.
	if( srcLen[0] < dstLen ) { setErr("branch input short"); return false; }
	if( dstLen )
		{
		memcpy( dst, src[0], dstLen );
		if( SevenZBranchDecode( c->id, dst, dstLen ) )
		{ *produced = dstLen; return true; }
		}
	setErr( "unsupported coder" );
	return false;
}

// =========================================================================
// encoded header (a .7z whose own header is a one-folder archive)
// =========================================================================

bool CSevenZ::decodeEncodedHeader( const BYTE* buf, DWORD len, BYTE** out, DWORD* outLen )
{
	SzRdr r;
	DWORD t;
	bool ok;
	BYTE* bufOut;
	DWORD usz, declen = 0;

	*out = NULL; *outLen = 0;

	r.b = buf; r.p = 0; r.end = len; r.err = false;
	if( !szNum32(&r, &t) ) return false;
	if( t == SZ_kPackInfo )
		{
		if( !readPackInfo(&r) ) return false;
		if( !szNum32(&r, &t) ) return false;
		}
	if( t != SZ_kUnpackInfo ) { setErr("encoded header: no streams"); return false; }
	if( !readUnpackInfo(&r) ) return false;
	if( m_numFolders != 1 ) { setErr("encoded header folders"); return false; }

	usz = m_folders[0].unpackSize;
	if( usz == 0 || usz > SZ_MAX_HEADER_SIZE ) { setErr("encoded header size"); return false; }

	bufOut = new BYTE[m_folders[0].outCap[m_folders[0].tout - 1]];
	ok = decodeOut( 0, m_folders[0].tout - 1, bufOut,
	                  m_folders[0].outCap[m_folders[0].tout - 1], &declen );
	if( !ok ) { delete [] bufOut; return false; }

	*out = bufOut;
	*outLen = usz;
	return true;
}

// =========================================================================
// extraction
// =========================================================================

bool CSevenZ::Extract( DWORD i, const char* path )
{
	const CSevenZEntry* e = Get(i);
	FILE* f;

	if( !e ) return false;

	if( e->nFolder == (DWORD)-1 )
		{
		// no data stream: an empty file (dirs are created by the caller)
		f = fopen( path, "wb" );
		if( !f ) return false;
		fclose( f );
		return true;
		}

	if( !decodeFolder( e->nFolder ) ) return false;
	if( (unsigned __int64)e->dwOffset + e->dwSize > m_cacheLen )
		{ setErr("entry past folder"); return false; }

	f = fopen( path, "wb" );
	if( !f ) return false;
	if( e->dwSize )
		fwrite( m_cache + e->dwOffset, 1, e->dwSize, f );
	fclose( f );

	if( e->bCRCKnown && szCrc32( m_cache + e->dwOffset, e->dwSize ) != e->dwCRC )
		{
		DeleteFileA( path );
		setErr( "crc mismatch" );
		return false;
		}
	return true;
}