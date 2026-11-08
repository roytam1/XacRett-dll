#ifndef AFX_UHA_DECODE_H__UHA_DECODE_INCLUDED__
#define AFX_UHA_DECODE_H__UHA_DECODE_INCLUDED__

#include <stdio.h>

/* UHARC container. Written from FORMAT.md sections 1-6.
 * One provider covers all four versions (like BSA/BSN); the header,
 * table and directory layouts are version-specific but share the
 * header cipher, table ciphers, backward size deltas and the solid
 * stream + per-file CRC model.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define UHA_MAX_NAME	1024

/* caps: refuse loud rather than allocate huge (house pattern) */
#define UHA_MAX_FILES	(1UL << 20)
#define UHA_MAX_MEMBER	(512UL * 1024UL * 1024UL)
#define UHA_MAX_ARCHIVE	(512UL * 1024UL * 1024UL)

typedef struct UhaEntry
{
	char dir[UHA_MAX_NAME];	/* directory string, "" or "sub\\" */
	char name[UHA_MAX_NAME];	/* file name (no path) */
	char path[UHA_MAX_NAME];	/* dir + name */
	unsigned char method;	/* entry+2 (0xFF skip marker on 0.6) */
	unsigned long size;		/* unpacked size (reconstructed) */
	unsigned long crc;		/* stored file CRC32 */
	unsigned date;			/* DOS date */
	unsigned time;			/* DOS time */
	unsigned long offset;		/* cumulative unpacked offset */
} UhaEntry;

typedef struct UhaArchive
{
	int ver;			/* 2, 3, 4 or 6 */
	int hdr_size;		/* 48 / 44 / 44 / 39 */
	int method;			/* header solid-stream method */
	unsigned long total_unpacked;
	unsigned long packed_len;	/* data_off / base_e01 / data_len */
	int exe_filter;		/* post-filter switch (gating) */
	int mm_filter;		/* multimedia filter flag (not yet applied) */
	int pw_tables;		/* -pw: tables encrypted with password */
	int pw_stream;		/* -pw: packed stream encrypted */
	UhaEntry* entries;
	unsigned long nfiles;
} UhaArchive;

/* Parse header + directory without extracting. pwd may be NULL (no
 * command-line password). Returns 0 ok, 1 io, 2 nomem, 3 magic,
 * 4 version, 5 crc, 6 method, 7 data, 8 password required.
 */
int UhaOpen(const char* path, const char* pwd, UhaArchive* arc);

void UhaClose(UhaArchive* arc);

/* Extract the solid stream and split it at cumulative file ends.
 * outpath is the full path the caller made via kiutil::pathMake
 * ("" entries are already filtered); with_paths=1 writes dir/name,
 * 0 writes the flat name. Returns 0 ok, 5 crc mismatch, 1 io, 7 data.
 * CRC is over the post-filter bytes (filter applied before verify).
 */
int UhaExtractAll(UhaArchive* arc, const char* path, unsigned long data_off,
	int with_paths);

#ifdef __cplusplus
}
#endif

#endif
