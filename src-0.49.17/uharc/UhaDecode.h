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
	int mm_filter;		/* multimedia filter flag (BMP/WAV/VOC) */
	int fax_filter;		/* fax transpose flag (0.3/0.6, 513216-byte files) */
	unsigned char flags2;	/* header +9 for the core-4 kennedy case (bit6) */
	int pw_tables;		/* -pw: tables encrypted with password */
	int pw_stream;		/* -pw: packed stream encrypted */
	UhaEntry* entries;
	unsigned long nfiles;
	/* the whole file image (owned; the packed stream lives in it) */
	unsigned char* img;
	unsigned long imgsize;
} UhaArchive;

/* Parse header + directory without extracting. pwd may be NULL (no
 * command-line password). Returns 0 ok, 1 io, 2 nomem, 3 magic,
 * 4 version, 5 crc, 6 method, 7 data, 8 password required.
 */
int UhaOpen(const char* path, const char* pwd, UhaArchive* arc);

void UhaClose(UhaArchive* arc);

/* Decode the whole solid stream into one buffer (caller frees).
 * packed holds packed_len cipher-decrypted bytes; cum is the
 * cumulative file-end table (NULL = single block). Returns 0 ok,
 * 6 unknown core, 7 data error, 2 nomem.
 */
int UhaDecodeSolid(UhaArchive* arc, const unsigned char* packed,
	unsigned char** out, unsigned long* outlen);

/* Apply the whole-file EXE post-filter (FORMAT 6.4) when the archive
 * gates it on and the file is at least 0x400 bytes. 0.2: MZ/ZM + len
 * and header method != 0 (STORE bypasses the filter), simpler
 * E8/E9 pass. 0.3/0.4/0.6: header
 * flag + len (MTF table + MZ relocation rebuild). Buffer is the
 * file bytes (size bytes); filter modifies in place.
 */
void UhaExeFilter(UhaArchive* arc, unsigned char* buf, unsigned long len);

/* Multimedia (BMP/WAV/VOC) inverse filter (FORMAT 2.4/6.5): the
 * delta + sign-fold transform applied to whole media files before
 * compression. Only for 0.3/0.4/0.6 with header +9 bit1 set; the
 * 0.2 decoder must not call it (0.2's in-stream predictor lives in
 * core3). In place; untouched below 16 bytes or for non-media data. */
void UhaMmFilter(unsigned char* buf, unsigned long len);

/* Fax transpose inverse (1728x2376 bit-plane rebuild, exactly
 * 513216 bytes). Selected by header +9 bit7 (0.3/0.6); disables
 * the other filters for that file. In place. */
void UhaFaxFilter(unsigned char* buf, unsigned long len);

/* The full per-file post-filter chain, decode order fax -> EXE ->
 * mm (FORMAT 2.4). Assumes the caller already gated on
 * size >= 0x400, like the original's buffering splitter. */
void UhaApplyFilters(UhaArchive* arc, unsigned char* buf,
	unsigned long len);

/* CRC32 (zlib) of a decoded member; used by the provider for the
 * per-file verify (post-filter bytes). */
unsigned long UhaFileCrc(const unsigned char* d, unsigned long n);

#ifdef __cplusplus
}
#endif

#endif
