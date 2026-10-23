#include "stdafx.h"
#include "EggDecode.h"
#include "EggAzo.h"
#include "LzmaDecode.h"
#include "zlib/zlib.h"
#include "libbzip2/bzlib.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//-- EGG \x95\x9c\x8bC -----------------------
//
// Shared container + block decoder.  See EggDecode.h for the layout.  The
// volume and sub-header walks mirror alzgg/src/egg.c exactly; every block
// is verified against its own CRC-32 before (compressed) or while (store)
// its bytes reach the sink.
//
// A member without sub-headers and without blocks is an empty file.  An
// archive-level Comment may sit between members.  Encrypted members
// (ZipCrypto, AES128/256, LEA128/256) are refused up front: the key
// material lives in alzgg/src/egg_crypt.c and is not ported yet.

// CRC-32 (the Info-ZIP table, same as zip deflate)
struct EggCrcT
{
	DWORD t[256];
	int   ready;
};

static EggCrcT g_egg_crc;

static void EggCrcInit( DWORD* t )
{
	DWORD c, n, k;

	for( n = 0; n < 256; n++ )
	{
		c = n;
		for( k = 8; k; k-- )
			c = c & 1 ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
		t[n] = c;
	}
}

static DWORD EggCrc32( DWORD crc, const BYTE* p, DWORD n )
{
	// the standard reflected CRC-32 (the one zlib computes): the state is
	// inverted before and after, so a running value can be chained
	if( !g_egg_crc.ready )
	{
		EggCrcInit( g_egg_crc.t );
		g_egg_crc.ready = 1;
	}
	crc = ~crc;
	while( n-- )
		crc = g_egg_crc.t[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	return ~crc;
}

static DWORD EggLe16( const BYTE* p )
{
	return (DWORD)p[0] | ((DWORD)p[1] << 8);
}

static DWORD EggLe32( const BYTE* p )
{
	return EggLe16( p ) | (EggLe16( p + 2 ) << 16);
}

static unsigned __int64 EggLe64( const BYTE* p )
{
	return (unsigned __int64)EggLe32( p )
	     | ((unsigned __int64)EggLe32( p + 4 ) << 32);
}

//-- the archive -----------------------

EggArchive::EggArchive()
{
	m_buf = NULL;
	m_len = 0;
	m_solid = 0;
	m_err[0] = '\0';
	m_vol = NULL;
	m_nvol = 0;
	m_mem = NULL;
	m_count = 0;
	m_cap = 0;
	m_blk = NULL;
	m_nblk = 0;
	m_blkcap = 0;
	m_password[0] = '\0';
	m_pwBad = false;
}

EggArchive::~EggArchive()
{
	Close();
}

void EggArchive::Close()
{
	if( m_mem )
		free( m_mem );
	m_mem = NULL;
	if( m_blk )
		free( m_blk );
	m_blk = NULL;
	if( m_vol )
		free( m_vol );
	m_vol = NULL;
	if( m_buf )
		free( m_buf );
	m_buf = NULL;
	m_len = 0;
	m_solid = 0;
	m_count = 0;
	m_cap = 0;
	m_nblk = 0;
	m_blkcap = 0;
	m_nvol = 0;
	m_pwBad = false;
}

static void EggSetErr( char* err, const char* fmt, ... )
{
	va_list ap;

	if( !err )
		return;
	va_start( ap, fmt );
	_vsnprintf( err, 256, fmt, ap );
	err[255] = '\0';
	va_end( ap );
}

bool EggArchive::Member( int i, EggMember* m ) const
{
	if( i < 0 || i >= m_count || !m_mem )
		return false;
	*m = m_mem[i];
	return true;
}

BYTE EggArchive::MemberMethod( int i ) const
{
	if( i < 0 || i >= m_count || !m_mem || m_mem[i].nblocks < 1 )
		return 0xFF;
	return m_blk[m_mem[i].firstblock].method;
}

unsigned __int64 EggArchive::MemberCsize( int i ) const
{
	unsigned __int64 total = 0;
	int j;

	if( i < 0 || i >= m_count || !m_mem )
		return 0;
	for( j = 0; j < m_mem[i].nblocks; j++ )
		total += m_blk[m_mem[i].firstblock + j].csize;
	return total;
}

// Reads one volume's header and prefix.  *payloadOut receives the offset
// of the volume data after the prefix.
bool EggArchive::ParseVolume( const char* path, DWORD* idOut, DWORD* prevOut,
                              DWORD* nextOut, DWORD* payloadOut, int* solidOut,
                              char* volpath ) const
{
	FILE* f;
	BYTE  b[16];
	DWORD sz;
	BYTE  flags;
	BYTE  sub[8];

	f = fopen( path, "rb" );
	if( !f )
		return false;
	if( fread( b, 1, EGG_HDRSZ, f ) != EGG_HDRSZ ||
	    EggLe32( b ) != EGG_SIG_ARCHIVE )
	{
		fclose( f );
		return false;
	}
	*idOut = EggLe32( b + 6 );
	*prevOut = 0;
	*nextOut = 0;
	*solidOut = 0;
	for(;;)
	{
		if( fread( b, 1, 4, f ) != 4 )
		{
			fclose( f );
			return false;
		}
		if( EggLe32( b ) == EGG_SIG_END )
			break;
		if( EggLe32( b ) == EGG_SIG_FILE )
		{
			// an empty prefix: the first member header follows
			if( fseek( f, -4, SEEK_CUR ) != 0 )
			{
				fclose( f );
				return false;
			}
			break;
		}
		if( fread( &flags, 1, 1, f ) != 1 )
		{
			fclose( f );
			return false;
		}
		if( flags & 1 )
		{
			if( fread( sub, 1, 4, f ) != 4 )
			{
				fclose( f );
				return false;
			}
			sz = EggLe32( sub );
		}
		else
		{
			if( fread( sub, 1, 2, f ) != 2 )
			{
				fclose( f );
				return false;
			}
			sz = EggLe16( sub );
		}
		if( EggLe32( b ) == EGG_SIG_SPLIT )
		{
			if( sz != 8 || fread( sub, 1, 8, f ) != 8 )
			{
				fclose( f );
				return false;
			}
			*prevOut = EggLe32( sub );
			*nextOut = EggLe32( sub + 4 );
		}
		else if( EggLe32( b ) == EGG_SIG_SOLID )
		{
			*solidOut = 1;
			if( fseek( f, (long)sz, SEEK_CUR ) != 0 )
			{
				fclose( f );
				return false;
			}
		}
		else if( EggLe32( b ) == EGG_SIG_DUMMY ||
		         EggLe32( b ) == EGG_SIG_SKIP ||
		         EggLe32( b ) == EGG_SIG_GENCRYPT )
		{
			if( fseek( f, (long)sz, SEEK_CUR ) != 0 )
			{
				fclose( f );
				return false;
			}
		}
		else
		{
			fclose( f );
			return false;
		}
	}
	*payloadOut = (DWORD)ftell( f );
	if( volpath )
		lstrcpy( volpath, path );
	fclose( f );
	return true;
}

// Slurps one volume's whole file into memory.
static BYTE* EggSlurp( const char* path, DWORD* outLen )
{
	FILE* f;
	long  n;
	BYTE* buf;

	f = fopen( path, "rb" );
	if( !f )
		return NULL;
	if( fseek( f, 0, SEEK_END ) != 0 || (n = ftell( f )) < 0 ||
	    fseek( f, 0, SEEK_SET ) != 0 )
	{
		fclose( f );
		return NULL;
	}
	buf = (BYTE*)malloc( (size_t)n + 1 );
	if( !buf )
	{
		fclose( f );
		return NULL;
	}
	if( n && fread( buf, 1, (size_t)n, f ) != (size_t)n )
	{
		free( buf );
		fclose( f );
		return NULL;
	}
	fclose( f );
	*outLen = (DWORD)n;
	return buf;
}

// The leading characters two file names share, ignoring case.  When two
// unrelated archives in the same directory carry the same header id (the
// ALZip sample corpus has a plain and a ZipCrypto spool of the same tree),
// the chain is resolved toward the volume whose name matches the requested
// one, instead of whichever the directory listing happens to return first.
static int EggNameScore( const char* a, const char* b )
{
	int n = 0;

	while( a[n] && b[n] )
	{
		char ca = a[n], cb = b[n];

		if( ca >= 'A' && ca <= 'Z' ) ca += 'a' - 'A';
		if( cb >= 'A' && cb <= 'Z' ) cb += 'a' - 'A';
		if( ca != cb )
			break;
		n++;
	}
	return n;
}

// The file name part of a path, for the affinity test.
static const char* EggBaseName( const char* path )
{
	const char* s = path;
	const char* base = path;

	while( *s )
	{
		if( *s == '/' || *s == '\\' )
			base = s + 1;
		s++;
	}
	return base;
}

bool EggArchive::CollectVolumes( const char* main )
{
	DWORD id, prev, next, payload;
	int   solid;
	char  firstPath[1024];
	char  mainDir[1024];
	HANDLE h;
	WIN32_FIND_DATAA fd;
	int   i;
	char* slash;

	lstrcpyn( firstPath, main, sizeof(firstPath) / sizeof(firstPath[0]) );
	if( !ParseVolume( firstPath, &id, &prev, &next, &payload,
	                  &solid, NULL ) )
	{
		EggSetErr( m_err, "%s: EGGA header missing", main );
		return false;
	}
	m_vol = (EggVol*)malloc( sizeof(EggVol) );
	if( !m_vol )
	{
		EggSetErr( m_err, "out of memory" );
		return false;
	}
	lstrcpyn( m_vol[0].path, firstPath, sizeof(m_vol[0].path) );
	m_vol[0].id = id;
	m_vol[0].prev = prev;
	m_vol[0].next = next;
	m_vol[0].payload = payload;
	m_vol[0].solid = solid;
	m_vol[0].score = 0x7FFFFFFF;
	m_nvol = 1;

	if( !prev && !next )        // a single volume: no scan needed
		return true;

	// the directory of the archive, with a trailing backslash
	lstrcpyn( mainDir, main, sizeof(mainDir) / sizeof(mainDir[0]) );
	slash = NULL;
	for( i = 0; mainDir[i]; i++ )
	{
		if( mainDir[i] == '/' || mainDir[i] == '\\' )
			slash = mainDir + i;
	}
	if( slash )
	{
		*(slash + 1) = '\0';        // keeps "C:\" intact
		lstrcat( mainDir, "*.egg" );
	}
	else
		lstrcpyn( mainDir, ".\\*.egg", sizeof(mainDir) );
	h = FindFirstFile( mainDir, &fd );
	if( h == INVALID_HANDLE_VALUE )
	{
		EggSetErr( m_err, "%s: directory unreadable", main );
		return false;
	}
	do
	{
		char  cand[1024 + 260];
		int   k, dup = -1, score;

		// the candidate path: the directory part + the file name
		for( k = 0; mainDir[k] && mainDir[k] != '*'; k++ )
			cand[k] = mainDir[k];
		lstrcpyn( cand + k, fd.cFileName,
		          sizeof(cand) - k * sizeof(cand[0]) );
		if( !_stricmp( cand, firstPath ) )
			continue;
		if( m_nvol >= EGG_MAXVOLS )
			continue;
		if( !ParseVolume( cand, &id, &prev, &next, &payload, &solid,
		                  NULL ) )
			continue;               // not an EGG archive
		score = EggNameScore( EggBaseName( cand ), EggBaseName( firstPath ) );
		for( k = 0; k < m_nvol; k++ )
		{
			if( m_vol[k].id == id )
			{
				dup = k;
				break;
			}
		}
		if( dup >= 0 )
		{
			// the same id from another archive: keep the better name
			if( score <= m_vol[dup].score )
				continue;
		}
		else
		{
			EggVol* grown =
			    (EggVol*)realloc( m_vol, (m_nvol + 1) * sizeof(EggVol) );

			if( !grown )
			{
				FindClose( h );
				EggSetErr( m_err, "out of memory" );
				return false;
			}
			m_vol = grown;
			dup = m_nvol++;
		}
		lstrcpyn( m_vol[dup].path, cand, sizeof(m_vol[dup].path) );
		m_vol[dup].id = id;
		m_vol[dup].prev = prev;
		m_vol[dup].next = next;
		m_vol[dup].payload = payload;
		m_vol[dup].solid = solid;
		m_vol[dup].score = score;
	}
	while( FindNextFile( h, &fd ) );
	FindClose( h );

	// walk up through prev to the head, then down through next
	{
		int   head, requested, guard;
		int*  chain;
		int   nchain;

		chain = (int*)malloc( EGG_MAXVOLS * sizeof(int) );
		if( !chain )
		{
			EggSetErr( m_err, "out of memory" );
			return false;
		}
		requested = -1;
		for( i = 0; i < m_nvol; i++ )
		{
			if( !_stricmp( m_vol[i].path, firstPath ) )
			{
				requested = i;
				break;
			}
		}
		head = requested;
		guard = 0;
		while( head >= 0 && m_vol[head].prev && guard++ < EGG_MAXVOLS )
		{
			DWORD want = m_vol[head].prev;

			for( i = 0; i < m_nvol; i++ )
			{
				if( m_vol[i].id == want )
					break;
			}
			if( i >= m_nvol )
			{
				free( chain );
				EggSetErr( m_err, "(%s) previous volume %08x missing",
				           main, want );
				return false;
			}
			head = i;
		}
		if( head >= 0 && m_vol[head].prev )
		{
			free( chain );
			EggSetErr( m_err, "(%s) cyclic volume chain (prev)", main );
			return false;
		}
		nchain = 0;
		if( head >= 0 )
		{
			for(;;)
			{
				int j;

				if( nchain >= EGG_MAXVOLS )
					break;
				chain[nchain++] = head;
				if( !m_vol[head].next )
					break;
				for( j = 0; j < m_nvol; j++ )
				{
					if( m_vol[j].id == m_vol[head].next )
						break;
				}
				if( j >= m_nvol )
				{
					free( chain );
					EggSetErr( m_err, "(%s) next volume %08x missing",
					           main, m_vol[head].next );
					return false;
				}
				head = j;
			}
		}
		{
			int in = 0;

			for( i = 0; i < nchain; i++ )
			{
				if( chain[i] == requested )
				{
					in = 1;
					break;
				}
			}
			if( !in )
			{
				free( chain );
				EggSetErr( m_err,
				           "(%s) volume missing from the Split Info chain",
				           main );
				return false;
			}
		}
		{
			EggVol* kept = (EggVol*)malloc( nchain * sizeof(EggVol) );

			if( !kept )
			{
				free( chain );
				EggSetErr( m_err, "out of memory" );
				return false;
			}
			for( i = 0; i < nchain; i++ )
				kept[i] = m_vol[chain[i]];
			free( m_vol );
			m_vol = kept;
			m_nvol = nchain;
		}
		free( chain );
	}
	return true;
}

bool EggArchive::BuildLogical()
{
	BYTE** parts;
	DWORD* sizes;
	DWORD total, off;
	int    i;

	parts = (BYTE**)calloc( m_nvol, sizeof(BYTE*) );
	sizes = (DWORD*)calloc( m_nvol, sizeof(DWORD) );
	if( !parts || !sizes )
	{
		free( parts );
		free( sizes );
		EggSetErr( m_err, "out of memory" );
		return false;
	}
	for( i = 0; i < m_nvol; i++ )
	{
		parts[i] = EggSlurp( m_vol[i].path, &sizes[i] );
		if( !parts[i] )
		{
			int j;

			for( j = 0; j < i; j++ )
				free( parts[j] );
			free( parts );
			free( sizes );
			EggSetErr( m_err, "%s: unreadable", m_vol[i].path );
			return false;
		}
		if( sizes[i] < m_vol[i].payload )
		{
			int j;

			for( j = 0; j <= i; j++ )
				free( parts[j] );
			free( parts );
			free( sizes );
			EggSetErr( m_err, "%s: volume shorter than its prefix",
			           m_vol[i].path );
			return false;
		}
	}
	total = sizes[0];
	for( i = 1; i < m_nvol; i++ )
		total += sizes[i] - m_vol[i].payload;
	m_buf = (BYTE*)malloc( (size_t)total + 1 );
	if( !m_buf )
	{
		for( i = 0; i < m_nvol; i++ )
			free( parts[i] );
		free( parts );
		free( sizes );
		EggSetErr( m_err, "out of memory (%u bytes)", total );
		return false;
	}
	memcpy( m_buf, parts[0], sizes[0] );
	off = sizes[0];
	for( i = 1; i < m_nvol; i++ )
	{
		memcpy( m_buf + off, parts[i] + m_vol[i].payload,
		        sizes[i] - m_vol[i].payload );
		off += sizes[i] - m_vol[i].payload;
		m_solid = m_solid || m_vol[i].solid;
	}
	for( i = 0; i < m_nvol; i++ )
		free( parts[i] );
	free( parts );
	free( sizes );
	m_len = total;
	return true;
}

static bool EggNeed( DWORD len, DWORD off, DWORD n )
{
	return off <= len && n <= len - off;
}

static DWORD EggSig( const BYTE* buf, DWORD len, DWORD off )
{
	return EggNeed( len, off, 4 ) ? EggLe32( buf + off ) : 0xFFFFFFFFu;
}

// one extra-field sub-header at pos: returns the data offset and the size.
// false means truncated.
static bool EggSubHeader( const BYTE* buf, DWORD len, DWORD pos,
                          DWORD* data, DWORD* size )
{
	BYTE flags;

	if( !EggNeed( len, pos, 5 ) )
		return false;
	flags = buf[pos + 4];
	if( flags & 1 )
	{
		if( !EggNeed( len, pos, 4 + 5 ) )
			return false;
		*size = EggLe32( buf + pos + 5 );
		*data = pos + 4 + 5;
	}
	else
	{
		if( !EggNeed( len, pos, 4 + 3 ) )
			return false;
		*size = EggLe16( buf + pos + 5 );
		*data = pos + 4 + 3;
	}
	return EggNeed( len, *data, *size );
}

bool EggArchive::NewMember( int* idx )
{
	EggMember* grown;

	if( m_count >= m_cap )
	{
		int cap = m_cap ? m_cap * 2 : 16;

		grown = (EggMember*)realloc( m_mem, cap * sizeof(EggMember) );
		if( !grown )
			return false;
		m_mem = grown;
		m_cap = cap;
	}
	m_mem[m_count].Zero();
	*idx = m_count++;
	return true;
}

bool EggArchive::NewBlock( int* idx )
{
	EggBlock* grown;

	if( m_nblk >= m_blkcap )
	{
		int cap = m_blkcap ? m_blkcap * 2 : 16;

		grown = (EggBlock*)realloc( m_blk, cap * sizeof(EggBlock) );
		if( !grown )
			return false;
		m_blk = grown;
		m_blkcap = cap;
	}
	*idx = m_nblk++;
	return true;
}

// parses the blocks of one member, from *pos.  Returns the index of the
// first new block and the block count.  *pos sits after them on exit.
bool EggArchive::ParseBlocks( DWORD* pos, int* firstIdx, int* nIdx )
{
	*firstIdx = m_nblk;
	*nIdx = 0;
	for(;;)
	{
		DWORD sig, data, size;
		int   idx;

		if( !EggNeed( m_len, *pos, 4 ) )
		{
			EggSetErr( m_err, "truncated blocks @%#x", *pos );
			return false;
		}
		sig = EggSig( m_buf, m_len, *pos );
		if( sig == EGG_SIG_COMMENT || sig == EGG_SIG_FILE ||
		    sig == EGG_SIG_END )
			return true;
		if( sig == EGG_SIG_ENCRYPT || sig == EGG_SIG_DUMMY )
		{
			if( !EggSubHeader( m_buf, m_len, *pos, &data, &size ) )
			{
				EggSetErr( m_err,
				           "truncated sub-header between blocks @%#x",
				           *pos );
				return false;
			}
			*pos = data + size;
			continue;
		}
		if( sig != EGG_SIG_BLOCK )
		{
			EggSetErr( m_err, "unexpected block %08x @%#x", sig, *pos );
			return false;
		}
		if( !EggNeed( m_len, *pos, EGG_BLK_HDRSZ ) )
		{
			EggSetErr( m_err, "truncated block header @%#x", *pos );
			return false;
		}
		if( EggSig( m_buf, m_len, *pos + EGG_BLK_HDRSZ ) != EGG_SIG_END )
		{
			EggSetErr( m_err,
			           "missing end marker after the block header @%#x",
			           *pos + EGG_BLK_HDRSZ );
			return false;
		}
		if( !NewBlock( &idx ) )
		{
			EggSetErr( m_err, "out of memory" );
			return false;
		}
		m_blk[idx].method = m_buf[*pos + 4];
		m_blk[idx].hint   = m_buf[*pos + 5];
		m_blk[idx].usize  = EggLe32( m_buf + *pos + 6 );
		m_blk[idx].csize  = EggLe32( m_buf + *pos + 10 );
		m_blk[idx].crc    = EggLe32( m_buf + *pos + 14 );
		{
			DWORD start = *pos + EGG_BLK_HDRSZ + 4;

			if( start > m_len || m_blk[idx].csize > m_len - start )
			{
				EggSetErr( m_err, "truncated block data (%u expected) @%#x "
				           "(stream %u bytes, %d volume(s))",
				           m_blk[idx].csize, start, m_len, m_nvol );
				return false;
			}
			m_blk[idx].data = start;
			*pos = start + m_blk[idx].csize;
		}
		(*nIdx)++;
	}
}

// parses one FILE header and its sub-headers/blocks at off.  The position
// after them is returned in *retPos.
bool EggArchive::ParseMember( DWORD off, DWORD* retPos )
{
	DWORD pos, sig, data, size;
	DWORD fileId;
	unsigned __int64 usize;
	char  name[EGG_NAME_MAX];
	int   namelen = -1;
	int   crypt = EGG_CRYPT_NONE;
	BYTE  cryptInfo[64];
	DWORD cryptInfoLen = 0;
	BYTE  attrs = 0;
	BOOL  hasWinInfo = FALSE;
	unsigned __int64 filetime = 0;
	int   idx, first, n;

	if( !EggNeed( m_len, off, 16 ) )
	{
		EggSetErr( m_err, "truncated file header @%#x", off );
		return false;
	}
	fileId = EggLe32( m_buf + off + 4 );
	usize  = EggLe64( m_buf + off + 8 );
	pos = off + 16;

	// sub-headers
	for(;;)
	{
		BYTE flags;

		if( !EggNeed( m_len, pos, 4 ) )
		{
			EggSetErr( m_err, "truncated sub-headers @%#x", pos );
			return false;
		}
		sig = EggSig( m_buf, m_len, pos );
		if( sig == EGG_SIG_END )
		{
			pos += 4;
			break;
		}
		if( sig == EGG_SIG_BLOCK )
		{
			EggSetErr( m_err,
			           "unterminated sub-headers before a block @%#x",
			           pos );
			return false;
		}
		if( !EggSubHeader( m_buf, m_len, pos, &data, &size ) )
		{
			EggSetErr( m_err, "truncated sub-header @%#x", pos );
			return false;
		}
		flags = m_buf[pos + 4];
		if( sig == EGG_SIG_FILENAME )
		{
			DWORD q = data;

			if( flags & 0x08 ) q += 2;       // the locale
			if( flags & 0x10 ) q += 4;       // the parent path id
			if( q > data + size ||
			    data + size - q >= (DWORD)sizeof(name) )
			{
				EggSetErr( m_err, "Filename: inconsistent flags @%#x",
				           pos );
				return false;
			}
			namelen = (int)(data + size - q);
			memcpy( name, m_buf + q, namelen );
			name[namelen] = '\0';
		}
		else if( sig == EGG_SIG_WININFO )
		{
			if( size != 9 )
			{
				EggSetErr( m_err, "Windows File Info of size %u", size );
				return false;
			}
			filetime = EggLe64( m_buf + data );
			attrs = m_buf[data + 8];
			hasWinInfo = TRUE;
		}
		else if( sig == EGG_SIG_ENCRYPT )
		{
			if( size < 1 )
			{
				EggSetErr( m_err, "empty Encrypt Info" );
				return false;
			}
			crypt = m_buf[data];
			if( crypt != EGG_CRYPT_ZIP && crypt != EGG_CRYPT_AES128 &&
			    crypt != EGG_CRYPT_AES256 && crypt != EGG_CRYPT_LEA128 &&
			    crypt != EGG_CRYPT_LEA256 )
			{
				EggSetErr( m_err, "unknown encryption method %d", crypt );
				return false;
			}
			// the key material (method byte excluded)
			{
				DWORD n = size - 1;

				if( n > sizeof(cryptInfo) )
					n = sizeof(cryptInfo);
				memcpy( cryptInfo, m_buf + data + 1, n );
				cryptInfoLen = n;
			}
		}
		else if( sig != EGG_SIG_COMMENT && sig != EGG_SIG_POSIXINFO &&
		         sig != EGG_SIG_DUMMY )
		{
			EggSetErr( m_err, "unexpected sub-header %08x @%#x", sig, pos );
			return false;
		}
		pos = data + size;
	}

	if( !NewMember( &idx ) )
	{
		EggSetErr( m_err, "out of memory" );
		return false;
	}
	if( !ParseBlocks( &pos, &first, &n ) )
		return false;
	if( namelen < 0 )
	{
		EggSetErr( m_err, "member #%u without a file name", fileId );
		return false;
	}
	m_mem[idx].fileId = fileId;
	m_mem[idx].usize = usize;
	m_mem[idx].crypt = crypt;
	if( cryptInfoLen )
		memcpy( m_mem[idx].cryptInfo, cryptInfo, cryptInfoLen );
	m_mem[idx].cryptInfoLen = cryptInfoLen;
	m_mem[idx].attrs = attrs;
	m_mem[idx].hasWinInfo = hasWinInfo;
	m_mem[idx].filetime = filetime;
	lstrcpyn( m_mem[idx].name, name, EGG_NAME_MAX );
	m_mem[idx].nblocks = n;
	m_mem[idx].firstblock = first;
	*retPos = pos;
	return true;
}

bool EggArchive::Parse()
{
	DWORD pos, sig, data, size;

	if( !EggNeed( m_len, 0, EGG_HDRSZ ) ||
	    EggLe32( m_buf ) != EGG_SIG_ARCHIVE )
	{
		EggSetErr( m_err, "EGGA header missing from the logical stream" );
		return false;
	}
	pos = EGG_HDRSZ;

	// the prefix section
	for(;;)
	{
		if( !EggNeed( m_len, pos, 4 ) )
		{
			EggSetErr( m_err, "truncated prefix @%#x", pos );
			return false;
		}
		sig = EggSig( m_buf, m_len, pos );
		if( sig == EGG_SIG_END )
		{
			pos += 4;
			break;
		}
		if( sig == EGG_SIG_FILE )
			break;              // an empty prefix
		if( !EggSubHeader( m_buf, m_len, pos, &data, &size ) )
		{
			EggSetErr( m_err, "truncated prefix @%#x", pos );
			return false;
		}
		if( sig == EGG_SIG_SOLID )
			m_solid = 1;
		else if( sig != EGG_SIG_SPLIT && sig != EGG_SIG_DUMMY &&
		         sig != EGG_SIG_SKIP && sig != EGG_SIG_GENCRYPT )
		{
			EggSetErr( m_err, "unexpected prefix sub-header %08x @%#x",
			           sig, pos );
			return false;
		}
		pos = data + size;
	}

	// the members
	for(;;)
	{
		if( !EggNeed( m_len, pos, 4 ) )
		{
			EggSetErr( m_err, "final end marker absent @%#x", pos );
			return false;
		}
		sig = EggSig( m_buf, m_len, pos );
		if( sig == EGG_SIG_END )
		{
			pos += 4;
			if( pos != m_len )
			{
				EggSetErr( m_err,
				           "data after the final end marker @%#x", pos );
				return false;
			}
			return true;
		}
		if( sig == EGG_SIG_COMMENT )
		{
			if( !EggSubHeader( m_buf, m_len, pos, &data, &size ) )
			{
				EggSetErr( m_err, "truncated archive comment @%#x", pos );
				return false;
			}
			pos = data + size;
			continue;
		}
		if( sig != EGG_SIG_FILE )
		{
			EggSetErr( m_err, "expected a file header, found %08x @%#x",
			           sig, pos );
			return false;
		}
		if( !ParseMember( pos, &pos ) )
			return false;
	}
}

bool EggArchive::Open( const char* main )
{
	Close();
	if( !CollectVolumes( main ) )
		return false;
	if( !BuildLogical() )
		return false;
	if( !Parse() )
	{
		Close();
		return false;
	}
	return true;
}

bool EggArchive::CheckUsize( const EggMember* m, char* why ) const
{
	unsigned __int64 total = 0;
	int i;

	for( i = 0; i < m->nblocks; i++ )
		total += m_blk[m->firstblock + i].usize;
	if( total != m->usize )
	{
		if( why )
			sprintf( why, "'%s': blocks %I64u bytes, header %I64u",
			         m->name, total, m->usize );
		return false;
	}
	if( total > 0x7FFFFFFFu )
	{
		if( why )
			sprintf( why, "'%s': member too large", m->name );
		return false;
	}
	return true;
}

//-- the block decoders -----------------------

bool EggArchive::DecodeDeflate( const BYTE* src, DWORD csize, BYTE* dst,
                                DWORD usize, char* why )
{
	z_stream s;
	int      r;

	memset( &s, 0, sizeof(s) );
	s.next_in = (Bytef*)src;
	s.avail_in = csize;
	s.next_out = (Bytef*)dst;
	s.avail_out = usize;
	if( inflateInit2( &s, -15 ) != Z_OK )
	{
		if( why )
			strcpy( why, "inflate: out of memory" );
		return false;
	}
	r = inflate( &s, Z_FINISH );
	inflateEnd( &s );
	if( s.total_out != usize )
	{
		if( why )
			sprintf( why, "inflate: %lu bytes for %u expected",
			         s.total_out, usize );
		return false;
	}
	(void)r;
	return true;
}

// Pushes one deflate stream into a sink, in 64 KiB slices.  The total must
// be exactly usize when the input runs out.
bool EggArchive::DecodeDeflatePush( const BYTE* src, DWORD csize, void* sctx,
                                    EggPushFn push, DWORD usize, char* why )
{
	z_stream s;
	BYTE     out[64 * 1024];
	DWORD    total = 0;
	DWORD    off = 0;
	int      r = Z_OK;

	memset( &s, 0, sizeof(s) );
	if( inflateInit2( &s, -15 ) != Z_OK )
	{
		if( why )
			strcpy( why, "inflate: out of memory" );
		return false;
	}
	while( r != Z_STREAM_END )
	{
		DWORD n = csize - off > 32768 ? 32768 : csize - off;

		s.next_in = (Bytef*)(src + off);
		s.avail_in = n;
		s.next_out = out;
		s.avail_out = sizeof(out);
		r = inflate( &s, Z_NO_FLUSH );
		if( r != Z_OK && r != Z_STREAM_END )
		{
			inflateEnd( &s );
			if( why )
				strcpy( why, "inflate: corrupt stream" );
			return false;
		}
		total += (DWORD)(sizeof(out) - s.avail_out);
		off += n - s.avail_in;
		if( sizeof(out) - s.avail_out )
		{
			if( push( sctx, out,
			    (DWORD)(sizeof(out) - s.avail_out) ) < 0 )
			{
				inflateEnd( &s );
				return false;
			}
		}
		if( total > usize )
		{
			inflateEnd( &s );
			if( why )
				strcpy( why, "solid stream: more bytes than the members" );
			return false;
		}
		if( r != Z_STREAM_END && s.avail_in == 0 && off == csize )
			break;
	}
	inflateEnd( &s );
	if( total != usize )
	{
		if( why )
			sprintf( why, "inflate: %u bytes for %u expected", total,
			         usize );
		return false;
	}
	return true;
}

bool EggArchive::DecodeBzip2( const BYTE* src, DWORD csize, BYTE* dst,
                              DWORD usize, char* why )
{
	unsigned int got = usize;
	int          r;

	if( usize == 0 )
		return true;
	r = BZ2_bzBuffToBuffDecompress( (char*)dst, &got, (char*)src,
	                                csize, 0, 0 );
	if( r != BZ_OK || got != usize )
	{
		if( why )
			sprintf( why, "bzip2: %s (code %d)",
			         r == BZ_MEM_ERROR ? "out of memory" :
			         r == BZ_DATA_ERROR ? "corrupt stream" :
			         r == BZ_DATA_ERROR_MAGIC ? "not bzip2" :
			         r == BZ_OUTBUFF_FULL ? "output full" : "failed", r );
		return false;
	}
	return true;
}

// Pushes one bzip2 stream into a sink.
bool EggArchive::DecodeBzip2Push( const BYTE* src, DWORD csize, void* sctx,
                                  EggPushFn push, DWORD usize, char* why )
{
	bz_stream s;
	BYTE      out[64 * 1024];
	DWORD     total = 0;
	DWORD     off = 0;
	int       r = BZ_OK;

	memset( &s, 0, sizeof(s) );
	r = BZ2_bzDecompressInit( &s, 0, 0 );
	if( r != BZ_OK )
	{
		if( why )
			strcpy( why, "bzip2: out of memory" );
		return false;
	}
	while( r != BZ_STREAM_END )
	{
		DWORD n = csize - off > 32768 ? 32768 : csize - off;

		s.next_in = (char*)(src + off);
		s.avail_in = n;
		s.next_out = (char*)out;
		s.avail_out = sizeof(out);
		r = BZ2_bzDecompress( &s );
		if( r != BZ_OK && r != BZ_STREAM_END )
		{
			BZ2_bzDecompressEnd( &s );
			if( why )
				sprintf( why, "bzip2: corrupt stream (code %d)", r );
			return false;
		}
		total += (DWORD)(sizeof(out) - s.avail_out);
		off += n - s.avail_in;
		if( sizeof(out) - s.avail_out )
		{
			if( push( sctx, out,
			    (DWORD)(sizeof(out) - s.avail_out) ) < 0 )
			{
				BZ2_bzDecompressEnd( &s );
				return false;
			}
		}
		if( total > usize )
		{
			BZ2_bzDecompressEnd( &s );
			if( why )
				strcpy( why, "solid stream: more bytes than the members" );
			return false;
		}
		if( r != BZ_STREAM_END && s.avail_in == 0 && off == csize )
			break;
	}
	BZ2_bzDecompressEnd( &s );
	if( total != usize )
	{
		if( why )
			sprintf( why, "bzip2: %u bytes for %u expected", total, usize );
		return false;
	}
	return true;
}

// One LZMA block is fed here: the props are the 5 bytes at +4 and the raw
// stream starts at +9.  A buffer sink (dst != NULL) counts itself; a push
// sink (dst == NULL) is called straight away and must count for itself.
struct EggLzIn
{
	const BYTE* p;
	DWORD left;
};

static int EggLzFill( void* ctx, BYTE* buf, int want )
{
	EggLzIn* in = (EggLzIn*)ctx;
	DWORD    n;

	n = in->left < (DWORD)want ? in->left : (DWORD)want;
	if( n == 0 )
		return 0;
	memcpy( buf, in->p, n );
	in->p += n;
	in->left -= n;
	return (int)n;
}

static void EggLzWrite( void* ctx, const BYTE* buf, int len )
{
	EggLzSink* o = (EggLzSink*)ctx;

	if( o->bad )
		return;
	if( o->dst )
	{
		if( (DWORD)len > o->left )
		{
			o->bad = true;
			return;
		}
		memcpy( o->dst, buf, len );
		o->dst += len;
		o->left -= len;
	}
	else if( o->push( o->sctx, buf, (DWORD)len ) < 0 )
		o->bad = true;
}

bool EggArchive::DecodeLzma( const BYTE* src, DWORD csize, EggLzSink* o,
                             DWORD usize, char* why )
{
	EggLzIn in;
	bool    ok;

	if( csize < 9 )
	{
		if( why )
			strcpy( why, "LZMA: incomplete 9-byte header" );
		return false;
	}
	in.p = src + 9;
	in.left = csize - 9;
	ok = LzmaDecodeStream( src + 4, EggLzFill, &in, EggLzWrite, o,
	                       usize, true );
	if( !ok || o->bad )
	{
		if( why )
			strcpy( why, "LZMA: corrupt stream" );
		return false;
	}
	return true;
}

bool EggArchive::DecodeAzo( const BYTE* src, DWORD csize, BYTE* dst,
                            DWORD usize, char* why )
{
	if( !EggAzoDecode( src, csize, dst, usize ) )
	{
		if( why )
			strcpy( why, "AZO: corrupt stream" );
		return false;
	}
	return true;
}

//-- the solid walker -----------------------

// The splitting sink of a solid archive: the decoded bytes arrive in order
// and are distributed over the members (each block also verifies its own
// CRC when it closes).
struct SolidOut
{
	EggArchive* arc;
	EggSink*    sink;
	int   ent;            // the next entry to open
	int   have;           // the open entry index, -1 = none
	unsigned __int64 entry_left;
	int   blk;            // the current block of the solid stream
	DWORD blk_left;
	DWORD blk_crc;
	const EggBlock* blocks;
	int   nblocks;
	unsigned __int64 total;
	unsigned __int64 expect;
	char  why[256];
	bool  bad;
};

static bool SolidOpenNext( SolidOut* s )
{
	for(;;)
	{
		EggMember m;
		int       r;

		if( s->ent >= s->arc->Count() )
			return true;    // done: no more entries to open
		s->arc->Member( s->ent, &m );
		r = s->sink->open( s->sink->ctx, s->ent, &m );
		s->ent++;
		if( r < 0 )
		{
			sprintf( s->why, "could not create '%.200s'", m.name );
			return false;
		}
		if( r == 0 )
			continue;       // a directory or a skipped member
		s->have = s->ent - 1;
		s->entry_left = m.usize;
		if( s->entry_left == 0 )    // an empty file
		{
			s->sink->close( s->sink->ctx, s->have );
			s->have = -1;
			continue;
		}
		return true;
	}
}

static bool SolidFinishBlock( SolidOut* s )
{
	if( s->blk >= s->nblocks )
	{
		sprintf( s->why, "solid stream: more bytes than blocks" );
		return false;
	}
	if( s->blk_crc != s->blocks[s->blk].crc )
	{
		sprintf( s->why,
		         "solid stream: diverging CRC (block %d: expected %08x, computed %08x)",
		         s->blk, s->blocks[s->blk].crc, s->blk_crc );
		return false;
	}
	s->blk++;
	while( s->blk < s->nblocks && s->blocks[s->blk].usize == 0 )
	{
		if( s->blocks[s->blk].crc != 0 )
		{
			sprintf( s->why, "solid stream: CRC diverges (empty block)" );
			return false;
		}
		s->blk++;
	}
	if( s->blk < s->nblocks )
	{
		s->blk_left = s->blocks[s->blk].usize;
		s->blk_crc = 0;
	}
	else
	{
		s->blk_left = 0;
		s->blk_crc = 0;
	}
	return true;
}

static int SolidPush( void* ctx, const BYTE* d, DWORD n )
{
	SolidOut* s = (SolidOut*)ctx;

	while( n && !s->bad )
	{
		DWORD take;

		if( s->have < 0 )
		{
			while( s->blk < s->nblocks && s->blk_left == 0 )
			{
				if( !SolidFinishBlock( s ) )
				{
					s->bad = true;
					return -1;
				}
			}
			if( !SolidOpenNext( s ) )
			{
				s->bad = true;
				return -1;
			}
			if( s->have < 0 )
			{
				sprintf( s->why,
				         "solid stream: more bytes than the members" );
				s->bad = true;
				return -1;
			}
		}
		if( s->blk >= s->nblocks )
		{
			sprintf( s->why, "solid stream: more bytes than blocks" );
			s->bad = true;
			return -1;
		}
		take = n;
		if( take > (DWORD)s->entry_left )
			take = (DWORD)s->entry_left;
		if( take > s->blk_left )
			take = s->blk_left;
		if( take == 0 )
		{
			if( !SolidFinishBlock( s ) )
			{
				s->bad = true;
				return -1;
			}
			continue;
		}
		if( s->sink->write( s->sink->ctx, d, take ) < 0 )
		{
			s->bad = true;
			return -1;
		}
		s->blk_crc = EggCrc32( s->blk_crc, d, take );
		s->blk_left -= take;
		s->entry_left -= take;
		s->total += take;
		d += take;
		n -= take;
		if( s->blk_left == 0 && !SolidFinishBlock( s ) )
		{
			s->bad = true;
			return -1;
		}
		if( s->entry_left == 0 )
		{
			s->sink->close( s->sink->ctx, s->have );
			s->have = -1;
		}
	}
	return s->bad ? -1 : 1;
}

static bool SolidFinish( SolidOut* s )
{
	EggMember m;
	int       r;

	while( s->blk < s->nblocks && s->blk_left == 0 )
	{
		if( !SolidFinishBlock( s ) )
			return false;
	}
	// create any trailing directories or empty files after the data
	for(;;)
	{
		if( s->ent >= s->arc->Count() )
			break;
		s->arc->Member( s->ent, &m );
		if( !m.IsDir() && m.usize > 0 )
			break;               // bytes are missing; caught below
		r = s->sink->open( s->sink->ctx, s->ent, &m );
		s->ent++;
		if( r < 0 )
		{
			sprintf( s->why, "could not create '%.200s'", m.name );
			return false;
		}
		if( r != 0 )
			s->sink->close( s->sink->ctx, s->ent - 1 );
	}
	if( s->have >= 0 || s->total != s->expect )
	{
		sprintf( s->why, "solid stream: truncated (%I64u of %I64u bytes)",
		         s->total, s->expect );
		return false;
	}
	return true;
}

// the solid stream of one method: the concatenated block payloads starting
// at cbuf, decoded once and split by SolidPush.
bool EggArchive::DecodeSolid( const EggBlock* blocks, int nblocks,
                              unsigned __int64 expect, BYTE* cbuf,
                              EggSink* sink )
{
	SolidOut s;
	BYTE     method = blocks[0].method;
	char     why[256];
	bool     ok = false;

	memset( &s, 0, sizeof(s) );
	s.arc = this;
	s.sink = sink;
	s.blk = 0;
	s.blocks = blocks;
	s.nblocks = nblocks;
	s.expect = expect;
	s.have = -1;
	s.ent = 0;
	while( s.blk < s.nblocks && s.blocks[s.blk].usize == 0 )
		s.blk++;
	if( s.blk < s.nblocks )
		s.blk_left = s.blocks[s.blk].usize;
	why[0] = '\0';

	if( method == EGG_M_STORE )
	{
		DWORD off = 0;
		DWORD left = (DWORD)expect;

		while( left )
		{
			DWORD n = left > 64 * 1024 ? 64 * 1024 : left;

			if( SolidPush( &s, cbuf + off, n ) < 0 )
				goto fail;
			off += n;
			left -= n;
		}
		ok = true;
	}
	else if( method == EGG_M_DEFLATE )
	{
		DWORD csize = 0;
		int   i;

		for( i = 0; i < nblocks; i++ )
			csize += blocks[i].csize;
		ok = DecodeDeflatePush( cbuf, csize, &s, SolidPush,
		                        (DWORD)expect, why );
	}
	else if( method == EGG_M_BZIP2 )
	{
		DWORD csize = 0;
		int   i;

		for( i = 0; i < nblocks; i++ )
			csize += blocks[i].csize;
		ok = DecodeBzip2Push( cbuf, csize, &s, SolidPush,
		                      (DWORD)expect, why );
	}
	else if( method == EGG_M_LZMA )
	{
		EggLzSink o;
		DWORD     csize = 0;
		int       i;

		for( i = 0; i < nblocks; i++ )
			csize += blocks[i].csize;
		o.push = SolidPush;
		o.sctx = &s;
		o.dst = NULL;
		o.left = 0;
		o.bad = false;
		ok = DecodeLzma( cbuf, csize, &o, (DWORD)expect, why );
		if( o.bad )
		{
			if( !why[0] )
				strcpy( why, "LZMA: size mismatch" );
			ok = false;
		}
	}
	else if( method == EGG_M_AZO )
	{
		DWORD csize = 0;
		BYTE* obuf;
		DWORD off;
		int   i;

		// AZO is only emitted by the Optimized profile, which solid mode
		// forbids: no sample can exist.  The block is decoded into one
		// buffer, like the reference, rather than refused.
		for( i = 0; i < nblocks; i++ )
			csize += blocks[i].csize;
		obuf = (BYTE*)malloc( (size_t)expect );
		if( !obuf )
		{
			strcpy( why, "out of memory (AZO solid stream)" );
			goto done;
		}
		ok = DecodeAzo( cbuf, csize, obuf, (DWORD)expect, why );
		if( ok )
		{
			off = 0;
			while( off < (DWORD)expect )
			{
				DWORD n = (DWORD)expect - off > 64 * 1024 ?
				          64 * 1024 : (DWORD)expect - off;

				if( SolidPush( &s, obuf + off, n ) < 0 )
				{
					ok = false;
					break;
				}
				off += n;
			}
		}
		free( obuf );
	}
	else
	{
		strcpy( why, "solid stream: unsupported method" );
	}

done:
	if( ok && !s.bad )
		ok = SolidFinish( &s );
	if( !ok )
	{
		if( s.bad && s.why[0] )
			EggSetErr( m_err, "%s", s.why );
		else if( why[0] )
			EggSetErr( m_err, "%s", why );
	}
fail:
	return ok;
}

//-- extraction -----------------------

bool EggArchive::Extract( EggSink* sink )
{
	int i, j;

	// preflight: refuse anything this tree cannot decode, before writing
	m_pwBad = false;
	for( i = 0; i < m_count; i++ )
	{
		EggMember* m = &m_mem[i];

		if( m->crypt != EGG_CRYPT_NONE )
		{
			if( !m_password[0] )
			{
				EggSetErr( m_err, "'%s' is encrypted: password required",
				           m->name );
				m_pwBad = true;
				return false;
			}
			if( m_solid )
			{
				// ALZip never writes an encrypted solid archive
				EggSetErr( m_err, "solid AND encrypted archive: refused" );
				return false;
			}
		}
		for( j = 0; j < m->nblocks; j++ )
		{
			BYTE method = m_blk[m->firstblock + j].method;

			if( method != EGG_M_STORE && method != EGG_M_DEFLATE &&
			    method != EGG_M_BZIP2 && method != EGG_M_AZO &&
			    method != EGG_M_LZMA )
			{
				EggSetErr( m_err, "'%s' uses block method %u: not implemented",
				           m->name, method );
				return false;
			}
		}
	}
	if( m_solid && m_count )
	{
		BYTE method = 0xFF;
		unsigned __int64 expect = 0;
		DWORD    ctotal = 0;
		BYTE*    cbuf;
		DWORD    off;

		for( i = 0; i < m_count; i++ )
		{
			EggMember* m = &m_mem[i];

			if( m->IsDir() )
				continue;
			expect += m->usize;
			for( j = 0; j < m->nblocks; j++ )
			{
				const EggBlock* b = &m_blk[m->firstblock + j];

				if( method == 0xFF )
					method = b->method;
				else if( b->method != method )
				{
					EggSetErr( m_err,
					           "solid stream: mixed block methods (%u and %u)",
					           method, b->method );
					return false;
				}
				ctotal += b->csize;
			}
		}
		if( !m_nblk )
		{
			// a dirs-only (or empty) archive: materialise the entries
			for( i = 0; i < m_count; i++ )
			{
				EggMember m;

				Member( i, &m );
				if( !m.IsDir() && m.usize > 0 )
				{
					EggSetErr( m_err, "solid stream: empty" );
					return false;
				}
				sink->open( sink->ctx, i, &m );
				sink->close( sink->ctx, i );
			}
			return true;
		}
		if( expect > 0x7FFFFFFFu )
		{
			EggSetErr( m_err, "solid stream: too large" );
			return false;
		}
		if( method == EGG_M_STORE && (unsigned __int64)ctotal != expect )
		{
			EggSetErr( m_err, "solid store stream: %u bytes for %I64u "
			           "expected", ctotal, expect );
			return false;
		}
		cbuf = (BYTE*)malloc( ctotal ? ctotal : 1 );
		if( !cbuf )
		{
			EggSetErr( m_err, "out of memory (%u bytes)", ctotal );
			return false;
		}
		off = 0;
		for( i = 0; i < m_count; i++ )
		{
			EggMember* m = &m_mem[i];

			for( j = 0; j < m->nblocks; j++ )
			{
				const EggBlock* b = &m_blk[m->firstblock + j];

				memcpy( cbuf + off, m_buf + b->data, b->csize );
				off += b->csize;
			}
		}
		if( !DecodeSolid( m_blk, m_nblk, expect, cbuf, sink ) )
		{
			free( cbuf );
			return false;
		}
		free( cbuf );
		return true;
	}

	// not solid: each member is decoded block by block
	for( i = 0; i < m_count; i++ )
	{
		if( !DecodeMember( i, sink ) )
			return false;
	}
	return true;
}

void EggArchive::SetPassword( const char* p )
{
	if( !p )
		p = "";
	lstrcpyn( m_password, p, sizeof(m_password) / sizeof(m_password[0]) );
	m_pwBad = false;
}

bool EggArchive::HasEncrypted() const
{
	int i;

	for( i = 0; i < m_count; i++ )
	{
		if( m_mem[i].crypt != EGG_CRYPT_NONE )
			return true;
	}
	return false;
}

// Decodes one member's blocks into the sink, or only counts them when sink
// is NULL (the ZipCrypto whole-member verifier pass).  `dc`, when given,
// decrypts the ciphertext in block order and keeps the stream state across
// the blocks, as EGG encrypts the member as one continuous stream.
bool EggArchive::DecodeMemberBody( int idx, EggDecrypt* dc, EggSink* sink,
                                   DWORD* memberCrc, char* why )
{
	const EggMember* m = &m_mem[idx];
	BYTE* chunk = NULL;
	DWORD crc = 0;
	int   j;

	if( memberCrc )
		*memberCrc = 0;
	if( why )
		why[0] = '\0';
	if( dc )
	{
		chunk = (BYTE*)malloc( 1024 * 1024 );
		if( !chunk )
		{
			strcpy( why, "out of memory" );
			return false;
		}
	}
	for( j = 0; j < m->nblocks; j++ )
	{
		const EggBlock* b = &m_blk[m->firstblock + j];
		DWORD bcrc = 0;

		if( b->usize == 0 )
		{
			if( b->csize != 0 )
			{
				sprintf( why, "'%s': empty block %d carries %u bytes",
				         m->name, j, b->csize );
				goto fail;
			}
			if( b->crc != 0 )
			{
				sprintf( why, "'%s': CRC mismatch (block %d: expected %08x, "
				         "computed 00000000)", m->name, j, b->crc );
				goto fail;
			}
			continue;
		}
		if( b->method == EGG_M_STORE )
		{
			DWORD left = b->usize;
			DWORD off = b->data;

			if( b->csize != b->usize )
			{
				sprintf( why, "'%s': store block %u/%u", m->name,
				         b->csize, b->usize );
				goto fail;
			}
			while( left )
			{
				DWORD n = left > 1024 * 1024 ? 1024 * 1024 : left;
				const BYTE* src;

				if( dc )
				{
					memcpy( chunk, m_buf + off, n );
					dc->Update( chunk, n );
					src = chunk;
				}
				else
					src = m_buf + off;
				bcrc = EggCrc32( bcrc, src, n );
				crc = EggCrc32( crc, src, n );
				if( sink && sink->write( sink->ctx, src, n ) < 0 )
				{
					sprintf( why, "short write for '%s'", m->name );
					goto fail;
				}
				off += n;
				left -= n;
			}
		}
		else
		{
			BYTE* cbuf = NULL;
			BYTE* obuf;
			const BYTE* src;
			bool  okd = false;

			if( dc )
			{
				cbuf = (BYTE*)malloc( b->csize ? b->csize : 1 );
				if( !cbuf )
				{
					sprintf( why, "out of memory for '%s'", m->name );
					goto fail;
				}
				memcpy( cbuf, m_buf + b->data, b->csize );
				dc->Update( cbuf, b->csize );
				src = cbuf;
			}
			else
				src = m_buf + b->data;

			obuf = (BYTE*)malloc( b->usize ? b->usize : 1 );
			if( !obuf )
			{
				free( cbuf );
				sprintf( why, "out of memory for '%s'", m->name );
				goto fail;
			}
			if( b->method == EGG_M_DEFLATE )
				okd = DecodeDeflate( src, b->csize, obuf, b->usize, why );
			else if( b->method == EGG_M_BZIP2 )
				okd = DecodeBzip2( src, b->csize, obuf, b->usize, why );
			else if( b->method == EGG_M_LZMA )
			{
				EggLzSink o;

				o.push = NULL;
				o.sctx = NULL;
				o.dst = obuf;
				o.left = b->usize;
				o.bad = false;
				okd = DecodeLzma( src, b->csize, &o, b->usize, why );
				if( !why[0] && (o.bad || o.left != 0) )
					strcpy( why, "LZMA: size mismatch" );
				if( o.bad || o.left != 0 )
					okd = false;
			}
			else
				okd = DecodeAzo( src, b->csize, obuf, b->usize, why );
			free( cbuf );
			if( !okd )
			{
				char what[256 + EGG_NAME_MAX];

				if( !why[0] )
					strcpy( why, "decode failed" );
				sprintf( what, "'%s': %s (block %d)", m->name, why, j );
				strcpy( why, what );
				free( obuf );
				goto fail;
			}
			bcrc = EggCrc32( 0, obuf, b->usize );
			crc = EggCrc32( crc, obuf, b->usize );
			{
				DWORD left = b->usize;
				DWORD off = 0;

				while( left )
				{
					DWORD n = left > 1024 * 1024 ? 1024 * 1024 : left;

					if( sink && sink->write( sink->ctx, obuf + off, n ) < 0 )
					{
						sprintf( why, "short write for '%s'", m->name );
						free( obuf );
						goto fail;
					}
					off += n;
					left -= n;
				}
			}
			free( obuf );
		}
		if( bcrc != b->crc )
		{
			sprintf( why, "'%s': CRC mismatch (block %d: expected %08x, "
			         "computed %08x)", m->name, j, b->crc, bcrc );
			goto fail;
		}
	}
	if( memberCrc )
		*memberCrc = crc;
	free( chunk );
	return true;

fail:
	free( chunk );
	return false;
}

bool EggArchive::DecodeMember( int idx, EggSink* sink )
{
	const EggMember* m = &m_mem[idx];
	EggDecrypt dc;
	char  why[256 + EGG_NAME_MAX];
	DWORD mcrc = 0;
	bool  encrypted = m->crypt != EGG_CRYPT_NONE;
	int   r, j;

	if( !CheckUsize( m, why ) )
	{
		EggSetErr( m_err, "%s", why );
		sink->fail( sink->ctx, idx, why );
		return false;
	}

	if( encrypted )
	{
		if( !dc.Begin( m->crypt, m->cryptInfo, m->cryptInfoLen,
		               m_password, why ) )
		{
			char what[256 + EGG_NAME_MAX];

			sprintf( what, "'%s': %s", m->name, why );
			EggSetErr( m_err, "%s", what );
			sink->fail( sink->ctx, idx, what );
			m_pwBad = true;
			return false;
		}
		if( !dc.IsZip() )
		{
			for( j = 0; j < m->nblocks; j++ )
			{
				const EggBlock* b = &m_blk[m->firstblock + j];

				if( !dc.Auth( m_buf + b->data, b->csize ) )
					break;
			}
			if( j != m->nblocks || !dc.Ready( why ) )
			{
				char what[256 + EGG_NAME_MAX];

				if( !why[0] )
					strcpy( why, "authentication failed" );
				sprintf( what, "'%s': %s", m->name, why );
				EggSetErr( m_err, "%s", what );
				sink->fail( sink->ctx, idx, what );
				m_pwBad = true;
				return false;
			}
		}
		else
		{
			BYTE need;

			if( m->nblocks <= 1 )
			{
				need = m->nblocks
				     ? (BYTE)(m_blk[m->firstblock].crc >> 24) : 0;
				if( dc.ZipVerifier()[11] != need )
				{
					sprintf( why, "'%s': incorrect password "
					         "(ZipCrypto verifier)", m->name );
					EggSetErr( m_err, "%s", why );
					sink->fail( sink->ctx, idx, why );
					m_pwBad = true;
					return false;
				}
			}
			else
			{
				// The verifier of a multi-block member is the high byte of
				// the WHOLE member CRC, so it must be decoded once without
				// writing anything.
				if( !DecodeMemberBody( idx, &dc, NULL, &mcrc, why ) )
				{
					char what[256 + EGG_NAME_MAX];

					sprintf( what, "'%s': %s", m->name, why );
					EggSetErr( m_err, "%s", what );
					sink->fail( sink->ctx, idx, what );
					m_pwBad = true;
					return false;
				}
				if( (BYTE)(mcrc >> 24) != dc.ZipVerifier()[11] )
				{
					sprintf( why, "'%s': incorrect password "
					         "(ZipCrypto verifier)", m->name );
					EggSetErr( m_err, "%s", why );
					sink->fail( sink->ctx, idx, why );
					m_pwBad = true;
					return false;
				}
				if( !dc.Begin( m->crypt, m->cryptInfo, m->cryptInfoLen,
				               m_password, why ) )
				{
					EggSetErr( m_err, "%s", why );
					sink->fail( sink->ctx, idx, why );
					return false;
				}
			}
		}
	}

	r = sink->open( sink->ctx, idx, m );
	if( r < 0 )
	{
		EggSetErr( m_err, "could not create '%s'", m->name );
		return false;
	}
	if( r == 0 )
		return true;

	if( !DecodeMemberBody( idx, encrypted ? &dc : NULL, sink, &mcrc, why ) )
	{
		char what[256 + EGG_NAME_MAX];

		sprintf( what, "'%s': %s", m->name, why );
		EggSetErr( m_err, "%s", what );
		sink->fail( sink->ctx, idx, what );
		return false;
	}
	sink->close( sink->ctx, idx );
	return true;
}
