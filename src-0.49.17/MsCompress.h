#ifndef AFX_MSCOMPRESS_H__INCLUDED_
#define AFX_MSCOMPRESS_H__INCLUDED_

#include <stdio.h>

// Decoders for the COMPRESS.EXE / EXPAND.EXE formats, SZDD and KWAJ.
//
// These are written from the published format description, so that no
// system DLL is needed: the LZ32 interface (lz32.dll) only implements
// the LZ/SZDD variant, is not present on the older Windows targets
// and copies unknown input through unchanged.

enum ms_compress_format
{
	MSCOMPRESS_NONE = 0,		// not one of these formats
	MSCOMPRESS_SZDD,			// the "expand" variant
	MSCOMPRESS_SZDD_QBASIC,	// the QBasic variant
	MSCOMPRESS_KWAJ			// the KWAJ variant
};

enum ms_compress_result
{
	MSCOMPRESS_OK = 0,
	MSCOMPRESS_ERR_SIGNATURE,	// not a COMPRESS.EXE file
	MSCOMPRESS_ERR_UNSUPPORTED,	// a known format we do not decode yet
	MSCOMPRESS_ERR_DATA,		// the compressed data is damaged
	MSCOMPRESS_ERR_READ,		// the input ended early
	MSCOMPRESS_ERR_WRITE		// the output could not be written
};

struct MsCompressHeader
{
	int format;			// one of ms_compress_format
	int method;			// the KWAJ compression method
	unsigned long origsize;	// the length of the file when unpacked
	unsigned long dataoffset;	// where the compressed data starts
	char missingchar;		// the character missing from the name
};

// Look at the header of an open file and work out what it is.  The
// file is left positioned at the start of the compressed data.
int MsCompressReadHeader( FILE *in, MsCompressHeader *h );

// Unpack the file.  "in" must be positioned where MsCompressReadHeader
// left it.
int MsCompressDecode( FILE *in, FILE *out, const MsCompressHeader *h );

// Work out the name of the unpacked file from the name of the
// compressed one.  COMPRESS.EXE replaced the last character of the
// name with an underscore, and the character that was there is stored
// in the header, so it is passed in as "missingchar" (0 if unknown).
// Returns the length of the name, or 0 on failure.
int MsCompressMakeName( const char *arcname, char missingchar,
						char *out, int outsz );

// The name of the compression method, for the listing.  It has to fit
// in the eight bytes that the member information has room for.
const char *MsCompressModeName( const MsCompressHeader *h );

#endif
