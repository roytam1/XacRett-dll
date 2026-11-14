#ifndef AFX_QIPDECODE_H__QIP_ENGINE_INCLUDED__
#define AFX_QIPDECODE_H__QIP_ENGINE_INCLUDED__

#include "Tool.h"

// A reader for the QIP archives of the QEMM386 / QPACK installers.
//
// The format was recovered from QUNPCK32.DLL (the 32 bit unpacker library
// of the QIP support library) and from its 16 bit sibling QUNPACK.DLL; the
// reverse engineering notes are in the qunpack/ project of the workarea
// root.  This is an independent implementation of that description.
//
// Two containers exist:
//
//   "QP"  a 16 byte header, one 16 byte index entry per packed file and
//         then the chain of the "QD" blocks.  The index entry is the file
//         offset of the block and the name of the file; the blocks
//         themselves start right behind the index, at 16*(count+1).
//   "QD"  a raw archive: just the chain of the "QD" blocks, from offset 0.
//
// The header of the "QP" container is
//
//   +0   2   the magic "QP"
//   +2   2   the number of the index entries
//   +4   4   the size of the index in bytes (count * 16)
//   +8   2   the version: 1 = no checksums, 2 = a CRC-32 per file
//
// A "QD" block is a 10 byte header (version 1) or a 14 byte header
// (version 2, which adds a CRC-32 of the decompressed data):
//
//   +0   2   the magic "QD"
//   +2   2   the type: 0 = a file, 1 = a directory name
//   +4   4   the size of the compressed data
//   +8   2   the index of the file inside the archive
//   +10  4   the CRC-32 of the original data (version 2 only)
//
// A file block is followed by a 22 byte record - five archive metadata
// bytes, the original size and the NUL terminated name - and then the
// compressed data.  A directory block is followed by the NUL terminated
// directory name, and every file block after it belongs to it.
//
// The compressed data is an LZ77 with static Huffman codes.  Every symbol
// starts with a selector bit: 0 is a literal (the raw byte in mode 0, a
// Huffman code in mode 1) and 1 is a match (a length code with its extra
// bits, then a distance group code with its extra bits).  The window is
// 4096 bytes and the length code 517 ends the block.

// The maximum number of members a QIP archive may hold.
#define QIP_MAX_FILES	4096
// Members larger than this are refused rather than allocated.
#define QIP_MAX_MEMBER	(256UL*1024*1024)

struct QipFile
{
	char	path[MAX_PATH];	// the member path, with the directory
	DWORD	blockpos;		// the file offset of the "QD" block
	DWORD	csize;			// the size of the compressed data
	DWORD	orig;			// the original size
	DWORD	crc;			// the CRC-32 of the original data (0 in v1)
	WORD	flags;			// the index of the file inside the archive
};

struct QipArchive
{
	FILE*	fp;
	int		version;		// 1 (no checksums) or 2 (CRC-32 per file)
	int		count;			// the number of index entries ("QP" only)
	int		nfile;
	QipFile*	files;
};

// Open the archive and walk its chain of "QD" blocks.  Nothing is decoded
// yet; the members are described by their block.
bool QipOpen( const char* fname, QipArchive* arc );

// Give back the memory and close the file.
void QipClose( QipArchive* arc );

// Decode one member and write it to outpath.  The size and, in a version 2
// archive, the CRC-32 of the member are checked against the archive.
bool QipExtract( QipArchive* arc, int index, const char* outpath );

#endif
