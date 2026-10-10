
#include "stdafx.h"
#include "complib.h"

/*** de-compress(lzw) ***/

#define BITS 16
#define INIT_BITS 9
#define BIT_MASK 0x1f
#define BLOCK_MODE 0x80
#define FIRST 257
#define CLEAR 256
static bool lzw_readonce(LZWDEC* lzw);

LZWDEC* lzw_start( int bits,bool blockmode,lzw_in proc,unsigned long param )
{
	LZWDEC* lzw = new LZWDEC;
	lzw->m_pGet = proc;
	lzw->m_Param= param;

	lzw->m_insize = lzw->m_rsize = (*lzw->m_pGet)(lzw->m_inbuf,ZBUFSIZ,lzw->m_Param);

	lzw->m_maxbits = bits;
	lzw->m_block_mode = blockmode;
	if( lzw->m_maxbits>BITS ){lzw_end(lzw);return NULL;}

	lzw->m_eof = false;
	lzw->m_n_bits = INIT_BITS;
	lzw->m_maxcode = (1<<lzw->m_n_bits) - 1;
	lzw->m_oldcode = -1;
	lzw->m_finchar = 0;
	lzw->m_posbits = 0;
	lzw->m_free_ent = lzw->m_block_mode ? FIRST : 256;
	memset(lzw->m_codetab, 0, 256);
	for(int code=0; code!=256; code++)
		lzw->m_htab[code]= code;
	return lzw;
}

int lzw_read( void *buf,int size,LZWDEC* lzw )
{
	int nread = 0;
	while( !lzw->m_eof && size>0 )
	{
		int in_avail;
		in_avail = lzw->m_strstream.Len();
		if( in_avail==0 )
		{
			if( !lzw_readonce(lzw) )
				break;
		}
		in_avail = lzw->m_strstream.Len();
		int n = in_avail>size?size:in_avail;
		lzw->m_strstream.Get( buf,n );
		nread += n;
		size -= n;
		buf = (void*)( (char*)buf+n );
	}
	return nread;
}

static bool lzw_readonce(LZWDEC* lzw)
{
resetbuf:
	lzw->m_insize -= lzw->m_posbits/8;
	memmove( lzw->m_inbuf,lzw->m_inbuf+lzw->m_posbits/8,lzw->m_insize );
	lzw->m_posbits = 0;

	if( lzw->m_insize<sizeof(lzw->m_inbuf)-ZBUFSIZ )
	{
		lzw->m_rsize = (*lzw->m_pGet)( lzw->m_inbuf+lzw->m_insize,ZBUFSIZ,lzw->m_Param );
		lzw->m_insize += lzw->m_rsize;
	}
	int inbits;
	if(lzw->m_rsize>0)
		inbits = (lzw->m_insize-lzw->m_insize%lzw->m_n_bits)*8;
	else
		inbits = (lzw->m_insize*8)-(lzw->m_n_bits-1);

	while( inbits>lzw->m_posbits )
	{
		if(lzw->m_free_ent > lzw->m_maxcode)
		{
			lzw->m_posbits = (lzw->m_posbits-1)+((lzw->m_n_bits*8)-(lzw->m_posbits-1+(lzw->m_n_bits*8))%(lzw->m_n_bits*8));
			lzw->m_n_bits++;
			if(lzw->m_n_bits == lzw->m_maxbits)
				lzw->m_maxcode = 1 << lzw->m_maxbits;
			else
				lzw->m_maxcode = (1 << lzw->m_n_bits) - 1;
			goto resetbuf;
		}

		int code;
		{
			int bitmask = (1<<lzw->m_n_bits) - 1;
			code = (*(long*)(&lzw->m_inbuf[lzw->m_posbits/8]) >> (lzw->m_posbits&7)) & bitmask;
			lzw->m_posbits += lzw->m_n_bits;
		}

		if( lzw->m_oldcode==-1 )
		{
			lzw->m_strstream.Putc((unsigned char)code);
			lzw->m_oldcode = code;
			lzw->m_finchar = (int) code;
			continue;
		}
		if( code==CLEAR && lzw->m_block_mode )
		{
			memset(lzw->m_codetab, 0, 256);
			lzw->m_free_ent = FIRST - 1;
			int tmp = (lzw->m_posbits-1) + (lzw->m_n_bits*8);
			lzw->m_posbits = tmp - tmp%(lzw->m_n_bits*8);
			lzw->m_n_bits = INIT_BITS;
			lzw->m_maxcode = (1 << lzw->m_n_bits) - 1;
			goto resetbuf;
		}
		int incode = code;
		unsigned char *stackp = lzw->m_htab + sizeof(lzw->m_htab);
		if( code>lzw->m_free_ent )
			return false;
		else if( code==lzw->m_free_ent )
		{
			* --stackp = (unsigned char)lzw->m_finchar;
			code = lzw->m_oldcode;
		}
		while( (unsigned int)code>=(unsigned)256 )
		{
			*--stackp	= lzw->m_htab[code];
			code		= lzw->m_codetab[code];
		}
		* --stackp = (unsigned char)(lzw->m_finchar = lzw->m_htab[code] );

		lzw->m_strstream.Put( (char*)stackp, lzw->m_htab + sizeof(lzw->m_htab) - stackp);
		if( lzw->m_free_ent < (1<<lzw->m_maxbits) )
		{
			lzw->m_codetab[lzw->m_free_ent]	= (unsigned short)lzw->m_oldcode;
			lzw->m_htab[lzw->m_free_ent]		= lzw->m_finchar;
			lzw->m_free_ent ++;
		}
		lzw->m_oldcode = incode;
	}

	if( lzw->m_rsize<=0 )
		lzw->m_eof=true;
	return true;
}

static int Z_in(void* buf,int size,unsigned long param)
{
	ZFILE* fp=(ZFILE*)param;
	size=fread(buf,1,size,fp->m_pFile);
	return size==-1 ? 0 : size;
}

ZFILE* Z_open(const char* name,const char* mode)
{
	ZFILE* fp = new ZFILE;
	fp->m_pFile = fopen(name,"rb");
	if( fp->m_pFile==NULL ){delete fp;return NULL;}

	unsigned char tmp[3];
	if( 3!=fread(tmp,1,3,fp->m_pFile) )
	{Z_close(fp);return NULL;}
	if( tmp[0]!=0x1f || tmp[1]!=0x9d ){Z_close(fp);return NULL;}

	fp->lzw=lzw_start( tmp[2]&BIT_MASK, (tmp[2]&BLOCK_MODE)!=0, Z_in, (unsigned long)fp );
	if( fp->lzw==NULL ){Z_close(fp);return NULL;}
	return fp;
}

int Z_close(ZFILE* fp)
{
	if( fp==NULL )
		return EOF;
	lzw_end( fp->lzw );
	fclose( fp->m_pFile );
	delete fp;
	return 0;
}

int Z_read( void *buf,int size,ZFILE* fp )
{
	return lzw_read(buf,size,fp->lzw);
}

/*** de-pack ***/

#define MIN(a,b) ( (a)<=(b)?(a):(b) )
#define look_bits(code,bits,mask) \
{ \
  while (fp->valid < (bits)) fp->bitbuf = (fp->bitbuf<<8) | (DWORD)fgetc(fp->m_pFile), fp->valid += 8; \
  code = (fp->bitbuf >> (fp->valid-(bits))) & (mask); \
}
#define skip_bits(bits)  (fp->valid -= (bits))
#define clear_bitbuf() (fp->valid = 0, fp->bitbuf = 0)

static bool read_tree(PFILE* fp)
{
	int len;
	int base;
	int n;

	fp->orig_len = 0;
	for( n=1; n<=4; n++ )
		fp->orig_len = (fp->orig_len << 8) | (DWORD)fgetc(fp->m_pFile);

	fp->max_len = fgetc(fp->m_pFile);
	if (fp->max_len > MAX_BITLEN)
		return false;

    n = 0;
	for( len=1; len<=fp->max_len; len++ )
	{
		fp->leaves[len] = fgetc(fp->m_pFile);
		n += fp->leaves[len];
	}
    if( n>LITERALS )
		return false;

	fp->leaves[fp->max_len]++;

	base = 0;
	for( len=1; len<=fp->max_len; len++ )
	{
		fp->lit_base[len] = base;
		for( n=fp->leaves[len]; n>0; n-- )
			fp->literal[base++] = fgetc(fp->m_pFile);
    }
	fp->leaves[fp->max_len]++;
	return true;
}

static void build_tree(PFILE* fp)
{
	int nodes = 0;
	int len;
	BYTE *prefixp;

	for( len=fp->max_len; len>=1; len-- )
	{
		nodes >>= 1;
		fp->parents[len] = nodes;

		fp->lit_base[len] -= nodes;

		nodes += fp->leaves[len];
	}

	fp->peek_bits = MIN(fp->max_len, MAX_PEEK);
	prefixp = &fp->prefix_len[1<<fp->peek_bits];
	for( len=1; len<=fp->peek_bits; len++ )
	{
		int prefixes = fp->leaves[len] << (fp->peek_bits-len);
		while (prefixes--) *--prefixp = (BYTE)len;
	}
	while (prefixp > fp->prefix_len) *--prefixp = 0;
}

PFILE* P_open(const char* name,const char* mode)
{
	PFILE* fp = new PFILE;
	fp->m_pFile = fopen(name,"rb");
	if( fp->m_pFile==NULL ){delete fp;return NULL;}

	BYTE a=fgetc(fp->m_pFile);
	BYTE b=fgetc(fp->m_pFile);
	if( a!=0x1f || b!=0x1e )
	{
		P_close(fp);
		return NULL;
	}

    if( !read_tree(fp) ){delete fp;return NULL;}
	build_tree(fp);
    clear_bitbuf();

	fp->peek_mask = (1<<fp->peek_bits)-1;
	fp->eob = fp->leaves[fp->max_len]-1;
	fp->m_eof=false;

	return fp;
}

int P_close(PFILE* fp)
{
	if( fp==NULL || fp->m_pFile==NULL )
		return EOF;
	fclose( fp->m_pFile );
	delete fp;
	return 0;
}

static bool P_readonce(PFILE* fp);

int P_read( void *buf,int size,PFILE* fp )
{
	int nread = 0;
	while( size>0 )
	{
		int in_avail;
		in_avail = fp->m_strstream.Len();
		if( in_avail==0 )
		{
			if( !P_readonce(fp) )
				break;
		}
		in_avail = fp->m_strstream.Len();
		int n = in_avail>size?size:in_avail;
		fp->m_strstream.Get( buf,n );
		nread += n;
		size -= n;
		buf = (void*)( (char*)buf+n );
	}
	return nread;
}

static bool P_readonce(PFILE* fp)
{
	if( fp->m_eof )
		return false;

	int count=30000;
	while( count-- )
	{
		int len;
		register unsigned peek;

		look_bits(peek, fp->peek_bits, fp->peek_mask);
		len = fp->prefix_len[peek];
		if( len>0 )
			peek >>= fp->peek_bits - len;
		else
		{
			DWORD mask = fp->peek_mask;
			len = fp->peek_bits;
			do
			{
				len++, mask = (mask<<1)+1;
				look_bits(peek, len, mask);
		    }while( peek<(unsigned)fp->parents[len] );
		}
		if( peek==fp->eob && len==fp->max_len )
		{	fp->m_eof=true;	break;	}
		fp->m_strstream.Putc( fp->literal[peek+fp->lit_base[len]] );
		skip_bits(len);
	}
	return true;
}
