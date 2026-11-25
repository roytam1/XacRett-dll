#ifndef AFX_QUANTUMCORE_H__QUANTUM_ENGINE_INCLUDED__
#define AFX_QUANTUMCORE_H__QUANTUM_ENGINE_INCLUDED__

// The codec that is common to two independent formats:
//
//   - the .Q archives of Cinematronics / Dynamix (PAQ.EXE / UNPAQ.EXE),
//     where the compressed data is one continuous stream for the whole
//     archive, with a 16 bit CRC behind every member;
//   - the Quantum method of the Microsoft Cabinet format, where the
//     stream is cut into 32 KB frames, each with its own coder header and
//     a 0xFF padded tail.
//
// Both use the same carry-less ("Subbotin") adaptive binary arithmetic
// coder, the same frequency model, the same length and position tables
// and the same token meanings.  Only the framing and the model sizes
// differ, so those stay in the two engines; everything shared lives here.
//
// The models are ascending arrays of (symbol, cumulative frequency)
// pairs.  A model with "entries" symbols always has entries+1 slots: the
// last one has a zero frequency and closes the range.  The initial total
// "total" is normally the number of symbols, but the .Q engine uses a
// smaller total for the distance models when the window is small.

#define QTM_ENTRIES_MAX	43

typedef struct
{
	unsigned short	sym;
	unsigned short	cumfreq;
} QtmSym;

typedef struct
{
	int		shiftsLeft;
	int		entries;
	QtmSym*	syms;
} QtmModel;

typedef struct
{
	unsigned short	H, L, C;
} QtmArith;

// A bit source: 0 or 1, or -1 when the input has run out.
typedef int (*QtmGetBit)( void* ctx );

extern const unsigned char	QtmLengthBase[27];
extern const unsigned char	QtmLengthExtra[27];
extern const unsigned int	QtmPositionBase[42];
extern const unsigned char	QtmPositionExtra[42];

void QtmInitModel( QtmModel* m, QtmSym* syms, int start, int entries, int total );
bool QtmArithInit( QtmArith* a, QtmGetBit getbit, void* ctx );

// Decode one symbol; false only when the bit source runs out.
bool QtmDecodeSymbol( QtmModel* m, QtmArith* a, QtmGetBit getbit,
					  void* ctx, int* sym );

#endif
