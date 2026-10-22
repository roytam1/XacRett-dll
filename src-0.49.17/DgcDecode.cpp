#include "stdafx.h"
#include "DgcCrypt.h"
#include "DgcDecode.h"
#include "zlib/zlib.h"
#include <stdlib.h>

// ------------------------------------------------------------------
// The container
// ------------------------------------------------------------------

#define DGC_CHUNK_HDR	32
#define DGC_MAX_CHUNK	64

struct DgcChunk
{
	unsigned long	off;		// the offset of the header
	unsigned long	psize;		// the size of the payload
	int				depth;
	char			id[5];
};

struct DgcWalk
{
	unsigned char*	img;
	unsigned long	size;
	DgcChunk		chunks[DGC_MAX_CHUNK];
	int				nchunk;
	int				broken;
};

static unsigned long DgcRd32( const unsigned char* p )
{
	return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
		   ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}

static unsigned __int64 DgcRd64( const unsigned char* p )
{
	return (unsigned __int64)DgcRd32(p) |
		   ( (unsigned __int64)DgcRd32(p+4) << 32 );
}

static unsigned long DgcAlign16( unsigned long x )
{
	return (x + 15ul) & ~15ul;
}

static int DgcIsContainer( const char* id )
{
	return !strcmp(id,"DGCA") || !strcmp(id,"DATA") || !strcmp(id,"INFO") ||
		   !strcmp(id,"DGCR") || !strcmp(id,"IFDT") || !strcmp(id,"IFNM");
}

static int DgcIsKnown( const char* id )
{
	return DgcIsContainer(id) || !strcmp(id,"DGCC") || !strcmp(id,"IARC");
}

// The chunk tree: the walk and the two lookups.
static DgcChunk* DgcFindChunk( DgcWalk *w, const char* id, int depth );
static DgcChunk* DgcChildChunk( DgcWalk *w, DgcChunk* parent, const char* id );

// ------------------------------------------------------------------
// The recovery data (the DGCR chunk)
// ------------------------------------------------------------------
//
// A DGCR chunk holds an INFO chunk (the parameters) and a DATA chunk
// with the recovery data.  The data is a 2 dimensional parity code over
// the archive's 4 byte words:
//
//   the archive = pages x rows x 256 columns of 4 byte words
//   part 1 (rows*cols words): for every (row, column) the XOR of the
//           words of all the pages
//   part 2 (pages*rows words): for every (page, row) the XOR of the
//           words of all the columns
//   part 3 (rows words): the offsets of the row blocks
//
// It is a RAID-4 style redundancy: it can restore a lost 4 byte word.
// The recovery data is encrypted with the fixed key of the cipher (the
// password "SecureCryptograph" and 8 zero bytes) and the three parts
// are encrypted one after the other with the same cipher object, so
// the key stream runs over the whole recovery data.
//
// The recovery data covers the archive AS IT IS STORED (the encrypted
// bytes of an encrypted archive), so the repair has to happen before
// the decryption.

#define DGC_RECOVERY_KEY	"SecureCryptograph"

// The word of the archive: (page, row, column).  The matrix is padded
// with zeros if the archive is shorter than rows*cols*pages words.
#define DGC_ARCH_WORD(pg,rw,cl,alen) \
	( ( (((pg)*rows + (rw))*cols + (cl))*4 + 4) > (alen) ? 0ul : \
	  DgcRd32( arc->img + (((pg)*rows + (rw))*cols + (cl))*4 ) )

// Restore the damaged words of the archive with the parity table.  The
// number of restored words is returned (0 when nothing was damaged).
static unsigned long DgcRepair( DgcWalk *w, DgcArchive *arc, DgcChunk* dgcr )
{
	DgcChunk *info, *data;
	const unsigned char* ip;
	unsigned char* rec = NULL;
	unsigned char *par, *rowx;
	unsigned long off, crc, len, cols, rows, pages;
	unsigned long alen, i, j, k, x, fixed = 0;

	info = DgcChildChunk( w, dgcr, "INFO" );
	data = DgcChildChunk( w, dgcr, "DATA" );
	if( !info || !data ) return 0;
	if( info->psize < 0x30 ) return 0;
	ip = arc->img + info->off + DGC_CHUNK_HDR;
	off   = DgcRd32( ip + 0x28 );
	crc   = DgcRd32( ip + 0x2C );
	len   = DgcRd32( ip + 0x04 );
	cols  = DgcRd32( ip + 0x10 );
	rows  = DgcRd32( ip + 0x14 );
	pages = DgcRd32( ip + 0x18 );
	if( off > arc->imgsize ) return 0;
	if( !cols || !rows || !pages ) return 0;
	if( data->psize < len ) return 0;
	if( len < (rows*cols + pages*rows + rows) * 4 ) return 0;

	rec = (unsigned char*)malloc( len ? len : 1 );
	if( !rec ) return 0;
	memcpy( rec, arc->img + data->off + DGC_CHUNK_HDR, len );
	{
		// the fixed key of the cipher
		DgcCrypt c;
		c.SetKeyHeader( (const unsigned char*)DGC_RECOVERY_KEY, 17 );
		c.Decrypt( rec, len );
	}
	par  = rec;                    /* rows*cols words */
	rowx = rec + rows*cols*4;      /* pages*rows words */
	alen = off;

	// the columns whose parity does not match have a damaged word
	for( i=0; i<rows; i++ ) {
		for( j=0; j<cols; j++ ) {
			unsigned long want = DgcRd32( par + (i*cols+j)*4 );
			x = 0;
			for( k=0; k<pages; k++ ) x ^= DGC_ARCH_WORD(k,i,j,alen);
			if( x == want ) continue;
			{
				unsigned long cand = 0, kp = 0, y;
				int found = 0;
				// the damaged word must also repair the row parity
				for( k=0; k<pages; k++ ) {
					unsigned long z;
					y = 0;
					for( x=0; x<pages; x++ )
						if( x != k ) y ^= DGC_ARCH_WORD(x,i,j,alen);
					cand = want ^ y;
					z = 0;
					for( x=0; x<cols; x++ ) {
						unsigned long wv = ( x==j ) ? cand : DGC_ARCH_WORD(k,i,x,alen);
						z ^= wv;
					}
					if( z == DgcRd32( rowx + (k*rows+i)*4 ) ) {
						kp = k;
						found = 1;
						break;
					}
				}
				if( !found ) {
					// more than one word of the row is damaged: take
					// the most likely candidate
					y = 0;
					for( x=1; x<pages; x++ ) y ^= DGC_ARCH_WORD(x,i,j,alen);
					cand = want ^ y;
					kp = 0;
				}
				{
					unsigned long o = ((kp*rows + i)*cols + j)*4;
					if( o + 4 <= alen ) {
						unsigned char* q = arc->img + o;
						q[0] = (unsigned char)cand;
						q[1] = (unsigned char)(cand >> 8);
						q[2] = (unsigned char)(cand >> 16);
						q[3] = (unsigned char)(cand >> 24);
						fixed++;
					}
				}
			}
		}
	}
	free( rec );

	// the archive's checksum is the final check
	if( crc && (unsigned long)crc32( 0, arc->img, (uInt)alen )!=crc )
		return 0;
	return fixed;
}

// The DGCR chunk of a file: it is a chunk of the top level (a .dgcr is
// an archive with the recovery data appended, a .dgdr is a part of a
// spanning file with it appended).
static int DgcFindRecoveryChunk( unsigned char* img, unsigned long size,
								 unsigned long* off, unsigned long* psize )
{
	unsigned long p = 0;
	while( p + DGC_CHUNK_HDR <= size ) {
		unsigned long ps = DgcRd32( img + p + 8 );
		if( p + DGC_CHUNK_HDR + ps > size ) break;
		if( !memcmp( img + p, "DGCR", 4 ) ) {
			*off = p; *psize = ps;
			return 1;
		}
		p = DgcAlign16( p + DGC_CHUNK_HDR + ps );
	}
	return 0;
}

bool DgcFindRecovery( unsigned char* img, unsigned long size )
{
	unsigned long off, psize;
	return DgcFindRecoveryChunk( img, size, &off, &psize )!=0;
}

unsigned long DgcRepairRecovery( unsigned char* img, unsigned long size )
{
	DgcWalk w;
	DgcChunk dg;
	DgcArchive arc;
	unsigned long off, psize;

	if( !DgcFindRecoveryChunk( img, size, &off, &psize ) ) return 0;
	memset( &w, 0, sizeof(w) );
	w.img = img;
	w.size = size;
	memset( &dg, 0, sizeof(dg) );
	dg.off = off;
	dg.psize = psize;
	dg.depth = 0;
	memcpy( dg.id, "DGCR", 5 );
	memset( &arc, 0, sizeof(arc) );
	arc.img = img;
	arc.imgsize = size;
	return DgcRepair( &w, &arc, &dg );
}

//
// An encrypted archive has every DGCC record encrypted: the 32 byte
// chunk header AND the payload.  The outer chunk headers (the DGCA,
// the DATA, the INFO, the IARC, the IFDT and the IFNM) are in the
// clear, so the tree can still be walked; only the records have to be
// decrypted one by one.
//
// Two keys are used:
//   the chunk headers:  SHA-512( 8 zero bytes | password )
//   the payloads:       SHA-512( the plaintext header | password )
// and both are stretched over 39 rounds before they seed the MT19937
// of the key stream.

// Decrypt the DGCC records of one container (the DATA, the IFDT or
// the IFNM).  The records are decrypted in place.
static int DgcDecryptRecords( DgcWalk *w, DgcChunk* parent,
							   const unsigned char* pw, unsigned long pwlen )
{
	unsigned long p = parent->off + DGC_CHUNK_HDR;
	unsigned long end = parent->off + DGC_CHUNK_HDR + parent->psize;
	DgcCrypt c;

	while( p + DGC_CHUNK_HDR <= end ) {
		unsigned char hdr[DGC_CHUNK_HDR];
		unsigned long ps;

		c.SetKeyHeader( pw, pwlen );
		c.Decrypt( w->img + p, DGC_CHUNK_HDR );
		memcpy( hdr, w->img + p, DGC_CHUNK_HDR );
		if( memcmp(hdr,"DGCC",4) ) {
			// not a DGCC record: put the header back and stop
			c.SetKeyHeader( pw, pwlen );
			c.Encrypt( w->img + p, DGC_CHUNK_HDR );
			return 0;
		}
		ps = DgcRd32( hdr + 8 );
		if( p + DGC_CHUNK_HDR + ps > end ) return -1;
		c.SetKeyPayload( hdr, DGC_CHUNK_HDR, pw, pwlen );
		c.Decrypt( w->img + p + DGC_CHUNK_HDR, ps );
		p = DgcAlign16( p + DGC_CHUNK_HDR + ps );
	}
	return 0;
}

// Walk the chunk tree.  The recovery data (a DGCR chunk) is scrambled
// by design, so it is not walked into, and an encrypted archive has
// its chunk headers encrypted, so the walk stops there.
static void DgcWalkTree( DgcWalk *w, unsigned long off, unsigned long end,
						 int depth, const char* parent )
{
	unsigned long p = off;

	while( p + DGC_CHUNK_HDR <= end ) {
		const unsigned char* h = w->img + p;
		unsigned long ps = DgcRd32( h + 8 );
		DgcChunk* c;
		char id[5];

		if( w->nchunk >= DGC_MAX_CHUNK ) return;
		memcpy( id, h, 4 );
		id[4] = 0;
		if( !DgcIsKnown(id) ) {
			if( !parent || strcmp(parent,"DGCR") ) w->broken = 1;
			return;
		}
		c = &w->chunks[w->nchunk++];
		c->off = p;
		c->psize = ps;
		c->depth = depth;
		memcpy( c->id, h, 4 );
		c->id[4] = 0;
		if( strcmp(c->id,"DGCR") && DgcIsContainer(c->id) ) {
			DgcWalkTree( w, p + DGC_CHUNK_HDR, p + DGC_CHUNK_HDR + ps,
						 depth + 1, c->id );
		}
		p = DgcAlign16( p + DGC_CHUNK_HDR + ps );
	}
}

static DgcChunk* DgcFindChunk( DgcWalk *w, const char* id, int depth )
{
	int i;
	for( i=0; i<w->nchunk; i++ ) {
		if( !strcmp(w->chunks[i].id,id) && w->chunks[i].depth==depth )
			return &w->chunks[i];
	}
	return NULL;
}

// The first child of a container with the given id.
static DgcChunk* DgcChildChunk( DgcWalk *w, DgcChunk* parent, const char* id )
{
	unsigned long p = parent->off + DGC_CHUNK_HDR;
	unsigned long end = parent->off + DGC_CHUNK_HDR + parent->psize;

	while( p + DGC_CHUNK_HDR <= end ) {
		const unsigned char* h = w->img + p;
		unsigned long ps = DgcRd32( h + 8 );
		if( !memcmp(h,id,4) ) {
			DgcChunk* c = &w->chunks[w->nchunk<DGC_MAX_CHUNK ? w->nchunk++ : DGC_MAX_CHUNK-1];
			c->off = p;
			c->psize = ps;
			c->depth = parent->depth + 1;
			memcpy( c->id, h, 4 );
			c->id[4] = 0;
			return c;
		}
		p = DgcAlign16( p + DGC_CHUNK_HDR + ps );
	}
	return NULL;
}

// The CRC-32 of a chunk's payload and the one of its header (with the
// last field zeroed).
static int DgcChunkCrcOk( DgcWalk *w, DgcChunk* c )
{
	const unsigned char* h = w->img + c->off;
	unsigned long want = DgcRd32( h + 24 );
	unsigned long hwant = DgcRd32( h + 28 );
	unsigned char tmp[DGC_CHUNK_HDR];

	memcpy( tmp, h, DGC_CHUNK_HDR );
	memset( tmp + 28, 0, 4 );
	return ( (unsigned long)crc32( 0, w->img + c->off + DGC_CHUNK_HDR, (uInt)c->psize ) == want
			 && (unsigned long)crc32( 0, tmp, DGC_CHUNK_HDR ) == hwant );
}

// ------------------------------------------------------------------
// The range decoder (the GFL coder)
// ------------------------------------------------------------------

struct DgcRng
{
	const unsigned char*	buf;
	unsigned long			len;
	unsigned long			pos;
	unsigned long			code;
	unsigned long			range;
	unsigned long			prev;
};

static void DgcRngInit( DgcRng *r, const unsigned char* buf, unsigned long len )
{
	unsigned long b;

	r->buf = buf;
	r->len = len;
	r->pos = 0;
	// the encoder's dummy byte is skipped
	if( r->pos < r->len ) r->pos++;
	b = ( r->pos < r->len ) ? r->buf[r->pos++] : 0;
	r->prev = b;
	r->code = b >> 1;
	r->range = 0x80;
}

static unsigned long DgcRngByte( DgcRng *r )
{
	if( r->pos < r->len ) return r->buf[r->pos++];
	r->pos++;
	return 0;
}

static void DgcRngRenorm( DgcRng *r )
{
	while( r->range <= 0x800000ul ) {
		unsigned long b;
		r->code = ((r->code << 1) | (r->prev & 1)) << 7;
		b = DgcRngByte( r );
		r->prev = b;
		r->code |= (b >> 1);
		r->range <<= 8;
		if( r->range == 0 ) {
			r->range = 0x800000ul;
			return;
		}
	}
}

static unsigned long DgcRngRaw( DgcRng *r, int n )
{
	unsigned long v = 0;
	int i;
	for( i=0; i<n; i++ ) {
		unsigned long half;
		DgcRngRenorm( r );
		half = r->range >> 1;
		v <<= 1;
		if( r->code < half ) {
			r->range = half;
		} else {
			v |= 1;
			r->code -= half;
			r->range -= half;
		}
	}
	return v;
}

// A gamma style symbol: the number of "continue" bits is the level,
// and the level 0 and 1 are the values themselves.
static unsigned long DgcRngSym( DgcRng *r, unsigned long* probs, int count,
								int shift, unsigned long base )
{
	int v = 0;

	for(;;) {
		unsigned long p, bound;
		DgcRngRenorm( r );
		p = probs[base+v];
		bound = p * (r->range >> 12);
		if( r->code < bound ) {
			r->range = bound;
			probs[base+v] += (4096ul - probs[base+v]) >> shift;
			v++;
			if( v >= count ) break;
		} else {
			r->range -= bound;
			r->code -= bound;
			probs[base+v] -= probs[base+v] >> shift;
			break;
		}
	}
	if( v <= 1 ) return (unsigned long)v;
	return (1ul << (v-1)) + DgcRngRaw( r, v-1 );
}

// ------------------------------------------------------------------
// The context mixing decoder
// ------------------------------------------------------------------

struct DgcCm
{
	unsigned long	lit[256];
	unsigned long	ctx[32];
	unsigned long	run[32*256];
	unsigned long	pair[65536];
	unsigned long	state;
	int				p_lit, p_pairb, p_pair, p_ctx, p_run, p_minlen, p_lzsh, mode;
};

static int DgcBsr32( unsigned long x )
{
	int r = 31;
	if( x ) {
		while( !(x & 0x80000000ul) ) { x <<= 1; r--; }
	}
	return r;
}

static void DgcCmInit( DgcCm *c, unsigned long params )
{
	int i;
	for( i=0; i<256; i++ ) c->lit[i] = 2048;
	for( i=0; i<32; i++ ) c->ctx[i] = 2048;
	for( i=0; i<32*256; i++ ) c->run[i] = 2048;
	for( i=0; i<65536; i++ ) c->pair[i] = 2048;
	c->state = 0;
	c->p_lit    = (int)((params >> 4) & 0xF);
	c->p_pairb = (int)((params >> 8) & 0xF);
	c->p_pair  = (int)((params >> 12) & 0xF);
	c->p_ctx   = (int)((params >> 16) & 0xF);
	c->p_run   = (int)((params >> 20) & 0xF);
	c->p_minlen= (int)((params >> 24) & 0xF);
	c->p_lzsh  = (int)((params >> 28) & 0xF);
	c->mode    = (int)((params >> 1) & 1);
}

// The literal coder.  The tree of the PREVIOUS byte is walked from the
// deepest node up, the eight bits of the literal are coded MSB first
// with the two models mixed, and then the same nodes are walked again
// to restore the model.
static unsigned long DgcCmLiteral( DgcRng *r, DgcCm *C, int prev )
{
	unsigned long *lit = C->lit, *pair = C->pair, *a5 = C->ctx;
	int ci = DgcBsr32( C->state );
	unsigned long w = a5[ci];
	unsigned long chain, scale = 0x2000, node, value = 0;
	unsigned long saved[8];
	unsigned long v58 = 0x80000, v57 = 0x80000;
	int i;

	// the prepare pass
	chain = (unsigned long)prev | 0x100;
	for( i=0; i<8; i++ ) {
		unsigned long bit = chain & 1, nd = chain >> 1, p = lit[nd], d;
		saved[i] = p;
		if( bit ) { d = (scale * (4096ul - p)) >> 13; lit[nd] = p + d; }
		else     { d = (scale * p) >> 13;         lit[nd] = p - d; }
		scale = d;
		chain = nd;
	}

	// the main loop: the eight bits of the literal, MSB first
	node = 1;
	for( i=0; i<8; i++ ) {
		unsigned long pr, bound, code, rng, p;
		int bit;
		pr = (w * lit[node] + pair[256*prev + node] * (4096ul - w)) >> 12;
		DgcRngRenorm( r );
		code = r->code; rng = r->range;
		bound = pr * (rng >> 12);
		if( code < bound ) {
			bit = 0;
			r->range = bound;
			v58 = (v58 * lit[node]) >> 12;
			v57 = (v57 * pair[256*prev + node]) >> 12;
			p = pair[256*prev + node];
			pair[256*prev + node] = p + ((4096ul - p) >> C->p_pair);
		} else {
			bit = 1;
			r->range = rng - bound;
			r->code = code - bound;
			v58 = (v58 * (4096ul - lit[node])) >> 12;
			p = pair[256*prev + node];
			v57 = (v57 * (4096ul - p)) >> 12;
			pair[256*prev + node] = p - (p >> C->p_pair);
		}
		node = 2*node + (unsigned long)bit;
		value = 2*value + (unsigned long)bit;
	}

	// the restore pass: the same nodes as the prepare pass
	chain = (unsigned long)prev | 0x100;
	for( i=0; i<8; i++ ) {
		unsigned long bit = chain & 1, nd = chain >> 1, p = saved[i], q;
		lit[nd] = p;
		if( bit ) { lit[nd] = p - (p >> C->p_lit);
				   q = pair[256*value + nd];
				   pair[256*value + nd] = q - (q >> C->p_pairb); }
		else     { lit[nd] = p + ((4096ul - p) >> C->p_lit);
				   q = pair[256*value + nd];
				   pair[256*value + nd] = q + ((4096ul - q) >> C->p_pairb); }
		chain = nd;
	}

	// the state and the context weight
	{
		int v55 = (v58 <= v57);
		C->state = (v58 * w + v57 * (4096ul - w)) >> 12;
		if( v58 < v57 ) { a5[ci] -= a5[ci] >> C->p_ctx; v55 = (v58 <= v57); }
		if( !v55 )     { a5[ci] += (4096ul - a5[ci]) >> C->p_ctx; }
	}
	return value & 0xFF;
}

// The match coder.  It is used when the params' bit 1 is set, which
// the sample archives never do.
static unsigned long DgcCmMatch( DgcRng *r, DgcCm *C, int prev )
{
	unsigned long *lit = C->lit, *pair = C->pair, *a5 = C->ctx;
	int ci = DgcBsr32( C->state );
	unsigned long w = a5[ci];
	unsigned long v18 = 0x80000, v17 = 0x80000, v19 = 0, v8 = 1;
	int v21 = 8;

	for(;;) {
		unsigned long pr, bound, code, rng;
		int v13, last;
		pr = (w * lit[v8] + pair[256*prev + v8] * (4096ul - w)) >> 12;
		DgcRngRenorm( r );
		code = r->code; rng = r->range;
		bound = pr * (rng >> 12);
		if( code < bound ) {
			unsigned long p;
			r->range = bound;
			v18 = (v18 * lit[v8]) >> 12;
			v17 = (v17 * pair[256*prev + v8]) >> 12;
			v13 = 0;
			p = pair[256*prev + v8];
			pair[256*prev + v8] = p + ((4096ul - p) >> C->p_pair);
			lit[v8] += (4096ul - lit[v8]) >> C->p_lit;
		} else {
			unsigned long p = pair[256*prev + v8];
			r->range = rng - bound;
			r->code = code - bound;
			v18 = (v18 * (4096ul - lit[v8])) >> 12;
			v17 = (v17 * (4096ul - p)) >> 12;
			v13 = 1;
			pair[256*prev + v8] = p - (p >> C->p_pair);
			lit[v8] -= lit[v8] >> C->p_lit;
		}
		last = (v21 == 1);
		v8 = (unsigned long)v13 + 2*v8;
		v19 = (unsigned long)v13 + 2*v19;
		v21--;
		if( last ) break;
	}
	{
		int v15 = (v18 <= v17);
		C->state = (v18 * w + v17 * (4096ul - w)) >> 12;
		if( v18 < v17 ) { a5[ci] -= a5[ci] >> C->p_ctx; v15 = (v18 <= v17); }
		if( !v15 )     { a5[ci] += (4096ul - a5[ci]) >> C->p_ctx; }
	}
	return v19 & 0xFF;
}

// ------------------------------------------------------------------
// The blocks
// ------------------------------------------------------------------

struct DgcBlock
{
	unsigned long	tag, size;	// the size is the original size
	unsigned long	flags, params, v22, v7, lenA, lenB;
	const unsigned char* A;
	const unsigned char* B;
};

// The entropy decoder: it produces the token buffer.
static void DgcEntropyDecode( DgcCm *C, unsigned long params,
							  const unsigned char* A, unsigned long lenA,
							  unsigned char* out, unsigned long outlen )
{
	DgcRng r;
	unsigned long pos = 0;
	int prev;

	DgcCmInit( C, params );
	DgcRngInit( &r, A, lenA );
	// the first byte of the block decides the initial previous byte
	prev = (params & 1) ? 0xFF : 0;

	while( pos < outlen ) {
		if( C->mode ) {
			unsigned long b = DgcCmMatch( &r, C, prev );
			out[pos++] = (unsigned char)b;
			prev = (int)b;
		} else {
			unsigned long b = DgcCmLiteral( &r, C, prev );
			unsigned long rl, n, i;
			prev = (int)b;
			rl = DgcRngSym( &r, C->run, 32, C->p_run, 32*b );
			n = rl + 1;
			if( pos + n > outlen ) n = outlen - pos;
			for( i=0; i<n; i++ ) out[pos++] = (unsigned char)b;
		}
	}
}

// The LZ stage's inverse: the token buffer is permuted, and the
// permutation is rebuilt from the byte histogram of the buffer.
static int DgcLzInverse( const unsigned char* src, unsigned long n,
						 unsigned long start, unsigned char* dst )
{
	unsigned long cnt[256];
	unsigned long* block;
	unsigned long a = start, k;
	int i;

	if( n == 0 ) return 0;
	block = (unsigned long*)malloc( sizeof(unsigned long) * n );
	if( !block ) return -1;
	for( i=0; i<256; i++ ) cnt[i] = 0;
	for( k=0; k<n; k++ ) cnt[src[k]]++;
	{
		unsigned long s = 0;
		for( i=0; i<256; i++ ) {
			unsigned long c = cnt[i];
			cnt[i] = s;
			s += c;
		}
	}
	for( k=0; k<n; k++ ) {
		block[cnt[src[k]]] = k;
		cnt[src[k]]++;
	}
	for( k=0; k<n; k++ ) {
		if( a >= n ) { free(block); return -1; }
		a = block[a];
		if( a >= n ) { free(block); return -1; }
		dst[k] = src[a];
	}
	free( block );
	return 0;
}

// The pre-transform's inverse: an LZ77 with an implicit distance, the
// two byte hash table maps (the previous byte, the byte) to the last
// output position.
static int DgcPretransformInverse( DgcCm *C, unsigned long params,
								   const unsigned char* model, unsigned long lenModel,
								   const unsigned char* data, unsigned long n,
								   unsigned char* out, unsigned long outlen )
{
	static unsigned long m1[32], m2[32];
	static unsigned long tbl[65536];
	DgcRng r;
	unsigned long minlen, key = 0, op = 0, ip = 0;
	int i;

	DgcCmInit( C, params );
	DgcRngInit( &r, model, lenModel );
	for( i=0; i<32; i++ ) { m1[i] = 2048; m2[i] = 2048; }
	for( i=0; i<65536; i++ ) tbl[i] = 0xFFFFFFFFul;
	minlen = 1ul << C->p_minlen;

	while( op < outlen ) {
		unsigned long nr = DgcRngSym( &r, m1, 32, C->p_lzsh, 0 );
		unsigned long k;
		for( k=0; k<nr && op<outlen; k++ ) {
			out[op] = (ip<n) ? data[ip] : 0;
			ip++;
			tbl[key] = op;
			key = ((key & 0xFF) << 8) | out[op];
			op++;
		}
		if( op >= outlen ) break;
		{
			unsigned long ln = minlen + DgcRngSym( &r, m2, 32, C->p_lzsh, 0 );
			if( ln && tbl[key] != 0xFFFFFFFFul ) {
				unsigned long src = tbl[key];
				for( k=0; k<ln && op<outlen; k++ ) {
					if( src >= outlen ) { out[op] = 0; src = 0; }
					else out[op] = out[src];
					src++;
					tbl[key] = op;
					key = ((key & 0xFF) << 8) | out[op];
					op++;
				}
			}
		}
	}
	return 0;
}

// The inverse of the delta transform of the "column" method.
static void DgcInverseDelta( unsigned char* p, unsigned long n, int order )
{
	unsigned char prev = 0;
	unsigned long i;

	for( i=0; i<n; i++ ) {
		unsigned char d = p[i];
		p[i] = (unsigned char)(prev + d);
		prev = p[i];
	}
	if( order >= 2 ) {
		prev = 0;
		for( i=0; i<n; i++ ) {
			unsigned char d = p[i];
			p[i] = (unsigned char)(prev + d);
			prev = p[i];
		}
	}
}

// Decode one compressed block.
static int DgcDecodeBlock( DgcCm *C, const DgcBlock *b,
						   unsigned char* out, unsigned long outlen )
{
	unsigned char* tok;
	unsigned char* lz = NULL;
	int rc = -1;

	tok = (unsigned char*)malloc( b->v7 ? b->v7 : 1 );
	if( !tok ) return -1;

	DgcEntropyDecode( C, b->params, b->A, b->lenA, tok, b->v7 );

	if( b->flags & 2 ) {
		lz = (unsigned char*)malloc( b->v7 ? b->v7 : 1 );
		if( !lz ) { free(tok); return -1; }
		if( DgcLzInverse( tok, b->v7, b->v22, lz ) != 0 ) {
			free(lz); free(tok); return -1;
		}
	} else {
		lz = tok;
	}

	if( b->flags & 4 ) {
		rc = DgcPretransformInverse( C, b->params, b->B, b->lenB, lz, b->v7,
									 out, outlen );
	} else {
		// the column method: the delta transform
		int order = (int)((b->flags >> 16) & 0xF);
		if( order ) DgcInverseDelta( lz, b->v7, order );
		memcpy( out, lz, b->v7 < outlen ? b->v7 : outlen );
		rc = 0;
	}
	if( lz != tok ) free(lz);
	free( tok );
	return rc;
}

// ------------------------------------------------------------------
// The DGCC payload: a stream of blocks
// ------------------------------------------------------------------

struct DgcBuf
{
	unsigned char*	p;
	unsigned long	len;
	unsigned long	cap;
};

static int DgcBufPut( DgcBuf *b, const unsigned char* d, unsigned long n )
{
	if( b->len + n > b->cap ) {
		unsigned long nc = (b->len + n) * 2 + 1024;
		unsigned char* np = (unsigned char*)realloc( b->p, nc );
		if( !np ) return -1;
		b->p = np;
		b->cap = nc;
	}
	if( n ) memcpy( b->p + b->len, d, n );
	b->len += n;
	return 0;
}

// Decode a whole DGCC payload.  A tag 2 block is the "column" method:
// `size` columns follow and have to be de-interleaved again.
static int DgcDecompress( DgcCm *C, const unsigned char* pay, unsigned long len,
						  DgcBuf *out )
{
	unsigned long o = 0;

	while( o + 8 <= len ) {
		unsigned long tag = DgcRd32( pay + o );
		unsigned long size = DgcRd32( pay + o + 4 );

		if( tag == 0 ) {
			// stored
			if( o + 8 + size > len ) return -1;
			if( DgcBufPut( out, pay + o + 8, size )!=0 ) return -1;
			o += 8 + size;
		} else if( tag == 1 ) {
			DgcBlock b;
			unsigned char* tmp;
			memset( &b, 0, sizeof(b) );
			if( o + 32 > len ) return -1;
			b.tag    = 1;
			b.size   = size;
			b.flags  = DgcRd32( pay + o + 8 );
			b.params = DgcRd32( pay + o + 12 );
			b.v22    = DgcRd32( pay + o + 16 );
			b.v7     = DgcRd32( pay + o + 20 );
			b.lenA   = DgcRd32( pay + o + 24 );
			b.lenB   = DgcRd32( pay + o + 28 );
			if( o + 32 + b.lenA + b.lenB > len ) return -1;
			b.A = pay + o + 32;
			b.B = b.A + b.lenA;
			tmp = (unsigned char*)malloc( size ? size : 1 );
			if( !tmp ) return -1;
			if( DgcDecodeBlock( C, &b, tmp, size )==0 ) {
				if( DgcBufPut( out, tmp, size )!=0 ) { free(tmp); return -1; }
			} else {
				free(tmp);
				return -1;
			}
			free(tmp);
			o += 32 + b.lenA + b.lenB;
		} else if( tag == 2 ) {
			// the column method: `size` columns, each a compressed block
			unsigned long stride = size, oo = o + 8, k, i, total = 0, n;
			unsigned char** cols = NULL;
			unsigned long* csz = NULL;
			if( stride == 0 ) return -1;
			cols = (unsigned char**)malloc( sizeof(unsigned char*) * stride );
			csz = (unsigned long*)malloc( sizeof(unsigned long) * stride );
			if( !cols || !csz ) { free(cols); free(csz); return -1; }
			memset( cols, 0, sizeof(unsigned char*) * stride );
			memset( csz, 0, sizeof(unsigned long) * stride );
			for( k=0; k<stride; k++ ) {
				DgcBlock b;
				unsigned char* tmp;
				if( oo + 32 > len ) break;
				if( DgcRd32( pay + oo ) != 1 ) break;
				memset( &b, 0, sizeof(b) );
				b.tag    = 1;
				b.size   = DgcRd32( pay + oo + 4 );
				b.flags  = DgcRd32( pay + oo + 8 );
				b.params = DgcRd32( pay + oo + 12 );
				b.v22    = DgcRd32( pay + oo + 16 );
				b.v7     = DgcRd32( pay + oo + 20 );
				b.lenA   = DgcRd32( pay + oo + 24 );
				b.lenB   = DgcRd32( pay + oo + 28 );
				if( oo + 32 + b.lenA + b.lenB > len ) break;
				b.A = pay + oo + 32;
				b.B = b.A + b.lenA;
				tmp = (unsigned char*)malloc( b.size ? b.size : 1 );
				if( !tmp ) break;
				if( DgcDecodeBlock( C, &b, tmp, b.size )!=0 ) {
					free(tmp);
					break;
				}
				cols[k] = tmp;
				csz[k] = b.size;
				total += b.size;
				oo += 32 + b.lenA + b.lenB;
			}
			// de-interleave the columns
			n = total / stride;
			{
				unsigned char* buf = (unsigned char*)malloc( total ? total : 1 );
				if( buf ) {
					memset( buf, 0, total );
					for( i=0; i<n; i++ ) {
						for( k=0; k<stride; k++ ) {
							if( i < csz[k] ) buf[i*stride + k] = cols[k][i];
						}
					}
					DgcBufPut( out, buf, total );
					free(buf);
				}
			}
			for( k=0; k<stride; k++ ) if( cols[k] ) free(cols[k]);
			free(cols);
			free(csz);
			o = oo;
		} else {
			return -1;
		}
	}
	return 0;
}

// ------------------------------------------------------------------

// The payload of the DATA, the IFDT and the IFNM chunks holds a
// sequence of DGCC chunks, and each of them is a stream of blocks.  The
// output of all of them is concatenated.
static int DgcDecodeChildren( DgcWalk *w, DgcChunk* parent, DgcBuf *out )
{
	unsigned long p = parent->off + DGC_CHUNK_HDR;
	unsigned long end = parent->off + DGC_CHUNK_HDR + parent->psize;
	DgcCm* cm;

	cm = (DgcCm*)malloc( sizeof(DgcCm) );
	if( !cm ) return -1;
	while( p + DGC_CHUNK_HDR <= end ) {
		const unsigned char* h = w->img + p;
		unsigned long ps = DgcRd32( h + 8 );
		if( memcmp(h,"DGCC",4) ) break;
		if( DgcDecompress( cm, w->img + p + DGC_CHUNK_HDR, ps, out )!=0 ) {
			free(cm);
			return -1;
		}
		p = DgcAlign16( p + DGC_CHUNK_HDR + ps );
	}
	free(cm);
	return 0;
}

// The index table: 64 bytes per entry, stored transposed (column
// major), so the entry of row e is blob[c*n + e].
static int DgcParseIndex( DgcWalk *w, DgcChunk* ifdt, DgcArchive *arc )
{
	DgcBuf dec;
	unsigned char* blob;
	unsigned long blobsize, n, e, c;

	memset( &dec, 0, sizeof(dec) );
	if( DgcDecodeChildren( w, ifdt, &dec )!=0 ) {
		free(dec.p);
		return -1;
	}
	blob = dec.p;
	blobsize = dec.len;
	n = blobsize / 64;
	if( n == 0 ) { free(blob); return -1; }

	arc->entries = (DgcEntry*)malloc( sizeof(DgcEntry) * n );
	if( !arc->entries ) { free(blob); return -1; }
	for( e=0; e<n; e++ ) {
		unsigned char row[64];
		for( c=0; c<64; c++ ) row[c] = blob[c*n + e];
		arc->entries[e].crc   = DgcRd32( row + 0 );
		arc->entries[e].attr  = DgcRd32( row + 4 );
		arc->entries[e].mtime = DgcRd64( row + 16 );
		arc->entries[e].size  = (unsigned long)DgcRd64( row + 24 );
		arc->entries[e].csize = DgcRd32( row + 32 );
		arc->entries[e].offset= DgcRd32( row + 36 );
	}
	arc->nentry = (int)n;
	free( blob );
	return 0;
}

// The name list: the pairs (the length, the name), the names are
// UTF-8 and the paths use backslashes.
static int DgcParseNames( DgcWalk *w, DgcChunk* ifnm, DgcArchive *arc )
{
	DgcBuf dec;
	unsigned long p, namelen, i;

	memset( &dec, 0, sizeof(dec) );
	if( DgcDecodeChildren( w, ifnm, &dec )!=0 ) {
		free(dec.p);
		return -1;
	}
	arc->names = dec.p;
	arc->namesize = dec.len;

	// the names are in the same order as the index entries
	p = 0;
	for( i=0; i<(unsigned long)arc->nentry; i++ ) {
		if( p + 4 > arc->namesize ) break;
		namelen = DgcRd32( arc->names + p );
		p += 4;
		if( p + namelen > arc->namesize ) break;
		{
			unsigned long k, n = namelen;
			if( n > DGC_MAX_NAME-1 ) n = DGC_MAX_NAME-1;
			for( k=0; k<n; k++ ) arc->entries[i].name[k] = (char)arc->names[p+k];
			arc->entries[i].name[n] = 0;
		}
		p += namelen;
	}
	return 0;
}

bool DgcOpen( const char* path, DgcArchive *arc, const char* password )
{
	FILE* fp;
	DgcWalk w;
	DgcChunk* root, *data, *info, *iarc, *ifdt, *ifnm;
	unsigned long fsize;

	memset( arc, 0, sizeof(*arc) );

	fp = fopen( path, "rb" );
	if( !fp ) return false;
	fseek( fp, 0, SEEK_END );
	fsize = (unsigned long)ftell(fp);
	fseek( fp, 0, SEEK_SET );
	if( fsize < DGC_CHUNK_HDR || fsize > 0x20000000ul ) { fclose(fp); return false; }
	arc->img = new BYTE[fsize];
	if( !arc->img ) { fclose(fp); return false; }
	if( fsize!=fread(arc->img,1,fsize,fp) ) {
		fclose(fp);
		delete[] arc->img;
		arc->img = 0;
		return false;
	}
	fclose(fp);
	arc->imgsize = fsize;

	// the container
	memset( &w, 0, sizeof(w) );
	w.img = arc->img;
	w.size = fsize;
	DgcWalkTree( &w, 0, fsize, 0, 0 );
	if( w.nchunk < 1 || strcmp(w.chunks[0].id,"DGCA") ) {
		DgcClose( arc );
		return false;
	}
	root = &w.chunks[0];

	// The recovery data: a DGCR chunk at the top level.  Its INFO chunk
	// holds the checksum of the archive (the bytes in front of the
	// DGCR chunk) and the parity table restores the words that were
	// lost.  The repair has to happen before the checksums are read,
	// because a damaged word can be a checksum.
	{
		DgcChunk* dgcr = DgcFindChunk( &w, "DGCR", 0 );
		if( dgcr ) {
			DgcChunk* info = DgcChildChunk( &w, dgcr, "INFO" );
			if( info && info->psize >= 0x30 ) {
				const unsigned char* ip = arc->img + info->off + DGC_CHUNK_HDR;
				unsigned long off = DgcRd32( ip + 0x28 );
				unsigned long crc = DgcRd32( ip + 0x2C );
				if( crc && off <= arc->imgsize &&
					(unsigned long)crc32( 0, arc->img, (uInt)off )!=crc ) {
					DgcRepair( &w, arc, dgcr );
				}
			}
		}
	}

	if( !DgcChunkCrcOk( &w, &w.chunks[0] ) ) {
		DgcClose( arc );
		return false;
	}

	// the index
	iarc = DgcFindChunk( &w, "IARC", 2 );
	if( !iarc ) {
		DgcClose( arc );
		return false;
	}
	{
		const unsigned char* h = arc->img + iarc->off + DGC_CHUNK_HDR;
		if( (h[0] & 1) ) {
			// The archive is encrypted.  The records of the DATA,
			// the IFDT and the IFNM are decrypted with the password
			// and the tree is walked again, because the ids of the
			// records were encrypted too.
			DgcChunk* cont[3];
			unsigned long pwlen;
			int i;

			if( !password || !*password ) {
				DgcClose( arc );
				arc->encrypted = 1;
				return false;
			}
			pwlen = (unsigned long)strlen(password);
			cont[0] = DgcFindChunk( &w, "DATA", 1 );
			cont[1] = DgcFindChunk( &w, "IFDT", 2 );
			cont[2] = DgcFindChunk( &w, "IFNM", 2 );
			for( i=0; i<3; i++ ) {
				if( !cont[i] ) { DgcClose( arc ); return false; }
				if( DgcDecryptRecords( &w, cont[i], (const unsigned char*)password, pwlen )!=0 ) {
					DgcClose( arc );
					return false;
				}
			}
			// the IARC payload's +0x20 area (48 bytes) holds the
			// number of entries and the length of the name blob
			{
				DgcCrypt c;
				unsigned char* area = arc->img + iarc->off + DGC_CHUNK_HDR + 0x20;
				c.SetKeyHeader( (const unsigned char*)password, pwlen );
				c.Decrypt( area, 48 );
				// the numbers have to be plausible: a wrong
				// password gives garbage
				if( DgcRd64( area + 0x08 ) > (unsigned __int64)0x1000000 ||
					DgcRd32( area + 0x20 ) > 0x1000000u ) {
					DgcClose( arc );
					arc->encrypted = 1;
					return false;
				}
			}
			// walk the tree again: now the records are readable
			memset( &w, 0, sizeof(w) );
			w.img = arc->img;
			w.size = fsize;
			DgcWalkTree( &w, 0, fsize, 0, 0 );
			iarc = DgcFindChunk( &w, "IARC", 2 );
			if( !iarc ) { DgcClose( arc ); return false; }
		}
	}

	info = DgcFindChunk( &w, "INFO", 1 );
	data = DgcFindChunk( &w, "DATA", 1 );
	ifdt = DgcFindChunk( &w, "IFDT", 2 );
	ifnm = DgcFindChunk( &w, "IFNM", 2 );
	if( !info || !data || !ifdt || !ifnm ) {
		DgcClose( arc );
		return false;
	}

	if( DgcParseIndex( &w, ifdt, arc )!=0 ) {
		DgcClose( arc );
		return false;
	}
	if( DgcParseNames( &w, ifnm, arc )!=0 ) {
		DgcClose( arc );
		return false;
	}

	// the data area
	{
		DgcBuf dec;
		memset( &dec, 0, sizeof(dec) );
		if( DgcDecodeChildren( &w, data, &dec )!=0 ) {
			free(dec.p);
			DgcClose( arc );
			return false;
		}
		arc->data = dec.p;
		arc->datasize = dec.len;
	}
	return true;
}

void DgcClose( DgcArchive *arc )
{
	if( !arc ) return;
	if( arc->img ) delete[] arc->img;
	if( arc->entries ) delete[] arc->entries;
	if( arc->names ) free( arc->names );
	if( arc->data ) free( arc->data );
	memset( arc, 0, sizeof(*arc) );
}

bool DgcExtractFile( DgcArchive *arc, int index, const char* outpath )
{
	DgcEntry* e;
	FILE* out;
	unsigned long off, n;
	int i;

	if( index<0 || index>=arc->nentry ) return false;
	e = &arc->entries[index];

	// The records are laid out one after the other in the data area,
	// and the "compressed size" and the offset of the index are NOT
	// to be used: they differ between a stored and a compressed
	// archive of the same files.
	off = 0;
	for( i=0; i<index; i++ ) off += arc->entries[i].size;
	n = e->size;
	if( off + n > arc->datasize ) return false;

	out = fopen( outpath, "wb" );
	if( !out ) return false;
	if( n ) {
		if( n!=fwrite( arc->data + off, 1, n, out ) ) {
			fclose(out);
			return false;
		}
	}
	fclose(out);

	// the CRC-32 of the contents has to match the index
	if( e->crc ) {
		unsigned long crc = (unsigned long)crc32( 0, arc->data + off, (uInt)n );
		if( crc != e->crc ) return false;
	}
	return true;
}

// The password of an encrypted archive as the command line gives it.
// -p<password> is the password itself, -p#<file> is a key file whose
// content is hashed with SHA-512 (the hex digest of the hash is the
// password, which is what the DGC Archiver's -k switch does) and
// -p##text is the text itself.
std::string DgcPasswordFromArg( const char* arg )
{
	std::string s;
	if( !arg || !*arg ) return s;
	if( *arg=='#' ) {
		if( arg[1]=='#' ) return std::string(arg+1);
		{
			FILE* fp = fopen( arg+1, "rb" );
			std::string data;
			char buf[4096];
			size_t n;
			char hex[129];
			if( !fp ) return s;
			while( (n = fread(buf,1,sizeof(buf),fp)) > 0 )
				data.append(buf,n);
			fclose(fp);
			DgcSha512Hex( (const unsigned char*)data.data(),
						   (unsigned long)data.size(), hex );
			return std::string(hex);
		}
	}
	return std::string(arg);
}
