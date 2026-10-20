// MsCompress.cpp: decoders for the COMPRESS.EXE / EXPAND.EXE formats.
//
// Written from the published format description of the SZDD and KWAJ
// file formats, so that the LZ32 system DLL is not needed.
//
// SZDD header (14 bytes)
//   0..7   "SZDD" signature 53 5A 44 44 88 F0 27 33
//   8      compression mode, only "A" (0x41) is valid
//   9      the character missing from the end of the file name
//   10..13 the length of the file when unpacked (32 bit LE)
//
// SZDD, QBasic variant header (12 bytes)
//   0..7   "SZ" signature 53 5A 20 88 F0 27 33 D1
//   8..11  the length of the file when unpacked (32 bit LE)
//
// KWAJ header (14 bytes)
//   0..7   "KWAJ" signature 4B 57 41 4A 88 F0 27 D1
//   8..9   the compression method (0..4)
//   10..11 the file offset of the compressed data
//   12..13 the header extension flags
//   the header extensions follow, and the data is at the file offset.
//
// The data of both formats is LZSS: a control byte with eight flags,
// least significant bit first; a set flag is a literal byte, a clear
// flag is a two byte match holding an absolute position in the 4096
// byte window and a four bit length.  The window starts full of spaces
// and the write position starts at 4096-16 (4096-18 for the QBasic
// variants).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "zlib/zlib.h"
#include "MsCompress.h"

#define MSC_WINDOW_SIZE   4096
#define MSC_WINDOW_MASK   (MSC_WINDOW_SIZE-1)
#define MSC_WINDOW_FILL   0x20
#define MSC_START_EXPAND  (MSC_WINDOW_SIZE-16)
#define MSC_START_QBASIC  (MSC_WINDOW_SIZE-18)

static const unsigned char MSC_SIG_SZDD[8] =
	{ 0x53,0x5A,0x44,0x44,0x88,0xF0,0x27,0x33 };
static const unsigned char MSC_SIG_QBASIC[8] =
	{ 0x53,0x5A,0x20,0x88,0xF0,0x27,0x33,0xD1 };
static const unsigned char MSC_SIG_KWAJ[8] =
	{ 0x4B,0x57,0x41,0x4A,0x88,0xF0,0x27,0xD1 };

// The LZSS decompressor, shared by the SZDD formats and by the KWAJ
// method 2.  "start" is where the write position begins.
static int MsLzssDecode( FILE *in, FILE *out, unsigned long origsize,
						 int start )
{
	unsigned char window[MSC_WINDOW_SIZE];
	unsigned long written = 0;
	int pos = start;
	int flag, bit;
	int ch, mpos, mlen;

	memset( window, MSC_WINDOW_FILL, sizeof(window) );

	for(;;) {
		int eof = 0;

		flag = fgetc( in );
		if( flag==EOF ) break;

		for( bit=0; bit<8; bit++ ) {
			if( flag & (1<<bit) ) {
				/* a literal */
				ch = fgetc( in );
				if( ch==EOF ) { eof = 1; break; }
				window[pos] = (unsigned char)ch;
				if( fputc(ch,out)==EOF ) return MSCOMPRESS_ERR_WRITE;
				pos = (pos+1) & MSC_WINDOW_MASK;
				written++;
			} else {
				/* a match */
				mpos = fgetc( in );
				if( mpos==EOF ) { eof = 1; break; }
				mlen = fgetc( in );
				if( mlen==EOF ) { eof = 1; break; }
				mpos |= (mlen & 0xf0) << 4;
				mlen = (mlen & 0x0f) + 3;
				while( mlen-- ) {
					ch = window[mpos];
					window[pos] = (unsigned char)ch;
					if( fputc(ch,out)==EOF ) return MSCOMPRESS_ERR_WRITE;
					pos = (pos+1) & MSC_WINDOW_MASK;
					mpos = (mpos+1) & MSC_WINDOW_MASK;
					written++;
				}
			}
			if( origsize && written>origsize ) return MSCOMPRESS_ERR_DATA;
		}
		/* the last control byte does not always carry eight commands:
		   the stream simply ends when the input does */
		if( eof ) break;
	}

	if( origsize && written!=origsize ) return MSCOMPRESS_ERR_DATA;
	return MSCOMPRESS_OK;
}

// KWAJ method 4, MS-ZIP: every block of up to 32768 bytes is a raw
// DEFLATE stream of its own, but the history window is shared between
// the blocks, so they have to be unpacked in order.
// KWAJ method 4: the data is a deflate stream in blocks of 32 KB
// that share their history.  Every block starts with its length in
// two bytes and the "CK" signature, and the length is the number of
// deflate bytes that follow.
static int MsMszipDecode( FILE *in, FILE *out, unsigned long origsize )
{
	unsigned long written = 0;
	z_stream s;
	int err = MSCOMPRESS_OK;

	memset( &s, 0, sizeof(s) );
	if( inflateInit2( &s, -MAX_WBITS )!=Z_OK ) return MSCOMPRESS_ERR_DATA;

	for(;;) {
		unsigned char head[4];
		unsigned int clen;
		unsigned char *inbuf;
		unsigned char *outbuf;
		unsigned long outlen;

		if( fread(head,1,4,in)!=4 ) break;
		clen = (unsigned int)head[0] | ((unsigned int)head[1]<<8);
		if( clen==0 ) break;
		if( head[2]!='C' || head[3]!='K' ) { err = MSCOMPRESS_ERR_DATA; break; }
		if( clen<3 ) { err = MSCOMPRESS_ERR_DATA; break; }

		inbuf = (unsigned char*)malloc(clen+4);
		outbuf = (unsigned char*)malloc(32768+64);
		if( !inbuf || !outbuf ) {
			free(inbuf); free(outbuf); err = MSCOMPRESS_ERR_DATA; break;
		}
		if( fread(inbuf,1,clen,in)!=clen ) {
			free(inbuf); free(outbuf); err = MSCOMPRESS_ERR_READ; break;
		}

		/* the window of the block before is kept, which is what
		   makes the blocks of a folder better than plain deflate.
		   inflateReset would drop it, so the "keep" version is the
		   one that is wanted here. */
		inflateResetKeep( &s );
		s.next_in = inbuf;
		s.avail_in = clen;
		s.next_out = outbuf;
		s.avail_out = 32768+64;
		/* the result of the inflater is not the error code: it is
		   Z_STREAM_END when the block was read to the end */
		{
			int zret = inflate( &s, Z_FINISH );
			outlen = (32768+64) - s.avail_out;
			free(inbuf);
			if( zret!=Z_STREAM_END ) { free(outbuf); err = MSCOMPRESS_ERR_DATA; break; }
		}
		if( fwrite(outbuf,1,outlen,out)!=outlen ) { free(outbuf); err = MSCOMPRESS_ERR_WRITE; break; }

		written += outlen;
		if( origsize && written>origsize ) { free(outbuf); err = MSCOMPRESS_ERR_DATA; break; }
		free(outbuf);
	}

	inflateEnd( &s );
	if( err==MSCOMPRESS_OK && origsize && written!=origsize )
		err = MSCOMPRESS_ERR_DATA;
	return err;
}

// KWAJ methods 0 and 1: the data is stored as it is, or XORed with
// 0xFF.
static int MsStoredDecode( FILE *in, FILE *out, unsigned long origsize,
						   int invert )
{
	unsigned long done = 0;
	int ch;

	while( done<origsize ) {
		ch = fgetc( in );
		if( ch==EOF ) return MSCOMPRESS_ERR_READ;
		if( invert ) ch ^= 0xFF;
		if( fputc(ch,out)==EOF ) return MSCOMPRESS_ERR_WRITE;
		done++;
	}
	return MSCOMPRESS_OK;
}

int MsCompressReadHeader( FILE *in, MsCompressHeader *h )
{
	unsigned char buf[16];
	unsigned long pos;

	h->format = MSCOMPRESS_NONE;
	h->method = 0;
	h->origsize = 0;
	h->dataoffset = 0;
	h->missingchar = 0;

	pos = (unsigned long)ftell( in );
	if( fread(buf,1,8,in)!=8 ) return MSCOMPRESS_ERR_SIGNATURE;

	if( memcmp(buf,MSC_SIG_SZDD,8)==0 ) {
		if( fread(buf,1,6,in)!=6 ) return MSCOMPRESS_ERR_READ;
		if( buf[0]!=0x41 ) return MSCOMPRESS_ERR_DATA;
		h->format = MSCOMPRESS_SZDD;
		h->missingchar = (char)buf[1];
		h->origsize = (unsigned long)buf[2] | ((unsigned long)buf[3]<<8) |
					   ((unsigned long)buf[4]<<16) | ((unsigned long)buf[5]<<24);
		h->dataoffset = 14;
	} else if( memcmp(buf,MSC_SIG_QBASIC,8)==0 ) {
		if( fread(buf,1,4,in)!=4 ) return MSCOMPRESS_ERR_READ;
		h->format = MSCOMPRESS_SZDD_QBASIC;
		h->origsize = (unsigned long)buf[0] | ((unsigned long)buf[1]<<8) |
					   ((unsigned long)buf[2]<<16) | ((unsigned long)buf[3]<<24);
		h->dataoffset = 12;
	} else if( memcmp(buf,MSC_SIG_KWAJ,8)==0 ) {
		unsigned int flags;
		unsigned int skip = 0;

		if( fread(buf,1,6,in)!=6 ) return MSCOMPRESS_ERR_READ;
		h->format = MSCOMPRESS_KWAJ;
		h->method = (int)(buf[0] | (buf[1]<<8));
		h->dataoffset = (unsigned long)(buf[2] | (buf[3]<<8));
		flags = (unsigned int)(buf[4] | (buf[5]<<8));

		/* the header extensions, in this order */
		if( flags & 0x01 ) {		/* decompressed length */
			unsigned char l[4];
			if( fread(l,1,4,in)!=4 ) return MSCOMPRESS_ERR_READ;
			h->origsize = (unsigned long)l[0] | ((unsigned long)l[1]<<8) |
						   ((unsigned long)l[2]<<16) | ((unsigned long)l[3]<<24);
			skip += 4;
		}
		if( flags & 0x02 ) skip += 2;	/* unknown */
		if( flags & 0x04 ) {		/* length + data */
			unsigned char l[2];
			if( fread(l,1,2,in)!=2 ) return MSCOMPRESS_ERR_READ;
			skip += 2 + (unsigned int)(l[0] | (l[1]<<8));
		}
		if( flags & 0x08 ) {		/* file name, at most 8 characters */
			unsigned char l[9];
			unsigned int n = 0;
			if( fread(l,1,9,in)!=9 ) return MSCOMPRESS_ERR_READ;
			while( n<9 && l[n] ) n++;
			skip += n+1;
		}
		if( flags & 0x10 ) {		/* file extension, at most 3 */
			unsigned char l[4];
			unsigned int n = 0;
			if( fread(l,1,4,in)!=4 ) return MSCOMPRESS_ERR_READ;
			while( n<4 && l[n] ) n++;
			skip += n+1;
		}
		if( flags & 0x20 ) {		/* length + data */
			unsigned char l[2];
			if( fread(l,1,2,in)!=2 ) return MSCOMPRESS_ERR_READ;
			skip += 2 + (unsigned int)(l[0] | (l[1]<<8));
		}
		/* the data is at the offset the header gives, so the
		   extensions only have to be stepped over */
		if( fseek(in,(long)(pos+14+skip),SEEK_SET)!=0 )
			return MSCOMPRESS_ERR_READ;
	} else {
		return MSCOMPRESS_ERR_SIGNATURE;
	}

	fseek( in, (long)h->dataoffset, SEEK_SET );
	return MSCOMPRESS_OK;
}

// COMPRESS.EXE replaced the last character of the name with an
// underscore.  The character that was there is in the header, but
// when it is not known the common extensions can still be worked out.
static const char *MsExtTable[] = {
	"ex","e", "co","m", "sc","r", "dl","l", "oc","x", "dr","v",
	"vx","d", "38","6", "sy","s", "cp","l", "li","b", "tt","f",
	"ch","m", "hl","p", "cn","t", "da","t", "tx","t", "wr","i",
	"wa","v", "mi","d", "rm","i", "bm","p", "rl","e", "cu","r",
	"do","c", "ic","o", "re","g", "rt","f", "ht","m", NULL, NULL
};

// ------------------------------------------------------------------
// KWAJ method 3, the LZ+Huffman variant.
//
// The data starts with six nybbles; the first five are the encoding
// type of the five Huffman length lists that follow and the sixth is
// padding.  Then come the five length lists, and then the compressed
// data.
//
// The five trees, in order, are
//   16 symbols  match run lengths (MATCHLEN)
//   16 symbols  match run lengths straight after a short literal run
//               (MATCHLEN2)
//   32 symbols  literal run lengths (LITLEN)
//   64 symbols  the upper six bits of a match distance (OFFSET)
//  256 symbols  the literals (LITERAL)
// and the codes are canonical, so a symbol is identified by how many
// symbols of that length there are and by the length of each symbol.
//
// The ring buffer is 4096 bytes of spaces; unlike the SZDD formats
// the starting position does not matter, because the match distances
// are stored relative to the current position.

#define MSC_KWAJ_WINDOW   4096
#define MSC_KWAJ_FILL     0x20
#define MSC_KWAJ_START    (MSC_KWAJ_WINDOW-17)

#define MSC_KWAJ_MATCHLEN   0
#define MSC_KWAJ_MATCHLEN2  1
#define MSC_KWAJ_LITLEN     2
#define MSC_KWAJ_OFFSET     3
#define MSC_KWAJ_LITERAL    4
#define MSC_KWAJ_TREES      5

static const int MsKwajTreeSize[MSC_KWAJ_TREES] = { 16, 16, 32, 64, 256 };

// a canonical Huffman decoder
typedef struct
{
	int count[17];		// symbols of each length
	int firstcode[17];	// the first code of each length
	int firstindex[17];	// where those symbols start
	int symbol[256];	// the symbols, ordered by length
	int maxlen;
} MsHuff;

// a bit reader, most significant bit first
typedef struct
{
	FILE *in;
	unsigned int bitbuf;
	int bitcount;
	int eof;
} MsBits;

static void MsBitsInit( MsBits *b, FILE *in )
{
	b->in = in;
	b->bitbuf = 0;
	b->bitcount = 0;
	b->eof = 0;
}

static int MsBitsGet( MsBits *b )
{
	int bit;

	if( b->bitcount==0 )
	{
		int c = fgetc( b->in );
		if( c==EOF ) { b->eof = 1; return -1; }
		b->bitbuf = (unsigned int)c;
		b->bitcount = 8;
	}
	bit = (int)((b->bitbuf >> (b->bitcount-1)) & 1);
	b->bitcount--;
	return bit;
}

static int MsBitsGetBits( MsBits *b, int n )
{
	int v = 0;
	while( n-- > 0 )
	{
		int bit = MsBitsGet( b );
		if( bit<0 ) return -1;
		v = (v<<1) | bit;
	}
	return v;
}

// build the canonical code tables from the symbol lengths
static void MsHuffBuild( MsHuff *h, unsigned char *lengths, int n )
{
	int i, l, code;

	memset( h->count, 0, sizeof(h->count) );
	h->maxlen = 0;
	for( i=0; i<n; i++ )
	{
		if( lengths[i]>16 ) lengths[i] = 16;
		h->count[lengths[i]]++;
		if( lengths[i]>h->maxlen ) h->maxlen = lengths[i];
	}
	code = 0;
	for( l=1; l<=16; l++ )
	{
		h->firstcode[l] = code;
		h->firstindex[l] = 0;
		code += h->count[l];
		code <<= 1;
	}
	// the symbols in canonical order: by length, then by symbol
	{
		int index = 0;
		for( l=1; l<=16; l++ )
		{
			h->firstindex[l] = index;
			for( i=0; i<n; i++ )
				if( lengths[i]==l ) h->symbol[index++] = i;
		}
	}
}

static int MsHuffDecode( MsHuff *h, MsBits *b )
{
	int len = 0, code = 0;

	for(;;) {
		int bit = MsBitsGet( b );
		if( bit<0 ) return -1;
		code = (code<<1) | bit;
		len++;
		if( len>16 || len>h->maxlen ) return -1;
		if( h->count[len] ) {
			int index = code - h->firstcode[len];
			if( index>=0 && index<h->count[len] )
				return h->symbol[h->firstindex[len]+index];
		}
	}
}

// read one of the five length lists
static int MsKwajReadLengths( MsBits *b, int type, unsigned char *lengths, int n )
{
	int i, prev;

	if( type==0 )
	{
		// every symbol has the same length, implied by the tree size
		int len = 0;
		if( n==16 ) len = 4;
		else if( n==32 ) len = 5;
		else if( n==64 ) len = 6;
		else if( n==256 ) len = 8;
		else return 0;
		for( i=0; i<n; i++ ) lengths[i] = (unsigned char)len;
		return 1;
	}

	prev = MsBitsGetBits( b, 4 );
	if( prev<0 ) return 0;
	lengths[0] = (unsigned char)prev;

	if( type==3 )
	{
		// no run length encoding: four bits per symbol
		for( i=1; i<n; i++ )
		{
			prev = MsBitsGetBits( b, 4 );
			if( prev<0 ) return 0;
			lengths[i] = (unsigned char)prev;
		}
		return 1;
	}

	for( i=1; i<n; i++ )
	{
		if( type==1 )
		{
			int bit = MsBitsGet( b );
			if( bit<0 ) return 0;
			if( bit==0 ) {
				// the same as the previous one
			} else {
				bit = MsBitsGet( b );
				if( bit<0 ) return 0;
				if( bit==0 ) {
					prev = prev + 1;			// one longer
				} else {
					prev = MsBitsGetBits( b, 4 );
					if( prev<0 ) return 0;
				}
			}
		}
		else if( type==2 )
		{
			int sel = MsBitsGetBits( b, 2 );
			if( sel<0 ) return 0;
			if( sel==3 ) {
				prev = MsBitsGetBits( b, 4 );
				if( prev<0 ) return 0;
			} else {
				prev = prev + (sel-1);		// -1, 0 or +1
			}
		}
		else return 0;
		if( prev<0 || prev>15 ) return 0;
		lengths[i] = (unsigned char)prev;
	}
	return 1;
}

static int MsKwajDecode( FILE *in, FILE *out, unsigned long origsize )
{
	unsigned char window[MSC_KWAJ_WINDOW];
	unsigned char lengths[256];
	MsHuff tree[MSC_KWAJ_TREES];
	int types[MSC_KWAJ_TREES];
	MsBits bits;
	unsigned long written = 0;
	int pos = MSC_KWAJ_START;
	int selected = MSC_KWAJ_MATCHLEN;
	int t;

	MsBitsInit( &bits, in );
	memset( window, MSC_KWAJ_FILL, sizeof(window) );

	// the six nybbles come first and all six have to be read, even
	// though only the first five are used: they are the encoding type
	// of each of the five length lists, and the sixth is padding
	for( t=0; t<6; t++ )
	{
		int type = MsBitsGetBits( &bits, 4 );
		if( type<0 ) return MSCOMPRESS_ERR_DATA;
		if( t<MSC_KWAJ_TREES )
		{
			if( type>3 ) return MSCOMPRESS_ERR_DATA;
			types[t] = type;
		}
	}
	// and only then the five length lists
	for( t=0; t<MSC_KWAJ_TREES; t++ )
	{
		if( !MsKwajReadLengths( &bits, types[t], lengths, MsKwajTreeSize[t] ) )
			return MSCOMPRESS_ERR_DATA;
		MsHuffBuild( &tree[t], lengths, MsKwajTreeSize[t] );
	}

	for(;;) {
		int code = MsHuffDecode( &tree[selected], &bits );
		if( code<0 ) break;			// end of the input
		if( code>0 )
		{
			// a match
			int x = MsHuffDecode( &tree[MSC_KWAJ_OFFSET], &bits );
			int y, len, dist;
			if( x<0 ) return MSCOMPRESS_ERR_DATA;
			y = MsBitsGetBits( &bits, 6 );
			if( y<0 ) return MSCOMPRESS_ERR_DATA;
			len = code + 2;
			dist = (x<<6) | y;
			while( len-- > 0 )
			{
				int ch = window[(pos - dist) & (MSC_KWAJ_WINDOW-1)];
				window[pos] = (unsigned char)ch;
				if( fputc(ch,out)==EOF ) return MSCOMPRESS_ERR_WRITE;
				pos = (pos+1) & (MSC_KWAJ_WINDOW-1);
				written++;
			}
			selected = MSC_KWAJ_MATCHLEN;
		}
		else
		{
			// a run of literals
			int x = MsHuffDecode( &tree[MSC_KWAJ_LITLEN], &bits );
			int n;
			if( x<0 ) return MSCOMPRESS_ERR_DATA;
			n = x + 1;
			selected = ( x!=31 ) ? MSC_KWAJ_MATCHLEN2 : MSC_KWAJ_MATCHLEN;
			while( n-- > 0 )
			{
				int ch = MsHuffDecode( &tree[MSC_KWAJ_LITERAL], &bits );
				if( ch<0 ) return MSCOMPRESS_ERR_DATA;
				window[pos] = (unsigned char)ch;
				if( fputc(ch,out)==EOF ) return MSCOMPRESS_ERR_WRITE;
				pos = (pos+1) & (MSC_KWAJ_WINDOW-1);
				written++;
			}
		}
		if( origsize && written>origsize ) return MSCOMPRESS_ERR_DATA;
	}

	if( origsize && written!=origsize ) return MSCOMPRESS_ERR_DATA;
	return MSCOMPRESS_OK;
}

int MsCompressMakeName( const char *arcname, char missingchar,
						char *out, int outsz )
{
	const char *base = arcname;
	const char *p;
	char name[512];
	char *ext;
	int i;
	int len;

	/* only the file name part of the path is used */
	for( p=arcname; *p; p++ ) {
		if( *p=='\\' || *p=='/' ) base = p+1;
	}
	len = (int)strlen(base);
	if( len<1 || len>510 ) return 0;
	memcpy( name, base, (size_t)len );
	name[len] = 0;

	/* the underscore that COMPRESS.EXE put at the end */
	if( len>1 && (name[len-1]=='_' || name[len-1]=='$') )
		name[len-1] = 0;

	/* the character that was replaced */
	if( missingchar ) {
		len = (int)strlen(name);
		if( len<510 ) { name[len] = missingchar; name[len+1] = 0; }
	}

	/* and the common extensions, for when the character is not known.
	   "ext" points at the dot, so the two characters of the extension
	   are ext[1] and ext[2] and the name ends at ext[3] */
	ext = strrchr( name, '.' );
	if( ext && ext[1] && ext[2] && !ext[3] ) {
		int found = 0;
		if( ext[1]>='A' && ext[1]<='Z' ) ext[1] = (char)(ext[1]+'a'-'A');
		if( ext[2]>='A' && ext[2]<='Z' ) ext[2] = (char)(ext[2]+'a'-'A');
		for( i=0; MsExtTable[i]; i+=2 ) {
			if( 0==_stricmp(ext+1,MsExtTable[i]) ) {
				strcat( ext+1, MsExtTable[i+1] );
				found = 1;
				break;
			}
		}
		/* an extension that is not in the table gets the same
		   treatment the original code gave it */
		if( !found ) ext[2] = '#';
	}

	len = (int)strlen(name);
	if( len<1 || len>=outsz ) return 0;
	memcpy( out, name, (size_t)len+1 );
	return len;
}

// The name of the compression method, for the listing.  It has to fit
// in the eight bytes that the member information has room for.
const char *MsCompressModeName( const MsCompressHeader *h )
{
	switch( h->format )
	{
	case MSCOMPRESS_SZDD:
		return "SZDD";
	case MSCOMPRESS_SZDD_QBASIC:
		return "SZDD-QB";
	case MSCOMPRESS_KWAJ:
		switch( h->method )
		{
		case 0:		return "KWAJ-STO";
		case 1:		return "KWAJ-XOR";
		case 2:		return "KWAJ-LZ";
		case 3:		return "KWAJ-HUF";
		case 4:		return "KWAJ-ZIP";
		}
		return "KWAJ";
	}
	return "";
}

int MsCompressDecode( FILE *in, FILE *out, const MsCompressHeader *h )
{
	switch( h->format )
	{
	case MSCOMPRESS_SZDD:
		return MsLzssDecode( in, out, h->origsize, MSC_START_EXPAND );
	case MSCOMPRESS_SZDD_QBASIC:
		return MsLzssDecode( in, out, h->origsize, MSC_START_QBASIC );
	case MSCOMPRESS_KWAJ:
		switch( h->method )
		{
		case 0:
			return MsStoredDecode( in, out, h->origsize, 0 );
		case 1:
			return MsStoredDecode( in, out, h->origsize, 1 );
		case 2:
			return MsLzssDecode( in, out, h->origsize, MSC_START_QBASIC );
		case 3:
			return MsKwajDecode( in, out, h->origsize );
		case 4:
			return MsMszipDecode( in, out, h->origsize );
		default:
			return MSCOMPRESS_ERR_UNSUPPORTED;
		}
	}
	return MSCOMPRESS_ERR_SIGNATURE;
}
