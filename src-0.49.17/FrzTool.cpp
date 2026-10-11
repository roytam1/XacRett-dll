
#include "stdafx.h"
#include "FrzTool.h"


#define bits(x) ((int)sizeof(x)*8)
#define BYSH  (bits(bitbuf)-8)
#define BISH  (bits(bitbuf)-1)
#define InitIO(){ overrun=bitlen=0; bitbuf=0; }
#define DefBits unsigned long locbuf=bitbuf;int loclen = bitlen
#define FillBits()   if( loclen<=bits(bitbuf)-8 ){\
	do{\
		locbuf |= (unsigned)(fgetc(frz) & 0xFF) << (BYSH - loclen);\
		loclen += 8;\
	}while (loclen <= bits(bitbuf) - 8);\
	if(feof(frz)) overrun++;\
}
#define KeepBits()  bitbuf = locbuf,bitlen = loclen
#define GetBit()    locbuf >> BISH, locbuf <<= 1, loclen--
#define FrzGetByte()   locbuf >> BYSH, locbuf <<= 8, loclen -= 8
#define GetNBits(n) locbuf >> (bits(bitbuf) - (n)),\
					locbuf <<= (n), loclen -= (n)

bool CFrzTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return ( siz>=2 && hdr[0]==0x1f
		&& ( hdr[1]==0x9e || hdr[1]==0x9f ) );
}

bool CFrzTool::Extract( const char* fname,const char* ddir )
{
	frz = fopen( fname,"rb" );
	if( frz==NULL )return false;
	int h1=fgetc(frz);
	int h2=fgetc(frz);
	if( h1!=0x1f || (h2!=0x9e && h2!=0x9f) )
	{fclose(frz);return false;}

	char oriname[MAX_PATH];
	const char* ext = kiutil::pathExt(fname);
	kiutil::getOriginalName( oriname,fname,
		0==stricmp(ext,"f")||0==stricmp(ext,"xf")||0==stricmp(ext,"frz")
		? NULL : "unfreeze" );

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(oriname)){fclose(frz); return true;}//追加 By Bonty

	out=fopen(oriname,"wb");

	if( h2==0x9e )melt1();
	else	melt2();

	fclose(out);
	fclose(frz);
	return true;
}

void CFrzTool::melt2()
{
	short i,j,k,r,c;

/* Huffman-dependent part */
	if( !read_header() )
		return;
	StartHuff(511);
	init(Table2);
/* end of Huffman-dependent part */

	InitIO();
	for( i=8192-256; i!=8192; i++ )
		window[i] = ' ';

	r=0;
	while( true )
	{
		c = DecodeChar();

		if( c==256 )
			break;
		else if( c<256 )
		{
			window[r++] = (unsigned char)c;
			r &= 0x1fff;
			if( !r )
				fwrite( window,1,8192,out );
		}
		else
		{
			i = (r - DecodePosition() - 1) & 0x1fff;
			j = c - 256 + 2;
			for( k=0; k!=j; k++ )
			{
				c = window[ (i+k)&0x1fff ];
				window[r++] = (unsigned char)c;
				r &= 0x1fff;
				if( !r )
					fwrite( window,1,8192,out );
			}
		}
	}
	if( r )
		fwrite( window,1,r,out );
}

void CFrzTool::melt1()
{
	short i,j,k,r,c;

	StartHuff(315);
	init(Table1);
	InitIO();

	for( i=4096-60; i!=4096; i++ )
		window[i] = ' ';

	r=0;
	while( true )
	{
		c =  DecodeChar();

		if( c==256 )
			break;
		else if( c<256 )
		{
			window[r++] = (unsigned char)c;
			r &= 0xfff;
			if( !r )
				fwrite( window,1,4096,out );
		}
		else
		{
			i = (r - DecodePOld() - 1) & 0xfff;
			j = c - 256 + 2;
			for( k=0; k!=j; k++ )
			{
				c = window[(i+k) & 0xfff];
				window[r++] = (unsigned char)c;
				r &= 0xfff;
				if( !r )
					fwrite( window,1,4096,out );
			}
		}
	}

	if( r )
		fwrite( window,1,r,out );
}

bool CFrzTool::read_header()
{
	int i, j;
	i = fgetc(frz) & 0xFF;
	i |= (fgetc(frz) & 0xFF) << 8;
	Table2[1] = i & 1; i >>= 1;
	Table2[2] = i & 3; i >>= 2;
	Table2[3] = i & 7; i >>= 3;
	Table2[4] = i & 0xF; i >>= 4;
	Table2[5] = i & 0x1F; i >>= 5;

	if (i & 1 || (i = fgetc(frz)) & 0xC0)
		return false;

	Table2[6] = i & 0x3F;

	i = Table2[1] + Table2[2] + Table2[3] + Table2[4] +
	Table2[5] + Table2[6];

	i = 62 - i;

	j = 128 * Table2[1] + 64 * Table2[2] + 32 * Table2[3] +
	16 * Table2[4] + 8 * Table2[5] + 4 * Table2[6];

	j = 256 - j;

	j -= i;
	if (j < 0 || i < j)
		return false;

	Table2[7] = j;
	Table2[8] = i - j;

	return true;
}

void CFrzTool::init(unsigned char* table)
{
	short i, j, k, num;
	num = 0;

	for(i = 1, j = 0; i <= 8; i++)
	{
		num += table[i] << (8 - i);
		for(k = table[i]; k; j++, k--)
			p_len[j] = (unsigned char)i;
	}
	if (num != 256)
		return;

	num = j;
	for(k = j = 0; j < num; j ++)
		for(i = 1 << (8 - p_len[j]); i--;)
			code[k++] = (unsigned char)j;
	for(k = j = 0; j < num; j ++)
		for(i = 1 << (8 - p_len[j]); i--;)
			d_len[k++] =  p_len[j] - 1 - (table == Table1);
}

void CFrzTool::StartHuff( int n_char )
{
	int i, j;
	tt = n_char * 2 - 1;
	rr = tt - 1;
	chars = n_char;

	for( i=0; i<n_char; i++ )
	{
		freq[i] = 1;
		son[i] = i + tt;
		prnt[i + tt] = i;
	}
	i=0; j=n_char;

	while( j<=rr )
	{
		freq[j] = freq[i] + freq[i+1];
		son[j] = i;
		prnt[i] = prnt[i+1] = j;
		i+=2; j++;
	}
	freq[tt] = 0xffff;
	prnt[rr] = 0;
}

void CFrzTool::reconst()
{
	int i, j, k;
	unsigned f;

	j = 0;
	for( i=0; i<tt; i++ )
	{
		if( son[i]>=tt )
		{
			freq[j] = (freq[i] + 1) / 2;
			son[j] = son[i];
			j++;
		}
	}

	for (i = 0, j = chars; j < tt; i += 2, j++)
	{
		k = i + 1;
		f = freq[j] = freq[i] + freq[k];
		for (k = j - 1; f < freq[k]; k--);
		k++;
		{
			register unsigned *p, *e;
			for (p = &freq[j], e = &freq[k]; p > e; p--)
				p[0] = p[-1];
			freq[k] = f;
		}
		{       register int *p, *e;
			for (p = &son[j], e = &son[k]; p > e; p--)
				p[0] = p[-1];
			son[k] = i;
		}
	}

	for (i = 0; i < tt; i++)
	{
		if ((k = son[i]) >= tt)
			prnt[k] = i;
		else
			prnt[k] = prnt[k + 1] = i;
	}
}

void CFrzTool::update(int c)
{
	unsigned k, *p;
	int i, j, l;

	if( freq[rr] == 0x8000 )
		reconst();

	c = prnt[c + tt];
	do
	{
		k = ++freq[c];

		if (k > freq[l = c + 1])
		{
			for (p = freq+l+1; k > *p++; );
			l = p - freq - 2;
			freq[c] = p[-2];
			p[-2] = k;

			i = son[c];
			prnt[i] = l;
			if (i < tt) prnt[i + 1] = l;

			j = son[l];
			son[l] = i;

			prnt[j] = c;
			if (j < tt) prnt[j + 1] = c;
			son[c] = j;

			c = l;
		}
	} while ((c = prnt[c]) != 0);
}

int CFrzTool::DecodeChar()
{
	int c = rr;
	DefBits;
	if( overrun>=sizeof(bitbuf) )
		return 256;
	FillBits();
	while( (c=son[c])<tt )
		c += GetBit();
	KeepBits();
	update( c-=tt );
	return c;
}

int CFrzTool::DecodePosition()
{
	int i, j;
	DefBits;
	FillBits();
	i = FrzGetByte();
	j = (code[i] << 7) | (i << d_len[i]) & 0x7F;
	j |= GetNBits(d_len[i]);
	KeepBits();
	return j;
}

short CFrzTool::DecodePOld()
{
	int i,j;
	DefBits;
	FillBits();
	i = FrzGetByte();
	j = (code[i] << 6) | (i << d_len[i]) & 0x3F;
	j |= GetNBits(d_len[i]);
	KeepBits();
	return j;
}
