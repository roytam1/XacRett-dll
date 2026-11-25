// The codec that the .Q archives and the Cabinet Quantum method share.
// See QuantumCore.h.  The widths here are load bearing: the cumulative
// counts are 16 bit and the arithmetic is 32 bit unsigned.  The truncation
// of the counts on a light rescale is what the original encoder expects.

#include "stdafx.h"
#include "QuantumCore.h"

const unsigned char QtmLengthBase[27] = {
	  0,   1,   2,   3,   4,   5,   6,   8,  10,  12,  14,  18,  22,  26,
	 30,  38,  46,  54,  62,  78,  94, 110, 126, 158, 190, 222, 254
};
const unsigned char QtmLengthExtra[27] = {
	0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};

const unsigned int QtmPositionBase[42] = {
	  0,   1,   2,   3,   4,   6,   8,  12,  16,  24,  32,  48,  64,  96, 128, 192,
	256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576,
	32768, 49152, 65536, 98304, 131072, 196608, 262144, 393216, 524288, 786432,
	1048576, 1572864
};
const unsigned char QtmPositionExtra[42] = {
	 0,  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,
	 7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14,
	15, 15, 16, 16, 17, 17, 18, 18, 19, 19
};

void QtmInitModel( QtmModel* m, QtmSym* syms, int start, int entries, int total )
{
	int i;

	m->shiftsLeft = 4;
	m->entries = entries;
	m->syms = syms;
	for( i=0; i<=entries; i++ )
	{
		syms[i].sym = (unsigned short)(start + i);
		syms[i].cumfreq = (unsigned short)(total - i);
	}
}

// Halve the frequencies, and every so often rebuild the model: the
// frequencies become plain counts, the entries are sorted by count (the
// symbol travels with its count) and the counts are made cumulative again.
static void qtm_update_model( QtmModel* m )
{
	QtmSym tmp;
	int i, j;

	if( --m->shiftsLeft )
	{
		for( i=m->entries-1; i>=0; i-- )
		{
			m->syms[i].cumfreq >>= 1;
			if( m->syms[i].cumfreq <= m->syms[i+1].cumfreq )
				m->syms[i].cumfreq = (unsigned short)(m->syms[i+1].cumfreq + 1);
		}
	}
	else
	{
		m->shiftsLeft = 50;
		for( i=0; i<m->entries; i++ )
		{
			m->syms[i].cumfreq = (unsigned short)(m->syms[i].cumfreq - m->syms[i+1].cumfreq);
			m->syms[i].cumfreq++;
			m->syms[i].cumfreq >>= 1;
		}
		for( i=0; i<m->entries-1; i++ )
		{
			for( j=i+1; j<m->entries; j++ )
			{
				if( m->syms[i].cumfreq < m->syms[j].cumfreq )
				{
					tmp = m->syms[i];
					m->syms[i] = m->syms[j];
					m->syms[j] = tmp;
				}
			}
		}
		for( i=m->entries-1; i>=0; i-- )
			m->syms[i].cumfreq = (unsigned short)(m->syms[i].cumfreq + m->syms[i+1].cumfreq);
	}
}

bool QtmArithInit( QtmArith* a, QtmGetBit getbit, void* ctx )
{
	int i, b;

	a->H = 0xFFFF;
	a->L = 0;
	a->C = 0;
	for( i=0; i<16; i++ )
	{
		b = getbit( ctx );
		if( b<0 ) return false;
		a->C = (unsigned short)((a->C << 1) | b);
	}
	return true;
}

bool QtmDecodeSymbol( QtmModel* m, QtmArith* a, QtmGetBit getbit,
					  void* ctx, int* sym )
{
	unsigned range, total, val, symf;
	int i, s, b;

	total = m->syms[0].cumfreq;
	range = ((unsigned)a->H - (unsigned)a->L) + 1u;
	if( !range || !total ) return false;

	val = (((unsigned)a->C - (unsigned)a->L + 1u) * total - 1u) / range;
	for( i=1; i<m->entries; i++ )
	{
		if( m->syms[i].cumfreq <= val ) break;
	}
	s = m->syms[i-1].sym;

	symf = m->syms[0].cumfreq;
	a->H = (unsigned short)( (unsigned)a->L +
			((m->syms[i-1].cumfreq * range) / symf) - 1u );
	a->L = (unsigned short)( (unsigned)a->L +
			((m->syms[i].cumfreq * range) / symf) );

	do { m->syms[--i].cumfreq = (unsigned short)(m->syms[i].cumfreq + 8); }
	while( i>0 );
	if( m->syms[0].cumfreq > 3800 ) qtm_update_model( m );

	for(;;)
	{
		if( (a->L & 0x8000) == (a->H & 0x8000) )
		{
			// the bit is decided
		}
		else if( (a->L & 0x4000) && !(a->H & 0x4000) )
		{
			a->C ^= 0x4000;
			a->L &= 0x3FFF;
			a->H |= 0x4000;
		}
		else
		{
			break;
		}
		a->L = (unsigned short)(a->L << 1);
		a->H = (unsigned short)((a->H << 1) | 1);
		b = getbit( ctx );
		if( b<0 ) return false;
		a->C = (unsigned short)((a->C << 1) | b);
	}
	*sym = s;
	return true;
}
