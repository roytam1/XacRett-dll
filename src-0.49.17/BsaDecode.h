#ifndef AFX_BSADECODE_H__BSA_ENGINE_INCLUDED__
#define AFX_BSADECODE_H__BSA_ENGINE_INCLUDED__

#include "Tool.h"

// A reader for the BSA and BSN archives (the game data archives that
// bsa32.exe handles).  The format was reverse engineered in the bsabsn/
// project; BsaCodec.cpp holds the three decompressors.
//
// BSN: a global header "FF 42 53 47" + a big-endian u16 flags (bit 0x0100
//      means a NUL terminated comment follows), then entries.  An entry is
//      "FF 42 53 41" + a big-endian u16 block length + that many bytes of
//      metadata block + a big-endian u32 CRC of the block, and the packed
//      data of the member follows the block.  The compression comes from
//      the entry flags: 0x4000 -> method 9 (PPM, and bit 0 makes it a
//      "text" member whose LF are written as CR LF), 0x8000 -> an
//      unsupported method, otherwise method 7 (LZ77 + Huffman).
//      An entry with no sizes and flags 0x0A00 is a directory.
//
// BSA: no global header.  An entry starts with a little-endian u16 header
//      length (0 is the terminator, and a lone trailing 00 at EOF also
//      ends the older archives), 0xAE, then a method byte: 0 is stored,
//      2/3/5/6 are the BSA200 LZSS + Huffman variants and 7 is method 7.
//      Then a little-endian u32 packed size, u32 original size, u32
//      timestamp, three u16 (the first one is a byte-sum checksum of the
//      original data, modulo 2^16, that the old tools verify), a name
//      length, the name, a NUL, and the packed data.
//      The BSA writer stores 7 for every compressed flavor, so a PPM
//      stream in a BSA file is tried as method 9 when method 7 fails
//      (the original tools cannot extract such files at all).

#define BSA_MAX_NAME	1024

// Members larger than this are refused rather than allocated.
#define BSA_MAX_MEMBER	(256UL*1024*1024)

// The methods, as they are named in the listing.
#define BSA_METHOD_STORED	0
#define BSA_METHOD_M2		2
#define BSA_METHOD_M3		3
#define BSA_METHOD_M5		5
#define BSA_METHOD_M6		6
#define BSA_METHOD_M7		7
#define BSA_METHOD_M9		9
#define BSA_METHOD_UNKNOWN	10

struct BsaEntry
{
	char			name[BSA_MAX_NAME];	// normalized, backslashes
	unsigned long	orig;		// the original size
	unsigned long	packed;		// the size in the archive
	unsigned long	offset;		// the offset of the packed data in img
	unsigned long	crc;		// BSN: the file CRC; BSA: the byte-sum
	unsigned long	ts;			// DOS date (high) and time (low)
	int				method;		// BSA_METHOD_*
	unsigned int	flag;		// the raw entry flags byte
	int				textmode;	// method 9 only: LF is written as CR LF
	int				isdir;
};

struct BsaArchive
{
	unsigned char*	img;		// the whole file
	unsigned long	imgsize;
	int				nentry;
	BsaEntry*		entries;
	int				isbsn;		// 0 = BSA, 1 = BSN
};

// Read the archive into memory and parse the directory.  The member data
// stays in the image; nothing is decompressed until BsaExtractFile.
bool BsaOpen( const char* path, BsaArchive* arc );

// Give back the memory.
void BsaClose( BsaArchive* arc );

// Extract one entry.  The caller decides the output path.
bool BsaExtractFile( BsaArchive* arc, int index, const char* outpath );

// The name of a method for the listing ("stored", "m2", "m7", "m9", ...).
const char* BsaMethodName( int method );

#endif
