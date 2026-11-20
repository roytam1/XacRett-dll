#ifndef AFX_IS3DECODE_H__IS3_ENGINE_INCLUDED__
#define AFX_IS3DECODE_H__IS3_ENGINE_INCLUDED__

#include "Tool.h"

// InstallShield 3 archive reader ("Z" container, ICOMP 3.00.062).
//
// The format was recovered from the reference unpacker in the i3comp/
// project of the workarea root and confirmed against archives produced by
// the original ICOMP.EXE 3.00.062.  The code here is an independent
// implementation; no reference source is vendored (the reference is GPLv3).
//
// Container (all multibyte fields little endian):
//
//   +0   4   magic 13 5D 65 8C  (LE 0x8C655D13)
//   +4   1   header size, 0x3A
//   +5  58   header:
//     +0  2 to1, +2 2 to2, +4 2 pad, +6 1 has_password, +7 2 flags,
//     +9  2 num_files, +11 4 file_date, +15 4 original_archive_size,
//     +19 4 total_original_size, +23 4 taken, +27 1 total_parts,
//     +28 1 part_id, +29 1 check_byte, +30 4 end_integral_data,
//     +34 4 start_integral_data, +38 4 dirs_list_offset,
//     +42 4 dirs_list_size, +46 2 num_dirs, +48 4 files_list_offset,
//     +52 4 files_list_size, +56 4 password
//
// The file data lives in [taken, dirs_list_offset).  The directory list is
// num_dirs entries of: 2 num_files, 2 entry_size, 2 name_size, name_size+1
// bytes of NUL terminated name, 4 unused (entry_size = 11 + name_size).
// The file list is num_files entries of:
//   1 end_part_id, 2 dir_id, 4 original_size, 4 compacted_size,
//   4 data_offset, 4 file_date, 2 attribs, 2 entry_size, 1 flags,
//   1 pad, 1 start_part_id, 1 name_size, name_size+1 bytes of name,
//   4 versionMS, 4 versionLS, 4 unused (entry_size = 43 + name_size).
//
// A member's payload is PKWARE DCL implode (ctype in {0,1}, dict 4..6),
// stored verbatim when file flag 0x10 is set.  A split archive has the
// global flag 0x1 (or 0x2 for contiguous splits): its parts are named
// <base>.1, <base>.2, ... and a file with flag 0x100 can span parts.
//
// Archive flags: 0x1 split, 0x2 split contiguously.
// File flags:    0x10 stored, 0x40 has version, 0x100 spans parts.

#define IS3_MAX_FILES	4096
#define IS3_MAX_MEMBER	(256UL*1024*1024)
#define IS3_MAX_COMP	(256UL*1024*1024)

struct Is3Dir
{
	char	name[MAX_PATH];
};

struct Is3File
{
	char	path[MAX_PATH];	// member path, directories joined with '\'
	DWORD	orig;			// original size
	DWORD	comp;			// compacted size
	DWORD	ofs;			// data offset in the part
	WORD	date, time;		// DOS date/time of the member
	BYTE	attrib;			// DOS attributes
	WORD	flags;			// 0x10 stored, 0x100 spans parts
	BYTE	start_part, end_part;
};

struct Is3Archive
{
	FILE*	fp;				// the currently open part
	char	self_path[MAX_PATH*2];	// archive path as passed in
	char	base[MAX_PATH*2];	// archive path without extension
	WORD	flags;			// archive flags (0x1 / 0x2 split)
	BYTE	part_id, total_parts;
	DWORD	start_integral_data, end_integral_data;
	DWORD	dirs_list_offset, dirs_list_size;
	bool	encrypted;
	int		nfile;
	Is3File*	files;
};

// Parse the archive and build the member table.  No data is decoded.
bool Is3Open( const char* fname, Is3Archive* arc );
void Is3Close( Is3Archive* arc );

// Decode one member (index in arc->files) and write it to outpath.  The
// original size is checked; a checksum mismatch or short output fails.
bool Is3Extract( Is3Archive* arc, int index, const char* outpath );

#endif
