#ifndef AFX_SEVENDECODE_H__6F1B83C4_D207_4E59_A8C3_1B7F5D92E604__INCLUDED_
#define AFX_SEVENDECODE_H__6F1B83C4_D207_4E59_A8C3_1B7F5D92E604__INCLUDED_

// Clean-room .7z container parser + coder-graph decoder.
//
// The container (signature header, VLQ numbers, PackInfo / UnPackInfo /
// SubStreamsInfo / FilesInfo, folder coder graphs) is parsed here; the actual
// algorithms come from the vendored cores (lzma/ and sevenz/, reached through
// the SevenZCore shim because the two 7zTypes.h copies share an include
// guard). Same split as XzDecode / LzmaDecode.

#include <vector>
#include <string>

#ifndef FNAME_MAX32
#define FNAME_MAX32 512
#endif

// A decoded folder is materialised in one contiguous buffer, so a solid
// block has to fit here. Refused loudly above this (see AGENTS.md).
#define SZ_MAX_FOLDER_OUT  0x04000000UL   // 64 MB
#define SZ_MAX_HEADER_SIZE 0x01000000UL   // 16 MB

struct CSevenZEntry
{
	char	szName[FNAME_MAX32 + 1];
	DWORD	dwSize;			// original size
	DWORD	dwPackSize;		// packed size of the containing folder
	DWORD	dwCRC;			// 0 when not stored
	bool	bCRCKnown;
	bool	bDir;
	DWORD	dwAttrib;
	bool	bHasTime;
	unsigned __int64 ftTime;	// FILETIME
	DWORD	nFolder;			// -1 for entries with no data stream
	DWORD	dwOffset;		// byte offset inside the folder output
	const char* szMode;
};

// Bounds-checked cursor over the header bytes. Lives at file scope because
// the free helper functions in SevenZDecode.cpp need it too.
struct SzRdr
{
	const BYTE* b;
	DWORD p, end;
	bool err;
};

class CSevenZ
{
public:
	CSevenZ();
	~CSevenZ();

	// Parses the whole header. `password` may be NULL for non-encrypted
	// archives; it is only consulted when a folder uses 7zAES.
	bool Open( const char* fname, const char* password );
	void Close();

	DWORD Count() const				{ return (DWORD)m_entries.size(); }
	const CSevenZEntry* Get( DWORD i ) const
		{ return i < m_entries.size() ? &m_entries[i] : NULL; }
	const char* Error() const		{ return m_err; }

	// True when any folder uses 7zAES, i.e. decoding needs a password.
	// Callers use this to avoid popping the modal dialog pointlessly.
	bool NeedsPassword() const;
	void SetPassword( const char* pw );

	// Decodes the folder holding entry `i` (cached, so solid archives only
	// pay for each block once) and writes that entry to `path`, verifying
	// the CRC when the archive stores one.
	bool Extract( DWORD i, const char* path );

private:
	struct SzCoder
	{
		DWORD id;
		DWORD nin, nout;
		DWORD inBase, outBase;
		DWORD propsSize;
		BYTE  props[32];
	};
	struct SzFolder
	{
		SzCoder coders[16];
		DWORD numCoders;
		DWORD bindIn[16], bindOut[16];
		DWORD numBindPairs;
		DWORD packedIdx[16];		// in-stream index per packed slot
		DWORD numPacked;
		DWORD packSlot[16];		// in-stream index -> pack stream ordinal
		DWORD tout, tin;
		DWORD outSize[16];		// as stored in UnPackInfo
		DWORD outData[16];		// real payload length
		DWORD outCap[16];		// buffer bytes this out-stream needs
		DWORD unpackSize;		// real folder output size
		DWORD crc;
		bool crcKnown;
		DWORD firstPack;			// pack stream ordinal of slot 0
		DWORD packSize;			// total packed bytes in this folder
		DWORD numPackStreams;
		char szMode[8];
	};

	struct SzSub
	{
		DWORD size;
		DWORD crc;
		bool  crcKnown;
		DWORD folder;
		DWORD offset;
	};

	// Picks a reporting mode from a folder's coder list (no decoding).
	static void szFolderMode( const SzFolder* fo, char* out );

	void releaseStreams();

	// True when coder `oi` belongs to coder id `id`.
	static bool szIsCoder( DWORD id );
	bool parseHeader( const BYTE* buf, DWORD len, int depth );
	bool readStreamsInfo( SzRdr* r );
	bool readPackInfo( SzRdr* r );
	bool readUnpackInfo( SzRdr* r );
	bool readFolder( SzRdr* r, SzFolder* fo );
	bool readSubStreamsInfo( SzRdr* r );
	bool readFilesInfo( SzRdr* r );
	bool readDigests( SzRdr* r, DWORD n, DWORD* out, bool* known );

	bool decodeFolder( DWORD fi );
	// dstLen is the CAPACITY of dst, not the expected output size.
	bool decodeOut( DWORD fi, DWORD oi, BYTE* dst, DWORD dstCap,
	                 DWORD* produced );
	// `produced` reports how many bytes the coder really wrote. It equals
	// dstLen for every coder except 7zAES, whose recorded outSize does not
	// match its plaintext length (the payload is always packLength-16).
	bool runCoder( SzFolder* fo, const SzCoder* c,
	               BYTE** src, DWORD* srcLen, BYTE* dst, DWORD dstLen,
	               DWORD* produced );
	bool readPackStream( DWORD ordinal, BYTE** buf, DWORD* len );

	// The next header is itself a 1-folder 7z archive; decode it into `out`.
	bool decodeEncodedHeader( const BYTE* buf, DWORD len, BYTE** out, DWORD* outLen );

	FILE*	m_fp;
	DWORD	m_fileSize;
	DWORD	m_packPos;			// file offset of packed area (== 32 + PackPos)
	DWORD	m_numPack;
	DWORD*	m_packSizes;
	DWORD*	m_packCRC;
	bool*	m_packCRCKnown;

	SzFolder* m_folders;
	DWORD	m_numFolders;

	std::vector<SzSub> m_subs;
	std::vector<CSevenZEntry> m_entries;

	// 1-entry decode cache: files are normally listed in folder order, so a
	// solid block is decoded once even when it holds many members.
	DWORD	m_cacheFolder;
	BYTE*	m_cache;
	DWORD	m_cacheLen;

	char*	m_password;
	char	m_err[128];

	void setErr( const char* e );
};

#endif