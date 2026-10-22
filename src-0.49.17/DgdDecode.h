#ifndef AFX_DGD_DECODE_H__DGD_SPANNED_FILE_INCLUDED__
#define AFX_DGD_DECODE_H__DGD_SPANNED_FILE_INCLUDED__

#include "Tool.h"

// The ".dgd" and ".dgdr" files of the "DGC Archiver" hold a file that
// is too big for a single archive, split into parts.  Every part is a
// chunk tree of its own:
//
//   DGCD  (the root)
//   |-- INFO  (116 bytes)
//   |     +0x00 u32  the flags, bit 0 = encrypted
//   |     +0x04 u32  the index of this part
//   |     +0x08 u32  the number of the parts
//   |     +0x0c u32  0
//   |     +0x10 u32  the size of this part
//   |     +0x14 u32  0
//   |     +0x18 u32  the size of the whole file
//   |     +0x1c u32  0
//   |     +0x20 64B  the SHA-512 of the whole file
//   |     +0x60 u32  the length of the name
//   |     +0x64      the name (UTF-8)
//   |-- DFDT  (32 bytes)  the date and the time of the file
//   `-- DATA  (this part's bytes, not compressed)
//
// A ".dgdr" has the same structure and a DGCR chunk (the recovery data)
// appended, and its parts are cut short to make room for it.
//
// The parts are named "<base>%04d.dgd", and the parts of two different
// files can lie in the same directory, so the parts are told apart by
// the SHA-512 of the whole file and by the number of the parts.
//
// The encryption is the one of the archives (see DgcCrypt.h), but the
// keys are derived from the material of the INFO chunk:
//   the 92 bytes of the material at INFO+4: SHA-512( 8 zeros | password )
//   the name, the DFDT and the DATA: SHA-512( the plaintext material | password )
// and each of them gets its own key stream.

#define DGD_MAX_NAME	1024
#define DGD_MAX_PART	64

struct DgdPart
{
	char			path[MAX_PATH];
	char			name[DGD_MAX_NAME];	// the name of the whole file
	unsigned long	index;		// the index of the part
	unsigned long	nparts;		// the number of the parts
	unsigned long	psize;		// the size of this part
	unsigned long	total;		// the size of the whole file
	unsigned char	sha[64];	// the SHA-512 of the whole file
	unsigned char*	data;		// this part's bytes
	unsigned long	datalen;
	int				encrypted;
	unsigned __int64 mtime;		// the FILETIME of the DFDT chunk
};

struct DgdFile
{
	char			name[DGD_MAX_NAME];	// the name of the whole file
	unsigned long	total;				// the size of the whole file
	unsigned char*	data;				// the whole file
	unsigned long	datalen;
	int				encrypted;
	int				nparts;
	unsigned __int64 mtime;			// the FILETIME of the DFDT chunk
};

// Put the parts of the file that the path belongs to together.  The
// password is the one of an encrypted file (0 when there is none).
bool DgdOpen( const char* path, const char* password, DgdFile *f );

// The information of the file that the path belongs to, without putting
// the parts together: the name, the size of the whole file, the number
// of the parts and the time stamp.  The listing uses it, so that it
// never has to read the whole file.
bool DgdInfo( const char* path, const char* password, DgdFile *f );

// Give back the memory.
void DgdClose( DgdFile *f );

#endif
