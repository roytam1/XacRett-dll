#include "stdafx.h"
#include "CabLzx.h"

#define LZX_FRAME_SIZE		32768
#define LZX_NUM_CHARS		256
#define LZX_NUM_PRIMARY		7
#define LZX_NUM_SECONDARY	249
#define LZX_PRETREE_NUM		20
#define LZX_ALIGNED_NUM		8
#define LZX_MAX_BITS		16
#define LZX_INPUT_MAX		65536
#define LZX_MAX_SLOTS		290
#define LZX_MAIN_SYMBOLS	(LZX_NUM_CHARS + LZX_MAX_SLOTS*8)

// the block types
#define LZX_BLOCK_VERBATIM		1
#define LZX_BLOCK_ALIGNED		2
#define LZX_BLOCK_UNCOMPRESSED	3

// The number of position slots for each window size, from 32 KB up.
static const unsigned int lzxSlotCount[11] = {
	30, 32, 34, 36, 38, 42, 50, 66, 98, 162, 290
};

// The number of extra bits of a position slot.  The slots below four
// have none, the ones up to 36 have half their number less one, and
// the rest have seventeen.
static const unsigned char lzxExtraBits[36] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
	9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16
};

// The base of every position slot.  It is worked out from the rule
// "the base of a slot is the base of the slot before it plus one to
// the power of the extra bits of the slot before it", so that the
// table does not have to be a list of numbers.
static unsigned int lzxPositionBase[LZX_MAX_SLOTS];
static int lzxPositionBaseReady = 0;

static void lzxMakePositionBase( void )
{
	unsigned int base = 0;
	int i;

	if( lzxPositionBaseReady ) return;
	for( i=0; i<LZX_MAX_SLOTS; i++ ) {
		lzxPositionBase[i] = base;
		if( i<4 )			base += 1;
		else if( i<36 )		base += 1u << (i/2 - 1);
		else				base += 1u << 17;
	}
	lzxPositionBaseReady = 1;
}

// A Huffman tree, decoded the slow but simple way: the code lengths
// are counted, and the symbols are put in the order of their codes.
struct LzxTree
{
	unsigned short	counts[17];
	unsigned short	symbols[LZX_MAIN_SYMBOLS];
};

struct CabLzxDecoder
{
	// the input
	unsigned char	inBuf[LZX_INPUT_MAX];
	unsigned long	inLen;
	unsigned long	inPos;
	int				eof;
	CabLzxFillFn	fill;
	void*			fillCtx;
	unsigned int	bitbuf;
	int				bitcnt;
	unsigned char	pendingByte;	// a byte that waits for its partner
	int				pendingValid;

	// the window
	unsigned char*	window;
	unsigned long	windowSize;
	unsigned long	windowPosn;
	unsigned long	framePosn;
	unsigned long	frame;
	unsigned long	offset;		// the bytes that were given out
	unsigned long	totalLength;

	// the repeated offsets
	unsigned int	R0, R1, R2;

	// the block
	int				blockType;
	unsigned long	blockRemaining;
	unsigned long	blockLength;
	int				headerRead;
	int				intelFilesize;
	int				intelStarted;

	// the trees
	LzxTree			pretree;
	LzxTree			maintree;
	LzxTree			lengthtree;
	LzxTree			alignedtree;
	unsigned char	mainLens[LZX_MAIN_SYMBOLS];
	unsigned char	lengthLens[LZX_NUM_SECONDARY+1];
	unsigned char	alignedLens[LZX_ALIGNED_NUM];
	unsigned char	pretreeLens[LZX_PRETREE_NUM];
	int				lengthEmpty;
	unsigned int	numOffsets;

	// the frame that is being given out
	unsigned char	frameBuf[LZX_FRAME_SIZE];
	unsigned long	pending;
	unsigned long	pendingPos;
};

// ------------------------------------------------------------------
// The bit reader.  The bits come from the most significant end down.
// ------------------------------------------------------------------

// Make sure that there are at least "n" bits in the buffer.
//
// The bits of the stream are taken from the top of sixteen bit words,
// and the two bytes of a word are in the file in the other order: the
// first byte of the file is the low byte of the word.  Getting that
// wrong shifts the whole stream by eight bits and every block header
// then reads as a type that does not exist.
static int lzxNeedBits( CabLzxDecoder *d, int n )
{
	while( d->bitcnt < n ) {
		if( d->pendingValid ) {
			// the byte that was left over needs its partner
			if( d->inPos >= d->inLen ) {
				int got;
				if( d->eof ) return 0;
				got = d->fill( d->fillCtx, d->inBuf, LZX_INPUT_MAX );
				if( got<=0 ) {
					d->eof = 1;
					return 0;
				}
				d->inLen = (unsigned long)got;
				d->inPos = 0;
			}
			d->bitbuf = (d->bitbuf << 16) |
						( ((unsigned int)d->inBuf[d->inPos]<<8) | d->pendingByte );
			d->inPos++;
			d->pendingValid = 0;
			d->bitcnt += 16;
			continue;
		}
		if( d->inPos+1 < d->inLen ) {
			d->bitbuf = (d->bitbuf << 16) |
						( ((unsigned int)d->inBuf[d->inPos+1]<<8) | d->inBuf[d->inPos] );
			d->inPos += 2;
			d->bitcnt += 16;
			continue;
		}
		if( d->inPos < d->inLen ) {
			// only one byte is left, so it waits for its partner
			d->pendingByte = d->inBuf[d->inPos];
			d->pendingValid = 1;
			d->inPos++;
			continue;
		}
		{
			int got;
			if( d->eof ) return 0;
			got = d->fill( d->fillCtx, d->inBuf, LZX_INPUT_MAX );
			if( got<=0 ) {
				d->eof = 1;
				return 0;
			}
			d->inLen = (unsigned long)got;
			d->inPos = 0;
		}
	}
	return 1;
}

static int lzxGetBits( CabLzxDecoder *d, int n, unsigned int *out )
{
	if( n==0 ) { *out = 0; return 1; }
	if( !lzxNeedBits( d, n ) ) return 0;
	d->bitcnt -= n;
	*out = ( d->bitbuf >> d->bitcnt ) & ((1u<<n)-1);
	d->bitbuf &= (1u<<d->bitcnt)-1;
	return 1;
}

static int lzxGetBit( CabLzxDecoder *d, unsigned int *out )
{
	return lzxGetBits( d, 1, out );
}

// Throw away the bits that are left, up to the next sixteen bit
// boundary.  This is what the format asks for at the end of a frame.
static void lzxAlign( CabLzxDecoder *d )
{
	if( d->bitcnt & 15 ) {
		int drop = d->bitcnt & 15;
		d->bitcnt -= drop;
		d->bitbuf &= (1u<<d->bitcnt)-1;
	}
}

// ------------------------------------------------------------------
// The Huffman trees.
// ------------------------------------------------------------------

// Work out the decoding table of a tree from its code lengths.
static int lzxBuildTree( LzxTree *t, const unsigned char *lens, int n )
{
	unsigned short offset[17];
	int i, len, left;

	for( i=0; i<17; i++ ) t->counts[i] = 0;
	for( i=0; i<n; i++ ) {
		if( lens[i]>16 ) return 0;
		t->counts[lens[i]]++;
	}

	// the tree must not ask for more codes than the space allows
	left = 1;
	for( len=1; len<=16; len++ ) {
		left <<= 1;
		left -= t->counts[len];
		if( left<0 ) return 0;
	}
	// a tree with no codes at all is not one, but the length tree of
	// a block without any match is empty, and that is allowed
	if( left>0 && n>0 ) {
		int used = 0;
		for( i=0; i<n; i++ ) if( lens[i] ) used = 1;
		if( used ) return 0;
	}

	offset[1] = 0;
	for( len=1; len<16; len++ ) {
		offset[len+1] = (unsigned short)( offset[len] + t->counts[len] );
	}
	for( i=0; i<n; i++ ) {
		if( lens[i] ) t->symbols[offset[lens[i]]++] = (unsigned short)i;
	}
	return 1;
}

// Read one symbol from a tree.
static int lzxReadSymbol( CabLzxDecoder *d, LzxTree *t )
{
	int code = 0, first = 0, index = 0;
	int len, count;
	unsigned int bit;

	for( len=1; len<=16; len++ ) {
		if( !lzxGetBit( d, &bit ) ) return -1;
		code |= (int)bit;
		count = t->counts[len];
		if( code - count < first ) {
			return t->symbols[index + (code - first)];
		}
		index += count;
		first += count;
		first <<= 1;
		code <<= 1;
	}
	return -1;
}

// Read the code lengths of a tree.  The twenty lengths of the pre tree
// come first, and they say how the lengths of the real tree are coded.
static int lzxReadLengths( CabLzxDecoder *d, unsigned char *lens,
							unsigned int first, unsigned int last )
{
	unsigned int x, y;
	int z;

	for( x=0; x<LZX_PRETREE_NUM; x++ ) {
		if( !lzxGetBits( d, 4, &y ) ) return 0;
		d->pretreeLens[x] = (unsigned char)y;
	}
	if( !lzxBuildTree( &d->pretree, d->pretreeLens, LZX_PRETREE_NUM ) ) return 0;

	for( x=first; x<last; ) {
		z = lzxReadSymbol( d, &d->pretree );
		if( z<0 ) return 0;
		if( z==17 ) {
			// a run of zeros
			if( !lzxGetBits( d, 4, &y ) ) return 0;
			y += 4;
			while( y-- ) {
				if( x>=last ) return 0;
				lens[x++] = 0;
			}
		} else if( z==18 ) {
			// a longer run of zeros
			if( !lzxGetBits( d, 5, &y ) ) return 0;
			y += 20;
			while( y-- ) {
				if( x>=last ) return 0;
				lens[x++] = 0;
			}
		} else if( z==19 ) {
			// a run of the same length as the one before
			int w;
			if( !lzxGetBits( d, 1, &y ) ) return 0;
			y += 4;
			w = lzxReadSymbol( d, &d->pretree );
			if( w<0 ) return 0;
			z = (int)lens[x] - w;
			if( z<0 ) z += 17;
			while( y-- ) {
				if( x>=last ) return 0;
				lens[x++] = (unsigned char)z;
			}
		} else {
			// the length of the symbol before, changed by this one
			z = (int)lens[x] - z;
			if( z<0 ) z += 17;
			lens[x++] = (unsigned char)z;
		}
	}
	return 1;
}

// ------------------------------------------------------------------

CabLzxDecoder* CabLzxCreate( int windowBits, unsigned long totalLength,
							  CabLzxFillFn fill, void *fillCtx )
{
	CabLzxDecoder* d;
	int i;

	if( windowBits<15 || windowBits>21 ) return NULL;
	lzxMakePositionBase();

	d = (CabLzxDecoder*) new BYTE[sizeof(CabLzxDecoder)];
	if( !d ) return NULL;
	memset( d, 0, sizeof(*d) );

	d->windowSize = 1u << windowBits;
	d->window = new BYTE[d->windowSize];
	if( !d->window ) {
		delete [] (BYTE*)d;
		return NULL;
	}
	memset( d->window, 0, d->windowSize );

	d->fill = fill;
	d->fillCtx = fillCtx;
	d->totalLength = totalLength;
	d->numOffsets = lzxSlotCount[windowBits-15] << 3;

	// the three offsets that were used last
	d->R0 = 1;
	d->R1 = 1;
	d->R2 = 1;

	return d;
}

void CabLzxDestroy( CabLzxDecoder* d )
{
	if( !d ) return;
	if( d->window ) delete [] d->window;
	delete [] (BYTE*)d;
}

// The x86 call instructions are translated, and the translation is
// undone here.  The last ten bytes of a frame are left alone.
static void lzxIntelUntranslate( CabLzxDecoder *d, unsigned long frameSize )
{
	unsigned char* data = d->frameBuf;
	unsigned char* dataend = d->frameBuf + frameSize - 10;
	signed int curpos = (signed int)d->offset;
	signed int filesize = d->intelFilesize;
	signed int absOff, relOff;

	if( !d->intelStarted || !d->intelFilesize ) return;
	if( d->frame>=32768 || frameSize<=10 ) return;

	while( data < dataend ) {
		if( *data++ != 0xE8 ) { curpos++; continue; }
		absOff = (signed int)( (unsigned int)data[0] |
							   ((unsigned int)data[1]<<8) |
							   ((unsigned int)data[2]<<16) |
							   ((unsigned int)data[3]<<24) );
		if( (absOff>=-curpos) && (absOff<filesize) ) {
			relOff = (absOff>=0) ? absOff-curpos : absOff+filesize;
			data[0] = (unsigned char)relOff;
			data[1] = (unsigned char)(relOff>>8);
			data[2] = (unsigned char)(relOff>>16);
			data[3] = (unsigned char)(relOff>>24);
		}
		data += 4;
		curpos += 5;
	}
}

// Read the header of a block and build its trees.
static int lzxReadBlockHeader( CabLzxDecoder *d )
{
	unsigned int v;
	unsigned int blockType, blockLength;
	int i;

	// the type of the block and its length
	if( !lzxGetBits( d, 3, &blockType ) ) return 0;
	if( !lzxGetBits( d, 16, &v ) ) return 0;
	blockLength = v;
	if( !lzxGetBits( d, 8, &v ) ) return 0;
	blockLength = (blockLength<<8) | v;

	d->blockType = (int)blockType;
	d->blockLength = blockLength;
	d->blockRemaining = blockLength;

	switch( d->blockType )
	{
	case LZX_BLOCK_ALIGNED:
		// the little tree of the offsets comes first
		for( i=0; i<LZX_ALIGNED_NUM; i++ ) {
			if( !lzxGetBits( d, 3, &v ) ) return 0;
			d->alignedLens[i] = (unsigned char)v;
		}
		if( !lzxBuildTree( &d->alignedtree, d->alignedLens, LZX_ALIGNED_NUM ) ) return 0;
		// and then the main tree, like a verbatim block
		if( !lzxReadLengths( d, d->mainLens, 0, LZX_NUM_CHARS ) ) return 0;
		if( !lzxReadLengths( d, d->mainLens, LZX_NUM_CHARS, LZX_NUM_CHARS+d->numOffsets ) ) return 0;
		if( !lzxBuildTree( &d->maintree, d->mainLens, LZX_NUM_CHARS+d->numOffsets ) ) return 0;
		if( d->mainLens[0xE8] ) d->intelStarted = 1;
		if( !lzxReadLengths( d, d->lengthLens, 0, LZX_NUM_SECONDARY ) ) return 0;
		d->lengthEmpty = 0;
		if( !lzxBuildTree( &d->lengthtree, d->lengthLens, LZX_NUM_SECONDARY ) ) {
			// an empty length tree is allowed, when the block has
			// no match at all
			for( i=0; i<LZX_NUM_SECONDARY; i++ ) {
				if( d->lengthLens[i] ) return 0;
			}
			d->lengthEmpty = 1;
		}
		break;

	case LZX_BLOCK_VERBATIM:
		if( !lzxReadLengths( d, d->mainLens, 0, LZX_NUM_CHARS ) ) return 0;
		if( !lzxReadLengths( d, d->mainLens, LZX_NUM_CHARS, LZX_NUM_CHARS+d->numOffsets ) ) return 0;
		if( !lzxBuildTree( &d->maintree, d->mainLens, LZX_NUM_CHARS+d->numOffsets ) ) return 0;
		if( d->mainLens[0xE8] ) d->intelStarted = 1;
		if( !lzxReadLengths( d, d->lengthLens, 0, LZX_NUM_SECONDARY ) ) return 0;
		d->lengthEmpty = 0;
		if( !lzxBuildTree( &d->lengthtree, d->lengthLens, LZX_NUM_SECONDARY ) ) {
			for( i=0; i<LZX_NUM_SECONDARY; i++ ) {
				if( d->lengthLens[i] ) return 0;
			}
			d->lengthEmpty = 1;
		}
		break;

	case LZX_BLOCK_UNCOMPRESSED:
		// the data is stored as it is, so the bits are dropped
		d->intelStarted = 1;
		d->bitcnt = 0;
		d->bitbuf = 0;
		// and the three offsets that were used last are stored too
		for( i=0; i<12; i++ ) {
			unsigned int byte;
			if( !lzxGetBits( d, 8, &byte ) ) return 0;
			if( i==0 ) d->R0 = 0;
			if( i==4 ) d->R1 = 0;
			if( i==8 ) d->R2 = 0;
			if( i<4 ) d->R0 |= byte<<(i*8);
			else if( i<8 ) d->R1 |= byte<<((i-4)*8);
			else d->R2 |= byte<<((i-8)*8);
		}
		break;

	default:
		return 0;
	}
	return 1;
}

// Copy a match out of the window.
static int lzxCopyMatch( CabLzxDecoder *d, unsigned int offset, unsigned int length )
{
	unsigned char* window = d->window;
	unsigned long posn = d->windowPosn;
	unsigned char* dest;
	unsigned char* src;
	unsigned int i, j;

	if( (posn+length) > d->windowSize ) return 0;

	dest = &window[posn];
	i = length;
	if( offset > posn ) {
		// the match reaches back over the start of the window
		if( offset > d->windowSize ) return 0;
		j = offset - posn;
		src = &window[d->windowSize - j];
		if( j < i ) {
			// the match goes over the end of the window, so it is
			// copied in two runs
			i -= j;
			while( j-- > 0 ) *dest++ = *src++;
			src = window;
		}
		while( i-- > 0 ) *dest++ = *src++;
	} else {
		src = dest - offset;
		while( i-- > 0 ) *dest++ = *src++;
	}
	d->windowPosn = posn + length;
	return 1;
}

// Decode one frame of the stream into the frame buffer.
static int lzxDecodeFrame( CabLzxDecoder *d, unsigned long frameSize )
{
	unsigned char* window = d->window;
	unsigned long bytesTodo;

	// the header of the stream, which is only there once
	if( !d->headerRead ) {
		unsigned int bit;
		unsigned int hi, lo;
		if( !lzxGetBit( d, &bit ) ) return 0;
		if( bit ) {
			if( !lzxGetBits( d, 16, &hi ) ) return 0;
			if( !lzxGetBits( d, 16, &lo ) ) return 0;
			d->intelFilesize = (int)((hi<<16)|lo);
		} else {
			d->intelFilesize = 0;
		}
		d->headerRead = 1;
	}

	// decode until the frame is full
	bytesTodo = d->framePosn + frameSize - d->windowPosn;
	while( bytesTodo > 0 ) {
		unsigned long thisRun;

		if( d->blockRemaining==0 ) {
			// a block that was stored as it is and had an odd
			// length is followed by a byte that is dropped
			if( (d->blockType==LZX_BLOCK_UNCOMPRESSED) && (d->blockLength & 1) ) {
				unsigned int bit;
				if( !lzxGetBit( d, &bit ) ) return 0;
			}
			if( !lzxReadBlockHeader( d ) ) return 0;
		}

		thisRun = d->blockRemaining;
		if( thisRun > bytesTodo ) thisRun = bytesTodo;
		bytesTodo -= thisRun;
		d->blockRemaining -= thisRun;

		switch( d->blockType )
		{
		case LZX_BLOCK_ALIGNED:
		case LZX_BLOCK_VERBATIM:
			while( thisRun > 0 ) {
				int mainElement, lengthFooter, extra;
				int matchLength;
				unsigned int matchOffset;
				unsigned int v;

				mainElement = lzxReadSymbol( d, &d->maintree );
				if( mainElement<0 ) return 0;
				if( mainElement < LZX_NUM_CHARS ) {
					// a literal
					window[d->windowPosn++] = (unsigned char)mainElement;
					thisRun--;
					continue;
				}
				mainElement -= LZX_NUM_CHARS;

				// the length of the match
				matchLength = mainElement & LZX_NUM_PRIMARY;
				if( matchLength==LZX_NUM_PRIMARY ) {
					if( d->lengthEmpty ) return 0;
					lengthFooter = lzxReadSymbol( d, &d->lengthtree );
					if( lengthFooter<0 ) return 0;
					matchLength += lengthFooter;
				}
				matchLength += 2;

				// the offset of the match
				matchOffset = (unsigned int)( mainElement >> 3 );
				switch( matchOffset )
				{
				case 0:
					matchOffset = d->R0;
					break;
				case 1:
					matchOffset = d->R1;
					d->R1 = d->R0;
					d->R0 = matchOffset;
					break;
				case 2:
					matchOffset = d->R2;
					d->R2 = d->R0;
					d->R0 = matchOffset;
					break;
				default:
					if( matchOffset >= 36 ) extra = 17;
					else extra = lzxExtraBits[matchOffset];
					matchOffset = lzxPositionBase[matchOffset] - 2;
					if( (extra>=3) && (d->blockType==LZX_BLOCK_ALIGNED) ) {
						// the last bits of the offset come out of
						// the little tree
						if( extra > 3 ) {
							if( !lzxGetBits( d, extra-3, &v ) ) return 0;
							matchOffset += v<<3;
						}
						{
							int aligned = lzxReadSymbol( d, &d->alignedtree );
							if( aligned<0 ) return 0;
							matchOffset += (unsigned int)aligned;
						}
					} else if( extra ) {
						if( !lzxGetBits( d, extra, &v ) ) return 0;
						matchOffset += v;
					}
					d->R2 = d->R1;
					d->R1 = d->R0;
					d->R0 = matchOffset;
					break;
				}

				if( !lzxCopyMatch( d, matchOffset, (unsigned int)matchLength ) ) return 0;
				thisRun -= (unsigned long)matchLength;
			}
			break;

		case LZX_BLOCK_UNCOMPRESSED: {
			// the data is stored as it is
			unsigned long n = thisRun;
			unsigned long posn = d->windowPosn;
			if( (posn+n) > d->windowSize ) return 0;
			while( n>0 ) {
				unsigned int byte;
				if( !lzxGetBits( d, 8, &byte ) ) return 0;
				window[posn++] = (unsigned char)byte;
				n--;
			}
			d->windowPosn = posn;
			break;
		}

		default:
			return 0;
		}

		// a match can be longer than the run that was asked for
		if( thisRun > 0x40000000UL ) return 0;
	}

	// the frame has to be exactly full
	if( (d->windowPosn - d->framePosn) != frameSize ) return 0;

	// the bits are put back in line for the next frame
	lzxAlign( d );

	// the frame is copied out of the window, because the translation
	// of the call instructions must not change the history
	memcpy( d->frameBuf, &window[d->framePosn], frameSize );
	lzxIntelUntranslate( d, frameSize );

	d->framePosn += frameSize;
	d->frame++;
	if( d->windowPosn==d->windowSize ) d->windowPosn = 0;
	if( d->framePosn==d->windowSize ) d->framePosn = 0;

	return 1;
}

bool CabLzxDecode( CabLzxDecoder* d, unsigned char *out, unsigned long len )
{
	if( !d ) return false;

	while( len>0 ) {
		unsigned long n;

		if( d->pending==0 ) {
			unsigned long frameSize = LZX_FRAME_SIZE;
			if( d->totalLength && ((d->totalLength-d->offset) < frameSize) ) {
				frameSize = d->totalLength - d->offset;
			}
			if( frameSize==0 ) return false;
			if( !lzxDecodeFrame( d, frameSize ) ) return false;
			d->pending = frameSize;
			d->pendingPos = 0;
		}

		n = ( len<d->pending ? len : d->pending );
		memcpy( out, d->frameBuf+d->pendingPos, n );
		d->pendingPos += n;
		d->pending -= n;
		d->offset += n;
		out += n;
		len -= n;
	}
	return true;
}
