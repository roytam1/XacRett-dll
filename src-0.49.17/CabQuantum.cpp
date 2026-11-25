#include "stdafx.h"
#include "CabQuantum.h"
#include "QuantumCore.h"

#define QTM_FRAME_SIZE		32768
#define QTM_INPUT_MAX		65536

// The codec (models, arithmetic coder and tables) is shared with the .Q
// archives; only the framing and the model sizes live here.

struct CabQuantumDecoder
{
	// the input
	unsigned char	inBuf[QTM_INPUT_MAX];
	unsigned long	inLen;
	unsigned long	inPos;
	unsigned char	pendingByte;
	int				pendingValid;
	int				eof;
	CabQuantumFillFn	fill;
	void*			fillCtx;
	unsigned int	bitbuf;
	int				bitcnt;

	// the window
	unsigned char*	window;
	unsigned long	windowSize;
	unsigned long	windowPosn;	// where the next byte is written
	unsigned long	outPos;		// where the next byte is given out
	unsigned long	frameTodo;

	// the arithmetic coder
	QtmArith		arith;
	int				headerRead;

	// the models
	QtmModel	model0, model1, model2, model3;
	QtmModel	model4, model5, model6, model6len, model7;
	QtmSym			m0sym[64+1];
	QtmSym			m1sym[64+1];
	QtmSym			m2sym[64+1];
	QtmSym			m3sym[64+1];
	QtmSym			m4sym[24+1];
	QtmSym			m5sym[36+1];
	QtmSym			m6sym[42+1];
	QtmSym			m6lsym[27+1];
	QtmSym			m7sym[7+1];
};

// ------------------------------------------------------------------
// The bit reader.  The bits come from the top of sixteen bit words,
// and the two bytes of a word are in the file in the same order, so a
// word is read as they are.
// ------------------------------------------------------------------

static int qtmNeedBits( CabQuantumDecoder *d, int n )
{
	while( d->bitcnt < n ) {
		if( d->pendingValid ) {
			if( d->inPos >= d->inLen ) {
				int got;
				if( d->eof ) return 0;
				got = d->fill( d->fillCtx, d->inBuf, QTM_INPUT_MAX );
				if( got<=0 ) {
					d->eof = 1;
					return 0;
				}
				d->inLen = (unsigned long)got;
				d->inPos = 0;
			}
			d->bitbuf = (d->bitbuf << 16) |
						( ((unsigned int)d->pendingByte<<8) | d->inBuf[d->inPos] );
			d->inPos++;
			d->pendingValid = 0;
			d->bitcnt += 16;
			continue;
		}
		if( d->inPos+1 < d->inLen ) {
			d->bitbuf = (d->bitbuf << 16) |
						( ((unsigned int)d->inBuf[d->inPos]<<8) | d->inBuf[d->inPos+1] );
			d->inPos += 2;
			d->bitcnt += 16;
			continue;
		}
		if( d->inPos < d->inLen ) {
			d->pendingByte = d->inBuf[d->inPos];
			d->pendingValid = 1;
			d->inPos++;
			continue;
		}
		{
			int got;
			if( d->eof ) return 0;
			got = d->fill( d->fillCtx, d->inBuf, QTM_INPUT_MAX );
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

static int qtmGetBits( CabQuantumDecoder *d, int n, unsigned int *out )
{
	if( n==0 ) { *out = 0; return 1; }
	if( !qtmNeedBits( d, n ) ) return 0;
	d->bitcnt -= n;
	*out = ( d->bitbuf >> d->bitcnt ) & ((1u<<n)-1);
	d->bitbuf &= (1u<<d->bitcnt)-1;
	return 1;
}

// Read a number of bits that is not known when the code is compiled.
static int qtmGetManyBits( CabQuantumDecoder *d, int n, unsigned int *out )
{
	unsigned int v = 0;
	int got = 0;

	while( got<n ) {
		int take = ( n-got < 16 ? n-got : 16 );
		unsigned int part;
		if( !qtmGetBits( d, take, &part ) ) return 0;
		v = (v<<take) | part;
		got += take;
	}
	*out = v;
	return 1;
}

// Put the bits back in line with the bytes.
static void qtmAlign( CabQuantumDecoder *d )
{
	if( d->bitcnt & 7 ) {
		int drop = d->bitcnt & 7;
		d->bitcnt -= drop;
		d->bitbuf &= (1u<<d->bitcnt)-1;
	}
}

// ------------------------------------------------------------------
// The bit source of the shared arithmetic decoder.

static int qtm_getbit( void* ctx )
{
	CabQuantumDecoder* d = (CabQuantumDecoder*)ctx;
	unsigned int bit;

	if( !qtmGetBits( d, 1, &bit ) ) {
		d->eof = 1;
		return -1;
	}
	return (int)bit;
}

// ------------------------------------------------------------------

CabQuantumDecoder* CabQuantumCreate( int windowBits,
									 CabQuantumFillFn fill, void *fillCtx )
{
	CabQuantumDecoder* d;
	int i;

	if( windowBits<10 || windowBits>21 ) return NULL;

	d = (CabQuantumDecoder*) new BYTE[sizeof(CabQuantumDecoder)];
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
	d->frameTodo = QTM_FRAME_SIZE;

	// the models: the size of the match models depends on the window
	i = windowBits * 2;
	QtmInitModel( &d->model0,    &d->m0sym[0],   0, 64, 64 );
	QtmInitModel( &d->model1,    &d->m1sym[0],  64, 64, 64 );
	QtmInitModel( &d->model2,    &d->m2sym[0], 128, 64, 64 );
	QtmInitModel( &d->model3,    &d->m3sym[0], 192, 64, 64 );
	QtmInitModel( &d->model4,    &d->m4sym[0],   0, (i>24) ? 24 : i, (i>24) ? 24 : i );
	QtmInitModel( &d->model5,    &d->m5sym[0],   0, (i>36) ? 36 : i, (i>36) ? 36 : i );
	QtmInitModel( &d->model6,    &d->m6sym[0],   0, i, i );
	QtmInitModel( &d->model6len, &d->m6lsym[0],  0, 27, 27 );
	QtmInitModel( &d->model7,    &d->m7sym[0],   0, 7, 7 );

	return d;
}

void CabQuantumDestroy( CabQuantumDecoder* d )
{
	if( !d ) return;
	if( d->window ) delete [] d->window;
	delete [] (BYTE*)d;
}

// Copy a match out of the window.
static int qtmCopyMatch( CabQuantumDecoder *d, unsigned int offset, unsigned int length )
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
		if( offset > d->windowSize ) return 0;
		j = offset - posn;
		src = &window[d->windowSize - j];
		if( j < i ) {
			i -= j;
			while( j-- > 0 ) *dest++ = *src++;
			src = window;
		}
		while( i-- > 0 ) *dest++ = *src++;
	}
	else {
		src = dest - offset;
		while( i-- > 0 ) *dest++ = *src++;
	}
	d->windowPosn = posn + length;
	return 1;
}

bool CabQuantumDecode( CabQuantumDecoder* d, unsigned char *out, unsigned long len )
{
	if( !d ) return false;

	while( len>0 ) {
		unsigned long frameEnd;
		unsigned long avail;

		// the header of a frame, which starts the arithmetic coder
		if( !d->headerRead ) {
			if( !QtmArithInit( &d->arith, qtm_getbit, d ) ) return false;
			d->headerRead = 1;
		}

		// the frame ends when the window is full, when the frame is
		// full, or when the caller has enough
		frameEnd = d->windowPosn + d->frameTodo;
		if( frameEnd > d->windowSize ) frameEnd = d->windowSize;
		avail = d->outPos + len;
		if( avail < frameEnd ) frameEnd = avail;

		while( d->windowPosn < frameEnd ) {
			int selector, sym, extra, matchLength;
			unsigned int matchOffset;

			if( !QtmDecodeSymbol( &d->model7, &d->arith, qtm_getbit, d, &selector ) ) return false;
			if( selector < 4 ) {
				// a literal byte
				QtmModel* m = (selector==0) ? &d->model0 :
							  (selector==1) ? &d->model1 :
							  (selector==2) ? &d->model2 : &d->model3;
				if( !QtmDecodeSymbol( m, &d->arith, qtm_getbit, d, &sym ) ) return false;
				d->window[d->windowPosn++] = (unsigned char)sym;
				d->frameTodo--;
				continue;
			}

			switch( selector )
			{
			case 4:		// a match of three bytes
				if( !QtmDecodeSymbol( &d->model4, &d->arith, qtm_getbit, d, &sym ) ) return false;
				if( !qtmGetManyBits( d, QtmPositionExtra[sym], (unsigned int*)&extra ) ) return false;
				matchOffset = QtmPositionBase[sym] + (unsigned int)extra + 1;
				matchLength = 3;
				break;

			case 5:		// a match of four bytes
				if( !QtmDecodeSymbol( &d->model5, &d->arith, qtm_getbit, d, &sym ) ) return false;
				if( !qtmGetManyBits( d, QtmPositionExtra[sym], (unsigned int*)&extra ) ) return false;
				matchOffset = QtmPositionBase[sym] + (unsigned int)extra + 1;
				matchLength = 4;
				break;

			case 6:		// a match of a length that is coded as well
				if( !QtmDecodeSymbol( &d->model6len, &d->arith, qtm_getbit, d, &sym ) ) return false;
				if( !qtmGetManyBits( d, QtmLengthExtra[sym], (unsigned int*)&extra ) ) return false;
				matchLength = QtmLengthBase[sym] + extra + 5;
				if( !QtmDecodeSymbol( &d->model6, &d->arith, qtm_getbit, d, &sym ) ) return false;
				if( !qtmGetManyBits( d, QtmPositionExtra[sym], (unsigned int*)&extra ) ) return false;
				matchOffset = QtmPositionBase[sym] + (unsigned int)extra + 1;
				break;

			default:
				return false;
			}

			if( d->frameTodo < (unsigned long)matchLength ) return false;
			d->frameTodo -= (unsigned long)matchLength;
			if( !qtmCopyMatch( d, matchOffset, (unsigned int)matchLength ) ) return false;
		}

		// the bytes that were decoded are given out
		{
			unsigned long n = d->windowPosn - d->outPos;
			if( n>len ) n = len;
			memcpy( out, d->window+d->outPos, n );
			d->outPos += n;
			out += n;
			len -= n;
		}

		// the frame is full
		if( d->frameTodo==0 ) {
			unsigned int v;
			// the bits are put back in line with the bytes, and the
			// padding that follows is skipped by looking for the byte
			// 0xFF, which the writer puts at the end of every block
			qtmAlign( d );
			for(;;) {
				if( !qtmGetBits( d, 8, &v ) ) return false;
				if( v==0xFF ) break;
			}
			d->headerRead = 0;
			d->frameTodo = QTM_FRAME_SIZE;
		}

		// the window is full, so it starts again
		if( d->windowPosn==d->windowSize ) {
			d->windowPosn = 0;
			d->outPos = 0;
		}
	}
	return true;
}
