#ifndef AFX_DGCDECODE_H__DGC_ENGINE_INCLUDED__
#define AFX_DGCDECODE_H__DGC_ENGINE_INCLUDED__

#include "Tool.h"

// A decoder for the DGCA archive format (the ".dgc" files of the
// "DGC Archiver" v1.10), written from the format description in
// dgca/NOTES-dgca-format.md.
//
// The container is a tree of chunks.  Every chunk starts with a 32 byte
// header (the id, the header size, the payload size, two reserved
// words, the flags, the alignment, the CRC-32 of the payload and the
// CRC-32 of the header with the last field zeroed) and the payload of
// a container is the concatenation of its children, each padded to the
// next 16 byte boundary:
//
//   DGCA
//   +- DATA    the file payloads
//   |   +- DGCC   the compressed blocks
//   +- INFO   the metadata
//       +- IARC   the archive index (80 bytes)
//       +- IFDT -> DGCC   the index table (64 bytes per entry, transposed)
//       +- IFNM -> DGCC   the name list
//
// The compressed blocks are a context mixing entropy coder, an LZ stage
// that permutes the token buffer and an LZ77 with an implicit distance,
// in that order.  The "column" method splits the data into interleaved
// columns, each of them delta transformed and compressed on its own.
//
// What is NOT implemented: the encryption (-p / -k).  The cipher is
// known, but the key material that the writer derives is not, so an
// encrypted archive is reported as such instead of being decoded into
// garbage.

#define DGC_MAX_NAME	1024

struct DgcEntry
{
	char			name[DGC_MAX_NAME];	// the name, with backslashes
	unsigned long	size;		// the original size
	unsigned long	csize;		// the size in the data area
	unsigned long	offset;		// the offset of the record in the data area
	unsigned long	crc;		// the CRC-32 of the contents
	unsigned long	attr;		// the Windows attributes
	unsigned __int64 mtime;	// the last write time
};

struct DgcArchive
{
	unsigned char*	img;		// the whole file
	unsigned long	imgsize;
	int				nentry;
	DgcEntry*		entries;
	unsigned char*	data;		// the decompressed DATA payload
	unsigned long	datasize;
	unsigned char*	names;		// the name list
	unsigned long	namesize;
	int				encrypted;
};

// Read the archive: the container, the index and the name list.  The
// data area is not decompressed yet, that happens when a file is
// extracted.
bool DgcOpen( const char* path, DgcArchive *arc );

// Give back the memory.
void DgcClose( DgcArchive *arc );

// Extract one entry.  The name is the one of the entry, so the caller
// decides where the file goes.
bool DgcExtractFile( DgcArchive *arc, int index, const char* outpath );

#endif
