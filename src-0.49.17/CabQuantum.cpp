#include "stdafx.h"
#include "CabQuantum.h"

#define QTM_FRAME_SIZE		32768
#define QTM_INPUT_MAX		65536

// The position slots: the base of every slot and the number of extra
// bits that go with it.
static const unsigned int qtmPositionBase[42] = {
	  0,   1,   2,   3,   4,   6,   8,  12,  16,  24,  32,  48,  64,  96, 128, 192,
	256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576,
	32768, 49152, 65536, 98304, 131072, 196608, 262144, 393216, 524288, 786432, 1048576, 1572864
};
static const unsigned char qtmExtraBits[42] = {
	 0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
	 7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
	15, 15, 16, 16, 17, 17, 18, 18, 19, 19
};

// The lengths of the variable length matches.
static const unsigned char qtmLengthBase[27] = {
	  0,  1,  2,  3,  4,  5,  6,  8, 10, 12, 14, 18, 22, 26,
	 30, 38, 46, 54, 62, 78, 94, 110, 126, 158, 190, 222, 254
};
static const unsigned char qtmLengthExtra[27] = {
	0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

struct CabQtmSym
{
	unsigned short	sym;
	unsigned short	cumfreq;
};

struct CabQtmModel
{
	int				shiftsLeft;
	int				entries;
	CabQtmSym*		syms;
};

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
	unsigned short	H, L, C;
	int				headerRead;

	// the models
	CabQtmModel	model0, model1, model2, model3;
	CabQtmModel	model4, model5, model6, model6len, model7;
	CabQtmSym		m0sym[64+1];
	CabQtmSym		m1sym[64+1];
	CabQtmSym		m2sym[64+1];
	CabQtmSym		m3sym[64+1];
	CabQtmSym		m4sym[24+1];
	CabQtmSym		m5sym[36+1];
	CabQtmSym		m6sym[42+1];
	CabQtmSym		m6lsym[27+1];
	CabQtmSym		m7sym[7+1];
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
// The models.
// ------------------------------------------------------------------

static void qtmInitModel( CabQtmModel *m, CabQtmSym *syms, int start, int len )
{
	int i;

	m->shiftsLeft = 4;
	m->entries = len;
	m->syms = syms;
	for( i=0; i<=len; i++ ) {
		syms[i].sym = (unsigned short)(start+i);
		syms[i].cumfreq = (unsigned short)(len-i);
	}
}

// When the frequencies have grown too big, they are halved and the
// symbols are put in the order of their frequencies again.
static void qtmUpdateModel( CabQtmModel *m )
{
	CabQtmSym tmp;
	int i, j;

	if( --m->shiftsLeft ) {
		for( i=m->entries-1; i>=0; i-- ) {
			m->syms[i].cumfreq >>= 1;
			if( m->syms[i].cumfreq <= m->syms[i+1].cumfreq ) {
				m->syms[i].cumfreq = (unsigned short)(m->syms[i+1].cumfreq+1);
			}
		}
	}
	else {
		m->shiftsLeft = 50;
		for( i=0; i<m->entries; i++ ) {
			m->syms[i].cumfreq -= m->syms[i+1].cumfreq;
			m->syms[i].cumfreq++;
			m->syms[i].cumfreq >>= 1;
		}
		// the symbols are put in the order of their frequencies
		for( i=0; i<m->entries-1; i++ ) {
			for( j=i+1; j<m->entries; j++ ) {
				if( m->syms[i].cumfreq < m->syms[j].cumfreq ) {
					tmp = m->syms[i];
					m->syms[i] = m->syms[j];
					m->syms[j] = tmp;
				}
			}
		}
		for( i=m->entries-1; i>=0; i-- ) {
			m->syms[i].cumfreq += m->syms[i+1].cumfreq;
		}
	}
}

// Read one symbol from a model.
static int qtmGetSymbol( CabQuantumDecoder *d, CabQtmModel *m, int *var )
{
	unsigned int range, symf;
	unsigned short H = d->H, L = d->L, C = d->C;
	int i;

	range = (unsigned int)(((H-L) & 0xFFFF) + 1);
	symf = (unsigned int)(((((unsigned int)(C-L+1) * m->syms[0].cumfreq) - 1) / range) & 0xFFFF);

	for( i=1; i<m->entries; i++ ) {
		if( m->syms[i].cumfreq <= symf ) break;
	}
	*var = m->syms[i-1].sym;

	range = (unsigned int)(H-L) + 1;
	symf = m->syms[0].cumfreq;
	H = (unsigned short)( L + ((m->syms[i-1].cumfreq * range) / symf) - 1 );
	L = (unsigned short)( L + ((m->syms[i].cumfreq   * range) / symf) );

	do { m->syms[--i].cumfreq += 8; } while( i>0 );
	if( m->syms[0].cumfreq > 3800 ) qtmUpdateModel( m );

	// put the coder back in line
	for(;;) {
		if( (L & 0x8000) != (H & 0x8000) ) {
			if( (L & 0x4000) && !(H & 0x4000) ) {
				C ^= 0x4000;
				L &= 0x3FFF;
				H |= 0x4000;
			}
			else break;
		}
		L = (unsigned short)( L << 1 );
		H = (unsigned short)( (H << 1) | 1 );
		{
			unsigned int bit;
			if( !qtmGetBits( d, 1, &bit ) ) return 0;
			C = (unsigned short)( (C << 1) | bit );
		}
	}

	d->H = H;
	d->L = L;
	d->C = C;
	return 1;
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
	qtmInitModel( &d->model0,    &d->m0sym[0],   0, 64 );
	qtmInitModel( &d->model1,    &d->m1sym[0],  64, 64 );
	qtmInitModel( &d->model2,    &d->m2sym[0], 128, 64 );
	qtmInitModel( &d->model3,    &d->m3sym[0], 192, 64 );
	qtmInitModel( &d->model4,    &d->m4sym[0],   0, (i>24) ? 24 : i );
	qtmInitModel( &d->model5,    &d->m5sym[0],   0, (i>36) ? 36 : i );
	qtmInitModel( &d->model6,    &d->m6sym[0],   0, i );
	qtmInitModel( &d->model6len, &d->m6lsym[0],  0, 27 );
	qtmInitModel( &d->model7,    &d->m7sym[0],   0, 7 );

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
			unsigned int v;
			d->H = 0xFFFF;
			d->L = 0;
			if( !qtmGetBits( d, 16, &v ) ) return false;
			d->C = (unsigned short)v;
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

			if( !qtmGetSymbol( d, &d->model7, &selector ) ) return false;
			if( selector < 4 ) {
				// a literal byte
				CabQtmModel* m = (selector==0) ? &d->model0 :
								 (selector==1) ? &d->model1 :
								 (selector==2) ? &d->model2 : &d->model3;
				if( !qtmGetSymbol( d, m, &sym ) ) return false;
				d->window[d->windowPosn++] = (unsigned char)sym;
				d->frameTodo--;
				continue;
			}

			switch( selector )
			{
			case 4:		// a match of three bytes
				if( !qtmGetSymbol( d, &d->model4, &sym ) ) return false;
				if( !qtmGetManyBits( d, qtmExtraBits[sym], (unsigned int*)&extra ) ) return false;
				matchOffset = qtmPositionBase[sym] + (unsigned int)extra + 1;
				matchLength = 3;
				break;

			case 5:		// a match of four bytes
				if( !qtmGetSymbol( d, &d->model5, &sym ) ) return false;
				if( !qtmGetManyBits( d, qtmExtraBits[sym], (unsigned int*)&extra ) ) return false;
				matchOffset = qtmPositionBase[sym] + (unsigned int)extra + 1;
				matchLength = 4;
				break;

			case 6:		// a match of a length that is coded as well
				if( !qtmGetSymbol( d, &d->model6len, &sym ) ) return false;
				if( !qtmGetManyBits( d, qtmLengthExtra[sym], (unsigned int*)&extra ) ) return false;
				matchLength = qtmLengthBase[sym] + extra + 5;
				if( !qtmGetSymbol( d, &d->model6, &sym ) ) return false;
				if( !qtmGetManyBits( d, qtmExtraBits[sym], (unsigned int*)&extra ) ) return false;
				matchOffset = qtmPositionBase[sym] + (unsigned int)extra + 1;
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
