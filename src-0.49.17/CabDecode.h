#ifndef AFX_CABDECODE_H__CABINET_ENGINE_INCLUDED__
#define AFX_CABDECODE_H__CABINET_ENGINE_INCLUDED__

#include <stdio.h>
#include "Tool.h"

// A decoder for the Microsoft Cabinet format, written from the published
// format description so that no system DLL is needed.  The Cabinet
// interface was first shipped as FDI.DLL (Windows 95) and later renamed
// cabinet.dll; neither exists on the older Windows targets, so the
// official binary cannot open a cabinet there either.
//
// The container is:
//
//   CFHEADER   the fixed part, then (when the reserve flag is set) the
//              sizes of the three reserve areas and the header reserve
//              area itself, then the names of the previous and of the
//              next cabinet of the set
//   CFFOLDER   one per folder: where its data blocks start, how many
//              there are and the compression method
//   CFFILE     one per file: the name, the size, the offset inside its
//              folder, the attributes and the date and the time
//   CFDATA     one per block: the checksum, the compressed size, the
//              uncompressed size, the reserve area and the data
//
// All the multibyte fields are little-endian.
//
// The compression methods are 0 (none), 1 (MSZIP), 2 (Quantum) and
// 3 (LZX).  The none, the MSZIP and the LZX methods are done here;
// the Quantum method is refused loudly rather than silently mangled.

#define CAB_MAX_NAME	300

// the compression methods
#define CAB_COMP_NONE		0
#define CAB_COMP_MSZIP		1
#define CAB_COMP_QUANTUM	2
#define CAB_COMP_LZX		3

struct CabFolderEntry
{
	unsigned long	coffCabStart;	// where the data blocks of the folder start
	unsigned short	cCFData;		// how many data blocks the folder has
	unsigned short	typeCompress;	// the compression method and the window size
};

struct CabFileEntry
{
	char			name[CAB_MAX_NAME];
	unsigned long	size;			// the size of the file when it is unpacked
	unsigned long	offset;		// where the file starts inside its folder
	unsigned short	folder;		// the folder the file is in
	unsigned short	attribs;
	unsigned short	date;
	unsigned short	time;
};

struct CabArchive
{
	unsigned long	baseOffset;	// where the cabinet starts inside the file
	unsigned long	cbCabinet;
	unsigned long	coffFiles;	// where the file table starts
	unsigned short	cFolders;
	unsigned short	cFiles;
	unsigned short	flags;
	unsigned short	setID;
	unsigned short	iCabinet;
	unsigned short	cbCFHeader;	// the size of the header reserve area
	unsigned short	cbCFFolder;	// the size of the reserve area of a folder
	unsigned short	cbCFData;		// the size of the reserve area of a block
	char			prevCabinet[CAB_MAX_NAME];
	char			nextCabinet[CAB_MAX_NAME];
	int				nFolders;
	int				nFiles;
	CabFolderEntry*	folders;
	CabFileEntry*	files;
};

// Find the cabinet inside a file.  Something may be in front of it, as
// with a self extracting archive.  Returns the offset of the "MSCF"
// signature, or -1 when there is none.
long CabFindHeader( FILE *fp, unsigned long size );

// Read the header of the cabinet at the given offset.  The folder and
// the file tables are read as well.  Returns false when the header is
// not one of ours.
bool CabReadHeader( FILE *fp, long base, CabArchive *cab );

// Give back the memory the header reader asked for.
void CabFreeHeader( CabArchive *cab );

// The name of the compression method, for the listing.
const char *CabCompName( unsigned int typeCompress );

// Can this cabinet be unpacked by the code in here?  The Quantum
// method is not done yet, so a cabinet that uses it has to be left to
// the system.
bool CabCanDecode( const CabArchive *cab );

// The callback is called for every file of the cabinet.  It has to put
// the path the file should be written to into "path" and return true,
// or return false to skip the file.
typedef bool (*CabFileCallback)( void *user, const CabFileEntry *entry,
								 char *path, int pathSize );

// Unpack the whole cabinet.  The files are written in the order they
// are stored in, so that the data of a folder is only decoded once.
bool CabExtractAll( FILE *fp, CabArchive *cab, void *user,
					CabFileCallback callback );

#endif
