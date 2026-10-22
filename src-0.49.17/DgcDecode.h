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
// An encrypted archive (the -p / -k switches) has its DGCC records
// encrypted: the 32 byte chunk header and the payload of every one of
// them.  The outer chunk headers are in the clear, so the tree can
// still be walked; only the records have to be decrypted one by one.

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
//
// The password is the one of an encrypted archive (0 when there is
// none).  If the archive is encrypted and the password is missing or
// wrong, the function fails and arc->encrypted is set.
bool DgcOpen( const char* path, DgcArchive *arc, const char* password );

// The password of an encrypted archive as the command line gives it:
// -p<password> is the password itself and -p#<file> is a key file,
// whose content is hashed with SHA-512 and whose hex digest is the
// password (the same thing the DGC Archiver's -k switch does).
std::string DgcPasswordFromArg( const char* arg );

// Give back the memory.
void DgcClose( DgcArchive *arc );

// Extract one entry.  The name is the one of the entry, so the caller
// decides where the file goes.
bool DgcExtractFile( DgcArchive *arc, int index, const char* outpath );

// The recovery data: a .dgcr (an archive with the recovery data
// appended) and a .dgdr (a part of a spanning file with it appended)
// carry a DGCR chunk.  The first function tells whether the file has
// one and the second restores the words that were damaged with the
// parity table.  The number of restored words is returned.
bool DgcFindRecovery( unsigned char* img, unsigned long size );
unsigned long DgcRepairRecovery( unsigned char* img, unsigned long size );

#endif
