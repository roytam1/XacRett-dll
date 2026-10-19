
#include "stdafx.h"
#include "LzhDecoder2.h"

void CLzhDecoder2::Unstore()
{
#define UNS_BUFSIZE (32768)
	int how_much;
	char buf[UNS_BUFSIZE];

	if( cmpsize==-1 )
	{
		while( (how_much=fread(buf,1,UNS_BUFSIZE,in))!=0 )
		{
			if( how_much==-1 )
				break;
			fwrite(buf,1,how_much,out);
		}
		return;
	}

	while( cmpsize>0 )
	{
		how_much = cmpsize>UNS_BUFSIZE?UNS_BUFSIZE:cmpsize;
		if( 0>=(how_much=fread( buf,1,how_much,in )) )
			break;
		fwrite( buf,1,how_much,out );
		cmpsize -= how_much;
	}
}

void CLzhDecoder2::Decode( lzh_method mhd,FILE* infile,DWORD insize,
										FILE* outfile,DWORD outsize )
{
	in      = infile;
	cmpsize = insize;
	out     = outfile;
	orisize = outsize;

	DWORD dicsiz,dicbit;

	// メソッドによっていろいろ切り替え
	switch( mhd )
	{
	case LH0:
	case LZ4:Unstore();	return;
	default:			return;

	case ARJ:	np=17,pbit=5,dicsiz=26624;break;
	case LH4:	np=14,pbit=4,dicbit=12,dicsiz=(1<<dicbit);break;
	case LH5:	np=14,pbit=4,dicbit=13,dicsiz=(1<<dicbit);break;
	case LH6:	np=16,pbit=5,dicbit=15,dicsiz=(1<<dicbit);break;
	case LH7:	np=17,pbit=5,dicbit=16,dicsiz=(1<<dicbit);break;

	case LZ5:	dicbit=12,dicsiz=(1<<dicbit);break;
	case LZS:	dicbit=11,dicsiz=(1<<dicbit);break;

	case LH1:	np=(1<<6),dicbit=12,dicsiz=(1<<dicbit);break;
	case LH2:	dicbit=13,dicsiz=(1<<dicbit);break;
	case LH3:	np=(1<<9),dicbit=13,dicsiz=(1<<dicbit);break;
	}

	BYTE* text = new BYTE[dicsiz];
	memset( text,' ',dicsiz );

	if( LH4<=mhd && mhd<=ARJ )
	{
		init_getbits();
		blocksize = 0;
	}
	else if( mhd==LZS )
		init_getbits();
	else if( mhd==LZ5 )
	{
		// The -lz5- history buffer starts with a fixed pattern
		// that archives in the wild copy from.  The reference
		// decoder fills its 4096 byte ring buffer with 13 copies
		// of each byte value, then all values ascending, then all
		// descending, then 128 zeros, then 110 spaces and finally
		// 18 zeros.  This decoder uses a linear window that is
		// rotated by START_OFFSET (18) relative to that ring
		// buffer, so the trailing 18 zeros belong at the FRONT of
		// the window.  Leaving spaces there breaks archives that
		// copy from the start of the history buffer.
		flagcnt = 0;
		int i;
		memset( text, 0, 18 );
		for( i=0; i<256; i++ )	memset(&text[i * 13 + 18], i, 13);
		for( i=0; i<256; i++ )	text[256 * 13 + 18 + i] = i;
		for( i=0; i<256; i++ )	text[256 * 13 + 256 + 18 + i] = 255 - i;
		memset(&text[256 * 13 + 512 + 18], 0, 128);
		memset(&text[256 * 13 + 512 + 128 + 18], ' ', 128 - 18);
	}
	else if( mhd==LH3 )
		init_getbits();
	else if( mhd==LH2 )
	{
		n_max = 286;
		init_getbits();

		int             i, j, f;
		n1 = (n_max >= 256 + MAXMATCH - THRESHOLD + 1) ? 512 : n_max - 1;
		for (i = 0; i < TREESIZE_C; i++) {
			stock[i] = i;
			block[i] = 0;
		}
		for (i = 0, j = (int)n_max * 2 - 2; i < (int)n_max; i++, j--)
		{
			freq[j] = 1;
			child[j] = ~i;
			s_node[i] = j;
			block[j] = 1;
		}
		avail = 2;
		edge[1] = (short)(n_max - 1);
		i = n_max * 2 - 2;
		while (j >= 0) {
			f = freq[j] = freq[i] + freq[i - 1];
			child[j] = i;
			parent[i] = parent[i - 1] = j;
			if (f == freq[j + 1]) {
				edge[block[j] = block[j + 1]] = j;
			}
			else {
				edge[block[j] = stock[avail++]] = j;
			}
			i -= 2;
			j--;
		}

		freq[ROOT_P] = 1;
		child[ROOT_P] = ~(N_CHAR);
		s_node[N_CHAR] = ROOT_P;
		edge[block[ROOT_P] = stock[avail++]] = ROOT_P;
		most_p = ROOT_P;
		total_p = 0;
		nn = 1 << dicbit;
		nextcount = 64;
	}
	else if( mhd==LH1 )
	{
		n_max = 314;
		init_getbits();
		np = 1 << 6;

		int             i, j, f;
		n1 = (n_max >= 256 + 60 - THRESHOLD + 1) ? 512 : n_max - 1;
		for (i = 0; i < TREESIZE_C; i++)
		{
			stock[i] = i;
			block[i] = 0;
		}
		for (i = 0, j = (int)n_max * 2 - 2; i < (int)n_max; i++, j--)
		{
			freq[j] = 1;
			child[j] = ~i;
			s_node[i] = j;
			block[j] = 1;
		}
		avail = 2;
		edge[1] = (short)(n_max - 1);
		i = n_max * 2 - 2;
		while (j >= 0) {
			f = freq[j] = freq[i] + freq[i - 1];
			child[j] = i;
			parent[i] = parent[i - 1] = j;
			if (f == freq[j + 1]) {
				edge[block[j] = block[j + 1]] = j;
			}
			else {
				edge[block[j] = stock[avail++]] = j;
			}
			i -= 2;
			j--;
		}

		ready_made(0);
		make_table(np, pt_len, 8, pt_table);
	}

	// デコード
	DWORD count=0;
	loc=0;
	int offset=(mhd==LZS)?(0x0100-2):(0x0100-3);
	while( count<orisize )
	{
		int c = Decode_C(mhd);

		if( c<=255 )
		{
			text[loc++] = c;
			if( loc==dicsiz )
			{
				fwrite(text,1,dicsiz,out);
				loc = 0;
			}
			count++;
		}
		else
		{
			int j = c - offset;
			count += j;
			int i = Decode_P(mhd);
			if( (i=loc-i-1)<0 )
				i += dicsiz;

			for( int k=0; k<j; k++ )
			{
				text[loc++] = text[i];
				if( loc>=dicsiz )
				{
					fwrite( text,1,dicsiz,out );
					loc = 0;
				}
				if( ++i>=(int)dicsiz )
					i = 0;
			}
		}
	}

	if( loc!=0 )
		fwrite( text,1,loc,out );

	delete [] text;
}

/**************** LH4-7,ARJ,ZOO ************************/

void CLzhDecoder2::make_table(WORD nchar,BYTE* bitlen,
								WORD tablebits,WORD* table)
{
	WORD count[17],weight[17],start[17],total,*p;
	unsigned int i;
	int j,k,l,m,n,avail;

	avail = nchar;

	for( i=1; i<=16; i++ )
	{
		count[i] = 0;
		weight[i] = 1 << (16 - i);
	}

	for( i=0; i<nchar; i++ )
		count[bitlen[i]]++;

	total = 0;
	for( i=1; i<=16; i++ )
	{
		start[i] = total;
		total += weight[i] * count[i];
	}
	if( (total & 0xffff) != 0 )
		return;//error("Bad table (5)\n");

	m = 16 - tablebits;
	for( i=1; i<=tablebits; i++ )
	{
		start[i] >>= m;
		weight[i] >>= m;
	}

	j = start[tablebits + 1] >> m;
	k = 1 << tablebits;
	if( j!=0 )
		for( i=j; i<(unsigned)k; i++ )
			table[i] = 0;

	for( j=0; j<nchar; j++ )
	{
		k = bitlen[j];
		if( k==0 )
			continue;
		l = start[k] + weight[k];
		if( k<=tablebits )
		{
			for( i=start[k]; i<(unsigned)l; i++ )
				table[i] = j;
		}
		else
		{
			p = &table[(i = start[k]) >> m];
			i <<= tablebits;
			n = k - tablebits;
			while (--n >= 0)
			{
				if (*p == 0)
				{
					right[avail] = left[avail] = 0;
					*p = avail++;
				}
				if (i & 0x8000)
					p = &right[*p];
				else
					p = &left[*p];
				i <<= 1;
			}
			*p = j;
		}
		start[k] = l;
	}
}

void CLzhDecoder2::read_pt_len(short nn,short nbit,short i_special)
{
	short i, c, n=getbits((BYTE)nbit);

	if( n==0 )
	{
		c = getbits((BYTE)nbit);
		for( i=0; i<nn; i++ )	pt_len[i] = 0;
		for( i=0; i<256; i++ )	pt_table[i] = c;
	}
	else
	{
		short i = 0;
		while( i<n )
		{
			c = bitbuf >> (16-3);
			if( c==7 )
			{
				WORD mask = 1<<(16-4);
				while( mask&bitbuf )
				{
					mask >>= 1;
					c++;
				}
			}
			fillbuf( (c<7) ? 3 : c-3 );
			pt_len[i++] = (BYTE)c;
			if( i==i_special )
			{
				c = getbits(2);
				while( --c>=0 )
					pt_len[i++] = 0;
			}
		}
		while( i<nn )
			pt_len[i++] = 0;
		make_table(nn,pt_len,8,pt_table);
	}
}

void CLzhDecoder2::read_c_len()
{
	short i, c, n=getbits(CBIT);

	if( n==0 )
	{
		c = getbits(CBIT);
		for( i=0; i<NC; i++ )	c_len[i] = 0;
		for( i=0; i<4096; i++ )	c_table[i] = c;
	}
	else
	{
		short i = 0;
		while( i<n )
		{
			c = pt_table[ bitbuf>>(16-8) ];
			if( c>=NT )
			{
				WORD mask = 1<<(16-9);
				do
				{
					if( bitbuf&mask )c = right[c];
					else			 c = left[c];
					mask >>= 1;
				}while( c>=NT );
			}
			fillbuf(pt_len[c]);
			if( c<=2 )
			{
				if( c==0 )		c = 1;
				else if( c==1 )	c = getbits(4) + 3;
				else			c = getbits(CBIT) + 20;
				while( --c>=0 )
					c_len[i++] = 0;
			}
			else
				c_len[i++] = c-2;
		}
		while( i<NC )
			c_len[i++]=0;
		make_table(NC,c_len,12,c_table);
	}
}

WORD CLzhDecoder2::decode_c_st1()
{
	WORD j,mask;

	if( blocksize==0 )
	{
		blocksize = getbits(16);
		read_pt_len(NT,TBIT,3);
		read_c_len();
		read_pt_len(np,pbit,-1);
	}
	blocksize--;
	j = c_table[ bitbuf>>4 ];
	if( j<NC )
		fillbuf(c_len[j]);
	else
	{
		fillbuf(12);
		mask = 1 << (16-1);
		do
		{
			if( bitbuf&mask )	j = right[j];
			else				j = left[j];
			mask >>= 1;
		}while( j>=NC );
		fillbuf( c_len[j]-12 );
	}
	return j;
}

WORD CLzhDecoder2::decode_p_st1()
{
	WORD j, mask;

	j = pt_table[ bitbuf>>(16-8) ];
	if( j<np )
		fillbuf(pt_len[j]);
	else
	{
		fillbuf(8);
		mask = 1 << (16-1);
		do
		{
			if( bitbuf&mask )	j = right[j];
			else				j = left[j];
			mask >>= 1;
		}while( j>=np );
		fillbuf(pt_len[j] - 8);
	}
	if( j!=0 )
		j = (1<<(j-1)) + getbits(j-1);
	return j;
}

/**************** LArc ************************/

WORD CLzhDecoder2::decode_c_lzs()
{
	if( getbits(1) )
		return getbits(8);
	else
	{
		matchpos = getbits(11);
		return getbits(4) + 0x100;
	}
}

WORD CLzhDecoder2::decode_p_lzs()
{
#define MAGIC0		18
	return (WORD)((loc-matchpos-MAGIC0)&0x7ff);
}

WORD CLzhDecoder2::decode_c_lz5()
{
	int c;

	if( flagcnt==0 )
	{
		flagcnt = 8;
		flag = getc(in);
	}
	flagcnt--;
	c = getc(in);
	if( (flag&1)==0 )
	{
		matchpos = c;
		c = getc(in);
		matchpos += (c&0xf0)<<4;
		c &= 0x0f;
		c += 0x100;
	}
	flag >>= 1;
	return (WORD)c;
}

WORD CLzhDecoder2::decode_p_lz5()
{
#define MAGIC5		19
	return (WORD)(loc-matchpos-MAGIC5)&0xfff;
}

/******************** lh1-3 *************************************/
/*lh1
	{decode_c_dyn, decode_p_st0, decode_start_fix},
lh2
	{decode_c_dyn, decode_p_dyn, decode_start_dyn},
lh3
	{decode_c_st0, decode_p_st0, decode_start_st0},*/

#define N1 286
#define N2 (2*N1-1)
#define NP			(8*1024/64)
#define NP2			(NP*2-1)
static const int fixed[2][16] = {
	{3, 0x01, 0x04, 0x0c, 0x18, 0x30, 0},	/* old compatible */
	{2, 0x01, 0x01, 0x03, 0x06, 0x0D, 0x1F, 0x4E, 0}	/* 8K buf */
};

void CLzhDecoder2::ready_made(int method)
{
	int             i, j;
	unsigned int    code, weight;
	const int            *tbl;

	tbl = fixed[method];
	j = *tbl++;
	weight = 1 << (16 - j);
	code = 0;
	for( i=0; i<np; i++ )
	{
		while( *tbl==i )
		{
			j++;
			tbl++;
			weight >>= 1;
		}
		pt_len[i] = j;
		pt_code[i] = code;
		code += weight;
	}
}

void CLzhDecoder2::read_tree_c()
{
	int i=0;
	while( i<N1 )
	{
		if( getbits(1) )
			c_len[i] = getbits(4) + 1;
		else
			c_len[i] = 0;
		if( ++i==3 && c_len[0]==1 && c_len[1]==1 && c_len[2]==1 )
		{
			int c = getbits(CBIT);
			for( i=0; i<N1; i++ )	c_len[i] = 0;
			for( i=0; i<4096; i++ )	c_table[i] = c;
			return;
		}
	}
	make_table( N1,c_len,12,c_table );
}

void CLzhDecoder2::read_tree_p()
{
	int i=0;
	while( i<NP )
	{
		pt_len[i] = (BYTE)getbits(4);
		if( ++i==3 && pt_len[0]==1 && pt_len[1]==1 && pt_len[2]==1 )
		{
			int c = getbits(9);
			for( i=0; i<NP; i++ )	c_len[i] = 0;
			for( i=0; i<256; i++ )	c_table[i] = c;
			return;
		}
	}
}

WORD CLzhDecoder2::decode_c_st0()
{
	static unsigned short blocksize = 0;

	if( blocksize==0 )
	{
		blocksize = getbits(16);
		read_tree_c();
		if( getbits(1) )
			read_tree_p();
		else
			ready_made(1);
		make_table( NP,pt_len,8,pt_table );
	}
	blocksize--;
	int j = c_table[bitbuf >> 4];
	if( j<N1 )
		fillbuf(c_len[j]);
	else
	{
		fillbuf(12);
		int i = bitbuf;
		do
		{
			if( (short)i<0 )j = right[j];
			else			j = left[j];
			i <<= 1;
		}while( j>=N1 );
		fillbuf( c_len[j]-12 );
	}
	if( j==N1-1 )
		j += getbits(8);
	return j;
}

WORD CLzhDecoder2::decode_p_st0()
{
	int j = pt_table[bitbuf >> 8];
	if( j<np )
		fillbuf(pt_len[j]);
	else
	{
		fillbuf(8);
		int i = bitbuf;
		do
		{
			if( (short)i<0 )j = right[j];
			else			j = left[j];
			i <<= 1;
		}while( j>=np );
		fillbuf( pt_len[j]-8 );
	}
	return (j<<6)+getbits(6);
}

///////////////

void CLzhDecoder2::reconst(int start,int end)
{
	int             i, j, k, l, b;
	unsigned int    f, g;

	for (i = j = start; i < end; i++) {
		if ((k = child[i]) < 0) {
			freq[j] = (freq[i] + 1) / 2;
			child[j] = k;
			j++;
		}
		if (edge[b = block[i]] == i) {
			stock[--avail] = b;
		}
	}
	j--;
	i = end - 1;
	l = end - 2;
	while (i >= start) {
		while (i >= l) {
			freq[i] = freq[j];
			child[i] = child[j];
			i--, j--;
		}
		f = freq[l] + freq[l + 1];
		for (k = start; f < freq[k]; k++);
		while (j >= k) {
			freq[i] = freq[j];
			child[i] = child[j];
			i--, j--;
		}
		freq[i] = f;
		child[i] = l + 1;
		i--;
		l -= 2;
	}
	f = 0;
	for (i = start; i < end; i++) {
		if ((j = child[i]) < 0)
			s_node[~j] = i;
		else
			parent[j] = parent[j - 1] = i;
		if ((g = freq[i]) == f) {
			block[i] = b;
		}
		else {
			edge[b = block[i] = stock[avail++]] = i;
			f = g;
		}
	}
}

int CLzhDecoder2::swap_inc(int p)
{
	int             b, q, r, s;

	b = block[p];
	if ((q = edge[b]) != p) {	/* swap for leader */
		r = child[p];
		s = child[q];
		child[p] = s;
		child[q] = r;
		if (r >= 0)
			parent[r] = parent[r - 1] = q;
		else
			s_node[~r] = q;
		if (s >= 0)
			parent[s] = parent[s - 1] = p;
		else
			s_node[~s] = p;
		p = q;
		goto Adjust;
	}
	else if (b == block[p + 1]) {
Adjust:
		edge[b]++;
		if (++freq[p] == freq[p - 1]) {
			block[p] = block[p - 1];
		}
		else {
			edge[block[p] = stock[avail++]] = p;	/* create block */
		}
	}
	else if (++freq[p] == freq[p - 1]) {
		stock[--avail] = b;	/* delete block */
		block[p] = block[p - 1];
	}
	return parent[p];
}

WORD CLzhDecoder2::decode_c_dyn()
{
	int c = child[ROOT_C];
	short buf = bitbuf,cnt = 0;
	do
	{
		c = child[c - (buf < 0)];
		buf <<= 1;
		if( ++cnt==16 )
		{
			fillbuf(16);
			buf = bitbuf;
			cnt = 0;
		}
	}while( c>0 );
	fillbuf((BYTE)cnt);
	c = ~c;
	update_c(c);
	if( c==n1 )
		c+=getbits(8);
	return c;
}

WORD CLzhDecoder2::decode_p_dyn()
{
	while (count > nextcount)
	{
		make_new_node(nextcount / 64);
		if( (int)(nextcount+=64)>=nn )
			nextcount = 0xffffffff;
	}
	int c = child[ROOT_P];
	short buf = bitbuf,cnt = 0;
	while( c>0 )
	{
		c = child[c - (buf < 0)];
		buf <<= 1;
		if (++cnt == 16)
		{
			fillbuf(16);
			buf = bitbuf;
			cnt = 0;
		}
	}
	fillbuf((BYTE)cnt);
	c = (~c) - N_CHAR;
	update_p(c);

	return (c << 6) + getbits(6);
}

void CLzhDecoder2::update_c(int p)
{
	if (freq[ROOT_C] == 0x8000)
		reconst(0, n_max * 2 - 1);
	freq[ROOT_C]++;
	int q = s_node[p];
	do{q = swap_inc(q);} while (q != ROOT_C);
}

void CLzhDecoder2::update_p(int p)
{
	if (total_p == 0x8000)
	{
		reconst(ROOT_P, most_p + 1);
		total_p = freq[ROOT_P];
		freq[ROOT_P] = 0xffff;
	}
	int q = s_node[p + N_CHAR];
	while( q!=ROOT_P )
		q = swap_inc(q);
	total_p++;
}

void CLzhDecoder2::make_new_node(int p)
{
	int r = most_p + 1;
	int q = r + 1;
	s_node[~(child[r] = child[most_p])] = r;
	child[q] = ~(p + N_CHAR);
	child[most_p] = q;
	freq[r] = freq[most_p];
	freq[q] = 0;
	block[r] = block[most_p];
	if (most_p == ROOT_P)
	{
		freq[ROOT_P] = 0xffff;
		edge[block[ROOT_P]]++;
	}
	parent[r] = parent[q] = most_p;
	edge[block[q] = stock[avail++]] = s_node[p + N_CHAR] = most_p = q;
	update_p(p);
}


/******************** bit単位のread&write ************************/

void CLzhDecoder2::init_getbits()
{
	bitbuf = 0;
	subbitbuf = 0;
	bitcount = 0;
	fillbuf(16);
}

void CLzhDecoder2::fillbuf(BYTE n)
{
	while( n>bitcount )
	{
		n -= bitcount;
		bitbuf = (bitbuf<<bitcount) + (subbitbuf>>(8-bitcount));
		if( cmpsize!=0 )
		{
			cmpsize--;
			subbitbuf = (BYTE)getc(in);
		}
		else
			subbitbuf = 0;
		bitcount = 8;
	}
	bitcount -= n;
	bitbuf = (bitbuf<<n) + (subbitbuf>>(8-n));
	subbitbuf <<= n;
}

WORD CLzhDecoder2::getbits(BYTE n)
{
	WORD x = bitbuf>>(16-n);
	fillbuf(n);
	return x;
}
