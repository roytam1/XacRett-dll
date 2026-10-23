#ifndef AFX_EGGDECODE_H__EGG_ENGINE_INCLUDED__
#define AFX_EGGDECODE_H__EGG_ENGINE_INCLUDED__

#include "Tool.h"
#include <stdio.h>

// The shared EGG container walker, this tree's rewrite of the reference
// implementation in alzgg/src/egg.c (the reverse engineering recorded in
// alzgg/FORMAT-EGG.md).
//
// Layout (every multibyte field is little-endian):
//
//   EGGA header (14 bytes): the id, the version, the header id
//   the prefix section: extra-field sub-headers until the END marker, or
//     until the first FILE header (an empty prefix).  Split Info names the
//     previous and the next volume, Solid Info marks a solid archive.
//   members: a FILE header (the file id, the size), then sub-headers
//     (Filename, Comment, Windows File Info, Encrypt Info, POSIX info),
//     then zero or more BLOCKs; sub-headers may resume between them, and
//     the sub-header END marker closes them.
//   the final END marker must be the last thing in the stream.
//
// A BLOCK is an 18-byte header (method, hint, usize, csize, crc32) followed
// by a four-byte END marker and the payload.
//
// Volumes: the .egg files of the archive's directory are indexed by header
// id, the chain is walked from the head through the next pointers, and only
// the first volume keeps its header and its prefix.
//
// Solid archives: the block payloads of every member form ONE continuous
// stream in archive order, decoded once and then spread over the members
// (the size of each slice is known from its block header).  Directories and
// empty files are materialised at their position while the stream decodes.

#define EGG_NAME_MAX  1024
#define EGG_MAXVOLS   4096
#define EGG_HDRSZ     14              // the fixed part of the EGGA header
#define EGG_BLK_HDRSZ 18

// the sub-header signatures
#define EGG_SIG_ARCHIVE   0x41474745   // "EGGA"
#define EGG_SIG_SPLIT     0x24F5A262
#define EGG_SIG_SOLID     0x24E5A060
#define EGG_SIG_GENCRYPT  0x08D144A8
#define EGG_SIG_FILE      0x0A8590E3
#define EGG_SIG_FILENAME  0x0A8591AC
#define EGG_SIG_COMMENT   0x04C63672
#define EGG_SIG_WININFO   0x2C86950B
#define EGG_SIG_POSIXINFO 0x1EE922E5
#define EGG_SIG_ENCRYPT   0x08D1470F
#define EGG_SIG_BLOCK     0x02B50C13
#define EGG_SIG_DUMMY     0x07463307
#define EGG_SIG_SKIP      0xFFFF0000
#define EGG_SIG_END       0x08E28222

// the block methods
#define EGG_M_STORE   0
#define EGG_M_DEFLATE 1
#define EGG_M_BZIP2   2
#define EGG_M_AZO     3
#define EGG_M_LZMA    4

// the encryption methods (the first byte of the Encrypt Info)
#define EGG_CRYPT_NONE   (-1)
#define EGG_CRYPT_ZIP    0
#define EGG_CRYPT_AES128 1
#define EGG_CRYPT_AES256 2
#define EGG_CRYPT_LEA128 5
#define EGG_CRYPT_LEA256 6

// the Windows File Info attribute bits (EGG's own layout, not FILE_ATTRIBUTE_*)
#define EGG_ATTR_RO 0x01
#define EGG_ATTR_H  0x02
#define EGG_ATTR_S  0x04
#define EGG_ATTR_L  0x08               // symbolic link
#define EGG_ATTR_D  0x80               // directory

struct EggBlock
{
	BYTE  method;
	BYTE  hint;
	DWORD usize;
	DWORD csize;
	DWORD crc;              // CRC-32 of the UNcompressed block bytes
	DWORD data;             // offset of the payload inside the logical stream
};

struct EggMember
{
	char  name[EGG_NAME_MAX];
	int   crypt;            // EGG_CRYPT_NONE, or an EGG_CRYPT_* value
	BYTE  attrs;            // the Windows File Info byte
	BOOL  hasWinInfo;
	unsigned __int64 filetime;   // 100 nsec from 1601.1.1, when hasWinInfo
	unsigned __int64 usize;
	DWORD fileId;
	int   nblocks;
	int   firstblock;       // index into the archive's block table

	void Zero()
	{
		name[0] = '\0';
		crypt = EGG_CRYPT_NONE;
		attrs = 0;
		hasWinInfo = FALSE;
		filetime = 0;
		usize = 0;
		fileId = 0;
		nblocks = 0;
		firstblock = 0;
	}
	bool IsDir() const
	{
		return hasWinInfo != FALSE && (attrs & EGG_ATTR_D) != 0;
	}
};

// A push callback: 1 on success, -1 on failure (never 0).
typedef int (*EggPushFn)( void* ctx, const BYTE* d, DWORD n );

// The destination of the block decoders: C callbacks, so the engine stays
// independent of the provider.  open() creates the member (a directory or a
// file) and answers 1 = write its bytes, 0 = nothing to write (a directory
// or a skipped member), -1 = could not create.  write() answers 1 or -1.
// close() is called when all of a member's bytes have been handed over.
// fail() is called once per failed member, so the caller can delete the
// partial file.
struct EggSink
{
	void* ctx;
	int  (*open)( void* ctx, int index, const EggMember* m );
	int  (*write)( void* ctx, const BYTE* d, DWORD n );
	void (*close)( void* ctx, int index );
	void (*fail)( void* ctx, int index, const char* msg );
};

// The output of the LZMA decoder: either a buffer (dst != NULL, left is its
// capacity) or a push sink (push + sctx).  bad is set when a write fails.
struct EggLzSink
{
	EggPushFn push;
	void*     sctx;
	BYTE*     dst;
	DWORD     left;
	bool      bad;
};

class EggArchive
{
public:
	EggArchive();
	~EggArchive();

	bool Open( const char* main );      // gathers, reads and parses
	void Close();

	bool Valid() const                  // true whenever the stream is open
		{ return m_buf != NULL; }
	int  Count() const                  // the parsed member count
		{ return m_count; }
	bool IsSolid() const
		{ return m_solid != 0; }
	bool Member( int i, EggMember* m ) const;
	// the first block's method (0xFF when there is no block) and the
	// sum of the block compressed sizes, for the listing
	BYTE MemberMethod( int i ) const;
	unsigned __int64 MemberCsize( int i ) const;
	const char* Error() const           // "" when clean
		{ return m_err; }

	bool Extract( EggSink* sink );      // everything, in archive order

private:
	struct EggVol
	{
		char  path[1024];
		DWORD id;
		DWORD prev;
		DWORD next;
		DWORD payload;
		int   solid;
		int   score;      // name affinity with the requested volume
	};

	bool CollectVolumes( const char* main );
	bool BuildLogical();
	bool Parse();
	bool ParseVolume( const char* path, DWORD* idOut, DWORD* prevOut,
	                  DWORD* nextOut, DWORD* payloadOut, int* solidOut,
	                  char* volpath ) const;
	bool ParseMember( DWORD off, DWORD* retPos );
	bool ParseBlocks( DWORD* pos, int* firstIdx, int* nIdx );
	bool NewMember( int* idx );
	bool NewBlock( int* idx );
	bool CheckUsize( const EggMember* m, char* why ) const;
	bool DecodeMember( int idx, EggSink* sink );
	bool DecodeSolid( const EggBlock* blocks, int nblocks,
	                  unsigned __int64 expect, BYTE* cbuf, EggSink* sink );
	bool DecodeDeflate( const BYTE* src, DWORD csize, BYTE* dst,
	                    DWORD usize, char* why );
	bool DecodeDeflatePush( const BYTE* src, DWORD csize, void* sctx,
	                        EggPushFn push, DWORD usize, char* why );
	bool DecodeBzip2( const BYTE* src, DWORD csize, BYTE* dst,
	                  DWORD usize, char* why );
	bool DecodeBzip2Push( const BYTE* src, DWORD csize, void* sctx,
	                      EggPushFn push, DWORD usize, char* why );
	bool DecodeLzma( const BYTE* src, DWORD csize, EggLzSink* o,
	                 DWORD usize, char* why );
	bool DecodeAzo( const BYTE* src, DWORD csize, BYTE* dst,
	                DWORD usize, char* why );

private:
	BYTE*  m_buf;          // the logical stream
	DWORD  m_len;
	int    m_solid;
	char   m_err[256];

	EggVol*    m_vol;      // the volume chain, in chain order
	int        m_nvol;

	EggMember* m_mem;      // the member table
	int        m_count;
	int        m_cap;
	EggBlock*  m_blk;      // every block of every member, in order
	int        m_nblk;
	int        m_blkcap;
};

#endif
