#ifndef AFX_CABQUANTUM_H__QUANTUM_ENGINE_INCLUDED__
#define AFX_CABQUANTUM_H__QUANTUM_ENGINE_INCLUDED__

// The Quantum compression method of the Microsoft Cabinet format,
// written from the published format description.
//
// Quantum is an arithmetic coder: a symbol is read from one of eight
// models, and the models are kept up to date as the symbols come.  A
// literal is coded with one of four models of 64 symbols each, and a
// match with one of three models, the last of which codes the length
// as well.  The match offsets are not coded as they are: a "position
// slot" and a few extra bits give the offset, like in LZX.
//
// The stream is a bit stream, read from the most significant bit of
// sixteen bit words down, and the two bytes of a word are in the file
// in the same order, so a word is read as they are.
//
// The data of a folder is a series of frames of 32 KB.  At the end of
// a frame the bits are put back in line with the bytes, and the
// padding that follows is skipped by looking for the byte 0xFF, which
// the writer puts at the end of every block.

struct CabQuantumDecoder;

// The decoder asks for more input when it runs out of it.  The
// callback has to put the data into "buf" and return how many bytes it
// placed; zero means the end of the input.
typedef int (*CabQuantumFillFn)( void *ctx, unsigned char *buf, unsigned long want );

// Make a decoder for a stream with the given window size.  The window
// is 2 to the power of "windowBits" bytes, which is between 10 and 21.
CabQuantumDecoder* CabQuantumCreate( int windowBits,
									 CabQuantumFillFn fill, void *fillCtx );
void CabQuantumDestroy( CabQuantumDecoder* d );

// Decode exactly "len" bytes into "out".  Returns false when the
// stream is damaged or ends too early.
bool CabQuantumDecode( CabQuantumDecoder* d, unsigned char *out, unsigned long len );

#endif
