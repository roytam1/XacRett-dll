#ifndef AFX_QUANTUMDECODE_H__QUANTUM_ENGINE_INCLUDED__
#define AFX_QUANTUMDECODE_H__QUANTUM_ENGINE_INCLUDED__

#include "Tool.h"

// A reader for the Quantum archives of Cinematronics / Dynamix (Quantum
// 0.90 and 0.96, 1993-1995), which are produced by PAQ.EXE and read by
// UNPAQ.EXE.
//
// The format was recovered from the disassembly of the two original
// programs; the reverse engineering notes are in the paq/ project of the
// workarea root.  This is an independent implementation of that
// description.
//
// The container:
//
//   header (8 bytes)
//     0  2  the magic 0x5344 ('D', 'S')
//     2  1  the version major
//     3  1  the version minor (0x5A = 0.90, 0x60 = 0.96)
//     4  2  the number of files (little endian)
//     6  1  the window size exponent (the window is 1 << this)
//     7  1  the compression level (masked with 7)
//   the file table, for every file
//     name    a length prefixed string
//     comment a length prefixed string
//     size    4 bytes little endian (the original size in bytes)
//     time    2 bytes, the DOS time
//     date    2 bytes, the DOS date
//   the compressed data of all the files, one continuous MSB first bit
//   stream, with the 16 bit CRC of every file behind its data
//
// The length prefix of a string is one byte; when its bit 7 is set the
// length is ((len & 0x7f) << 8) | the next byte.  Only the compressed data
// is a bit stream: the file count, the sizes, the timestamps and the names
// are ordinary byte fields.
//
// The compressed data is an LZ77 with an adaptive binary arithmetic coder
// (the carry less "Subbotin" variant with the 16 bit low/high/code
// registers).  The coder is initialised once for the whole archive, so the
// models and the coder state carry over from one file to the next: the
// files have to be decoded in their order.
//
// Every file is decoded as
//
//   1. the arithmetic coded symbols,
//   2. the raw bits of the matches (the length and the distance extras),
//   3. a 16 bit CRC of the uncompressed bytes of that file.

// The maximum number of members a Quantum archive may hold.
#define QUANTUM_MAX_FILES	4096
// The maximum length of a stored name or comment.
#define QUANTUM_MAX_NAME	512
// The largest model has 64 symbols.
#define QUANTUM_MAXSYM		64

struct QuantumFile
{
	char	name[QUANTUM_MAX_NAME];		// the stored path, normalized
	char	comment[QUANTUM_MAX_NAME];	// the stored comment
	DWORD	size;						// the original size
	WORD	date, time;				// the DOS date and time
	int		badname;					// the stored path was not usable
};

// One adaptive model: a decreasing array of cumulative counts and the
// mapping from the rank to the symbol.
struct QuantumModel
{
	int				n;						// the number of the symbols
	unsigned short	cum[QUANTUM_MAXSYM+1];	// the cumulative counts
	unsigned char	sym[QUANTUM_MAXSYM];	// the symbol of every rank
	unsigned char	inv[256];				// the rank of every symbol
	int				counter;				// to the next full rescale
};

// The bit reader.  When the input is exhausted it serves zero bits and
// counts them, so that a truncated archive is detected instead of being
// decoded into an endless output.
struct QuantumBitIn
{
	FILE*			fp;
	unsigned char	buf[8192];
	int				len;		// the bytes in the buffer
	int				idx;		// the current byte in the buffer
	int				left;		// the unread bits in the current byte
	unsigned long	nbits;		// the bits read so far
	unsigned long	overrun;	// the padding bytes served past the end
};

// The decoder state.  It is kept over the whole archive, because the
// models and the coder state carry over from one file to the next.
struct QuantumDec
{
	QuantumBitIn	in;
	unsigned		low, high, code;
	QuantumModel	token;			// the 7 token symbols
	QuantumModel	literal[4];		// one literal model per group of 64
	QuantumModel	ctx;				// the 27 length classes
	QuantumModel	dist_full;		// the distance classes of the window
	QuantumModel	dist_4k;			// the distance classes below 4 K
	QuantumModel	dist_256k;		// the distance classes below 256 K
	unsigned char*	win;			// the sliding window
	unsigned long	winpos;
	unsigned long	winsize;
};

struct QuantumArchive
{
	QuantumDec*		dec;
	int				nfile;
	int				level;
	int				windowbits;
	unsigned long	window;
	unsigned long	dataoff;		// the file offset of the compressed data
	int				nextmember;	// the member to decode next
	QuantumFile*	files;
};

// Open the archive and read its file table.  Nothing is decoded yet.
bool QuantumOpen( const char* fname, QuantumArchive* arc );

// Give back the memory.
void QuantumClose( QuantumArchive* arc );

// Decode the member with the given index and write it to outpath.  The
// members have to be decoded in their order: the models and the coder state
// carry over from one to the next.  When outpath is NULL the member is
// decoded but nothing is written, which is what a member that is not
// wanted needs (the decoder state still has to advance).
bool QuantumDecodeMember( QuantumArchive* arc, int index, const char* outpath );

#endif
