#ifndef AFX_CABLZX_H__LZX_ENGINE_INCLUDED__
#define AFX_CABLZX_H__LZX_ENGINE_INCLUDED__

// The LZX compression method of the Microsoft Cabinet format, written
// from the published format description.
//
// The stream is a bit stream, read from the most significant bit down.
// It is divided into frames of 32 KB, and each frame is divided into
// blocks, and every block starts with its own type and length:
//
//   1  the trees are read and the data is compressed with them
//   2  the same, but the offsets with three or more extra bits use
//      their own little tree
//   3  the data is stored as it is
//
// The trees are read with a "pre tree", whose twenty code lengths are
// stored in four bits each, and which then says how the code lengths
// of the real tree are to be read.
//
// Match offsets are not stored as they are: a "position slot" and a
// small number of extra bits give the offset, and the three offsets
// that were used last are kept for the three first slots.
//
// The last ten bytes of a frame are not translated, so the "intel"
// translation of the x86 call instructions is done a frame at a time.

struct CabLzxDecoder;

// The decoder asks for more input when it runs out of it.  The
// callback has to put the data into "buf" and return how many bytes it
// placed; zero means the end of the input.
typedef int (*CabLzxFillFn)( void *ctx, unsigned char *buf, unsigned long want );

// Make a decoder for a stream with the given window size.  The window
// is 2 to the power of "windowBits" bytes, which is between 15 and 21.
// "totalLength" is the number of bytes the whole stream gives; zero
// means that it is not known, and then every frame is 32 KB.
CabLzxDecoder* CabLzxCreate( int windowBits, unsigned long totalLength,
							  CabLzxFillFn fill, void *fillCtx );
void CabLzxDestroy( CabLzxDecoder* d );

// Decode exactly "len" bytes into "out".  Returns false when the
// stream is damaged or ends too early.
bool CabLzxDecode( CabLzxDecoder* d, unsigned char *out, unsigned long len );

#endif
