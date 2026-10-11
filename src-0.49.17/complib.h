
//
// original source is
//
// Tar32.dll 2.xx
//   http://openlab.ring.gr.jp/tsuneo/tar32/
//
// and
//
// unpack.c ( 4/21/90 )
//   http://www.ifi.uio.no/~in383/src/pack/
//

#ifndef COMPLIB
#define COMPLIB

/*** strstream ***/

class CMyStrBuf
{
protected:
	BYTE* buf;
	int st,ed,alen;
public:
	CMyStrBuf(){buf=new BYTE[alen=30000];st=ed=0;};
	virtual ~CMyStrBuf(){delete [] buf;};
	void Reset(){st=ed=0;};

	int Len(){return ed-st;}
	void Get( void* dat,int size )
	{
		memcpy(dat,buf+st,size);
		st+=size;
		if( ed-st<=0 )st=ed=0;
	};
	void Putc( BYTE c ){Put(&c,1);};// Oh! (^^;
	void Put( void* dat,int size )
	{
		if( alen<ed+size )
		{
			BYTE* tmp = new BYTE[alen=((ed+size)<<1)];
			memcpy( tmp,buf,ed );
			delete [] buf;
			buf = tmp;
		}
		memcpy( buf+ed,dat,size );
		ed+=size;
	};
};

/*** De-Compress(lzw) ***/

// low level routines
#define ZBUFSIZ 30000
typedef int (*lzw_in)(void* buf,int size,unsigned long param);

struct LZWDEC
{
	lzw_in m_pGet;
	unsigned long m_Param;

	int  m_maxbits;
	WORD m_codetab[1<<17];
	BYTE m_htab[1<<17];
	BYTE m_inbuf[ZBUFSIZ+64];
	bool m_eof;

	int m_finchar;
	int m_oldcode;
	int m_posbits;
	int m_free_ent;
	int m_maxcode;
	int m_n_bits;
	int m_rsize;
	int m_insize;
	bool m_block_mode;

	CMyStrBuf m_strstream;
};

LZWDEC* lzw_start( int bits,bool blockmode,lzw_in proc,unsigned long param );
int     lzw_read( void *buf,int size,LZWDEC* lzw );
#define lzw_end(lzw) delete(lzw)

// Z(UNIX-compressed) file io
struct ZFILE
{
	FILE *m_pFile;
	LZWDEC *lzw;
};
ZFILE* Z_open(const char* name,const char* mode);
int Z_close(ZFILE* fp);
int Z_read( void *buf,int size,ZFILE* fp );



/** UN-PACK ***************************************************/

#define MAX_BITLEN 25
#define LITERALS 256
#define MAX_PEEK 12

struct PFILE
{
	FILE *m_pFile;

	DWORD orig_len;
	int max_len;
	BYTE literal[LITERALS];
	int lit_base[MAX_BITLEN+1];
	int leaves [MAX_BITLEN+1];
	int parents[MAX_BITLEN+1];
	int peek_bits;

	BYTE prefix_len[16384];

	DWORD bitbuf;
	int valid;

	unsigned peek_mask;
	unsigned eob;

	CMyStrBuf m_strstream;

	bool m_eof;
};

int P_close(PFILE* fp);
int P_read( void *buf,int size,PFILE* fp );
PFILE* P_open(const char* name,const char* mode);

#endif
