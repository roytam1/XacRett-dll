/* UHARC container. Clean-room from FORMAT.md.
 * Phase 1: 0.2 container (48-byte header, 4 tables, backward size
 * deltas, Pascal-string directory). Tables 0/7/2/7 decode via cores
 * 0/2/2 (UhaDec0/UhaDec2 LSB). 0.3/0.4/0.6 headers land here next;
 * they share the same structure with different field widths.
 */

#include <stdlib.h>
#include <string.h>
#include "UhaDecode.h"
#include "UhaCrypt.h"
#include "UhaDec0.h"
#include "UhaDec2.h"
#include "UhaDec3.h"
#include "UhaDec4.h"
#include "UhaDec5.h"

static unsigned long uha_crc_tab[256];
static int uha_crc_ready = 0;

static void uha_crc_make(void)
{
	unsigned i, j;
	if( uha_crc_ready )
		return;
	for( i = 0; i < 256; i++ )
	{
		unsigned long c = i;
		for( j = 0; j < 8; j++ )
			c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
		uha_crc_tab[i] = c;
	}
	uha_crc_ready = 1;
}

static unsigned long uha_crc_update(unsigned long crc,
	const unsigned char* d, unsigned long n)
{
	unsigned long i;
	uha_crc_make();
	for( i = 0; i < n; i++ )
		crc = uha_crc_tab[(unsigned char)(crc ^ d[i])] ^ (crc >> 8);
	return crc;
}

static unsigned long uha_crc32(const unsigned char* d, unsigned long n)
{
	return ~uha_crc_update(0xFFFFFFFFUL, d, n);
}

static unsigned rd16le(const unsigned char* p)
{
	return (unsigned)(p[0] | (p[1] << 8));
}

static unsigned long rd24le(const unsigned char* p)
{
	return (unsigned long)(p[0] | (p[1] << 8) | (p[2] << 16));
}

static unsigned long rd32le(const unsigned char* p)
{
	return (unsigned long)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/* Packed-stream reader: one memory image over [off, off+len). */
typedef struct UhaMem
{
	const unsigned char* b;
	unsigned long n;
	unsigned long p;
} UhaMem;

static int uha_mem_read(void* ctx, unsigned char* buf, unsigned want)
{
	UhaMem* m = (UhaMem*)ctx;
	unsigned long avail = m->n - m->p;
	if( !avail )
		return 0;
	if( want > avail )
		want = (unsigned)avail;
	memcpy(buf, m->b + m->p, want);
	m->p += want;
	return (int)want;
}

/* Dynamic output buffer (decodes tables of known size). */
typedef struct UhaOut
{
	unsigned char* b;
	unsigned long n;
	unsigned long cap;
	int err;
} UhaOut;

static int uha_out_write(void* ctx, const unsigned char* buf, unsigned len)
{
	UhaOut* w = (UhaOut*)ctx;
	if( w->n + len > w->cap )
	{
		unsigned long ncap = (w->n + len) * 2 + 256;
		unsigned char* nb = (unsigned char*)realloc(w->b, ncap);
		if( !nb )
		{
			w->err = 2;
			return 1;
		}
		w->b = nb;
		w->cap = ncap;
	}
	memcpy(w->b + w->n, buf, len);
	w->n += len;
	return 0;
}

/* Decode one table image (cipher-decrypted, compressed) to dlen bytes.
 * method 0 = STORE, 2/7 = core 2 LSB (0.2 tables), 17 = core 3
 * (0.3/0.4/0.6 tables — Phase 4). Returns 0 ok.
 * Chained CRC: crc_in is updated with the decoded table on success.
 */
static int uha_decode_table(int method, int ver,
	const unsigned char* cimg, unsigned long clen,
	unsigned long dlen,
	unsigned char** out, unsigned long* outlen,
	unsigned long* crc)
{
	UhaMem mr;
	UhaOut bw;
	int rc;
	mr.b = cimg; mr.n = clen; mr.p = 0;
	bw.b = 0; bw.n = 0; bw.cap = 0; bw.err = 0;
	if( method == 0 )
	{
		unsigned char buf[8192];
		unsigned long left = dlen;
		while( left )
		{
			unsigned want = left > sizeof(buf) ? sizeof(buf) : (unsigned)left;
			int n = uha_mem_read(&mr, buf, want);
			if( n <= 0 )
			{
				free(bw.b);
				return 7;
			}
			uha_out_write(&bw, buf, (unsigned)n);
			left -= (unsigned long)n;
		}
	}
	else if( method >= 2 && method <= 7 )
	{
		rc = UhaDec2(method, 1, uha_mem_read, &mr,
			uha_out_write, &bw, dlen);
		if( rc != 0 )
		{
			free(bw.b);
			return rc == 2 ? 2 : (rc == 3 ? 7 : 5);
		}
	}
	else if( method == 17 && (ver == 3 || ver == 4 || ver == 6) )
	{
		void* ctx;
		int irc;
		irc = (ver == 3) ? UhaDec3_03Init(&ctx, method) :
			(ver == 4) ? UhaDec3_04Init(&ctx, method) :
			UhaDec3_06Init(&ctx, method);
		if( irc != 0 )
		{
			free(bw.b);
			return irc == 2 ? 2 : 7;
		}
		rc = (ver == 3) ? UhaDec3_03(ctx, NULL, 0, uha_mem_read, &mr,
			uha_out_write, &bw, dlen) :
			(ver == 4) ? UhaDec3_04(ctx, NULL, 0, uha_mem_read, &mr,
			uha_out_write, &bw, dlen) :
			UhaDec3_06(ctx, NULL, 0, uha_mem_read, &mr,
			uha_out_write, &bw, dlen);
		if( ver == 3 ) UhaDec3_03Free(ctx);
		else if( ver == 4 ) UhaDec3_04Free(ctx);
		else UhaDec3_06Free(ctx);
		if( rc != 0 )
		{
			free(bw.b);
			return rc == 2 ? 2 : (rc == 3 ? 7 : 5);
		}
	}
	else
	{
		(void)ver;
		return 6;	/* core 3/4/5 not yet ported */
	}
	if( bw.n != dlen )
	{
		free(bw.b);
		return 7;
	}
	*crc = uha_crc_update(*crc, bw.b, bw.n);
	*out = bw.b;
	*outlen = bw.n;
	return 0;
}

static void uha_free_tables(unsigned char** t, int n)
{
	int i;
	for( i = 0; i < n; i++ )
	{
		free(t[i]);
		t[i] = 0;
	}
}

void UhaClose(UhaArchive* arc)
{
	if( !arc )
		return;
	if( arc->entries )
		free(arc->entries);
	arc->entries = 0;
	arc->nfiles = 0;
	if( arc->img )
		free(arc->img);
	arc->img = 0;
	arc->imgsize = 0;
}

/* ---------- 0.2 header ---------- */
typedef struct Uha02Hdr
{
	unsigned method;
	unsigned ndirs;
	unsigned long nfiles;
	unsigned long total_unpacked;
	unsigned long data_len;
	unsigned long names_len;
	unsigned long cmp1, cmp2, cmp3;
	unsigned long contents_crc;
} Uha02Hdr;

static int uha02_parse(const unsigned char raw[48], Uha02Hdr* h)
{
	unsigned char b[48];
	unsigned long crc;
	memcpy(b, raw, 48);
	UhaHeaderDecrypt(b, 40, b[44]);
	crc = uha_crc_update(0xFFFFFFFFUL, b + 4, 40);
	if( (~crc & 0xFFFFFFFFUL) != rd32le(b + 44) )
		return 5;
	h->method = b[9];
	h->ndirs = rd16le(b + 10);
	h->nfiles = rd32le(b + 12);
	h->total_unpacked = rd32le(b + 16);
	h->data_len = rd32le(b + 20);
	h->names_len = rd32le(b + 24);
	h->cmp1 = rd32le(b + 28);
	h->cmp2 = rd32le(b + 32);
	h->cmp3 = rd32le(b + 36);
	h->contents_crc = rd32le(b + 40);
	if( h->nfiles > UHA_MAX_FILES )
		return 7;
	return 0;
}

static int uha02_open(const unsigned char* img, unsigned long flen,
	const char* pwd, UhaArchive* arc)
{
	Uha02Hdr h;
	unsigned char* tab[4];
	unsigned long tablen[4];
	unsigned long crc = 0xFFFFFFFFUL;
	unsigned long tab0_off, tab1_off, tab2_off, tab3_off;
	unsigned long end;
	unsigned long i, k;
	unsigned long p1, dirs_bytes, strs_bytes;
	char* pool;
	int rc;

	(void)pwd;	/* 0.2 has no password form */
	rc = uha02_parse(img, &h);
	if( rc != 0 )
		return rc;
	arc->ver = 2;
	arc->hdr_size = 48;
	arc->method = (int)h.method;
	arc->total_unpacked = h.total_unpacked;
	arc->packed_len = h.data_len;
	arc->exe_filter = 1;	/* 0.2: gated on MZ + len inside the filter */
	arc->mm_filter = 0;
	arc->fax_filter = 0;
	arc->flags2 = 0;
	arc->pw_tables = 0;
	arc->pw_stream = 0;

	tab0_off = 48 + h.data_len;
	tab1_off = tab0_off + 4 * h.nfiles;
	tab2_off = tab1_off + h.cmp1;
	tab3_off = tab2_off + h.cmp2;
	end = tab3_off + h.cmp3;
	if( end > flen )
		return 7;

	tab[0] = 0; tab[1] = 0; tab[2] = 0; tab[3] = 0;
	/* tab0 is STORE but still default-cipher encrypted (0.2 own) */
	{
		UhaSBox sb;
		UhaSBoxInit(&sb, NULL, 1);
		if( 4 * h.nfiles )
		{
			tab[0] = (unsigned char*)malloc(4 * h.nfiles);
			if( !tab[0] )
				return 2;
			memcpy(tab[0], img + tab0_off, 4 * h.nfiles);
			UhaSBoxUpdate(&sb, tab[0], 4 * h.nfiles);
			crc = uha_crc_update(crc, tab[0], 4 * h.nfiles);
			tablen[0] = 4 * h.nfiles;
		}
		else
		{
			tablen[0] = 0;
		}
	}
	/* tab1/tab2/tab3: cipher-decrypt the compressed image, then decode */
	{
		static const struct { unsigned long csz, dsz; int m; } t[3] = {
			{0, 0, 7}, {0, 0, 2}, {0, 0, 7}
		};
		static const unsigned long offs[3] = {0, 0, 0};
		(void)t; (void)offs;
		(void)tab1_off; (void)tab2_off; (void)tab3_off;
	}
	/* NOTE: table decode follows the same per-table fresh-S-box rule;
	 * implemented inline below (three calls, chained CRC). */
	{
		UhaSBox sb;
		unsigned char* c;
		/* tab1 */
		c = (unsigned char*)malloc(h.cmp1 ? h.cmp1 : 1);
		if( !c )
		{
			uha_free_tables(tab, 4);
			return 2;
		}
		memcpy(c, img + tab1_off, h.cmp1);
		UhaSBoxInit(&sb, NULL, 1);
		UhaSBoxUpdate(&sb, c, h.cmp1);
		rc = uha_decode_table(7, 2, c, h.cmp1, h.names_len,
			&tab[1], &tablen[1], &crc);
		free(c);
		if( rc != 0 )
		{
			uha_free_tables(tab, 4);
			return rc;
		}
		/* tab2 */
		c = (unsigned char*)malloc(h.cmp2 ? h.cmp2 : 1);
		if( !c )
		{
			uha_free_tables(tab, 4);
			return 2;
		}
		memcpy(c, img + tab2_off, h.cmp2);
		UhaSBoxInit(&sb, NULL, 1);
		UhaSBoxUpdate(&sb, c, h.cmp2);
		rc = uha_decode_table(2, 2, c, h.cmp2, 4 * h.nfiles,
			&tab[2], &tablen[2], &crc);
		free(c);
		if( rc != 0 )
		{
			uha_free_tables(tab, 4);
			return rc;
		}
		/* tab3 */
		c = (unsigned char*)malloc(h.cmp3 ? h.cmp3 : 1);
		if( !c )
		{
			uha_free_tables(tab, 4);
			return 2;
		}
		memcpy(c, img + tab3_off, h.cmp3);
		UhaSBoxInit(&sb, NULL, 1);
		UhaSBoxUpdate(&sb, c, h.cmp3);
		rc = uha_decode_table(7, 2, c, h.cmp3, 7 * h.nfiles,
			&tab[3], &tablen[3], &crc);
		free(c);
		if( rc != 0 )
		{
			uha_free_tables(tab, 4);
			return rc;
		}
	}
	if( (~crc & 0xFFFFFFFFUL) != (h.contents_crc & 0xFFFFFFFFUL) )
	{
		uha_free_tables(tab, 4);
		return 5;
	}

	/* entries: tab1 = ndirs + N Pascal strings; tab2 = times then
	 * dates; tab3 = 4N deltas, 2N dir bytes, N method bytes;
	 * tab0 = N u32 CRCs. Sizes reconstruct backwards. */
	arc->entries = (UhaEntry*)calloc(h.nfiles ? h.nfiles : 1,
		sizeof(UhaEntry));
	if( !arc->entries )
	{
		uha_free_tables(tab, 4);
		return 2;
	}
	arc->nfiles = h.nfiles;
	{
		unsigned long* diroffs = 0;
		if( h.ndirs )
		{
			diroffs = (unsigned long*)malloc(sizeof(unsigned long) * h.ndirs);
			if( !diroffs )
			{
				uha_free_tables(tab, 4);
				UhaClose(arc);
				return 2;
			}
		}
		p1 = 0; dirs_bytes = 0; strs_bytes = 0;
		for( k = 0; k < h.ndirs; k++ )
		{
			unsigned l;
			if( p1 >= h.names_len ) break;
			l = tab[1][p1++];
			if( p1 + l > h.names_len ) break;
			p1 += l;
			dirs_bytes += l + 1;
		}
		for( i = 0; i < h.nfiles; i++ )
		{
			unsigned l;
			if( p1 >= h.names_len ) break;
			l = tab[1][p1++];
			if( p1 + l > h.names_len ) break;
			p1 += l;
			strs_bytes += l + 1;
		}
		pool = (char*)malloc(dirs_bytes + strs_bytes + h.nfiles * 260 + 1024);
		if( !pool )
		{
			free(diroffs);
			uha_free_tables(tab, 4);
			UhaClose(arc);
			return 2;
		}
		p1 = 0;
		{
			unsigned long p2 = 0;
			for( k = 0; k < h.ndirs; k++ )
			{
				unsigned l = tab[1][p1++];
				diroffs[k] = p2;
				memcpy(pool + p2, tab[1] + p1, l);
				pool[p2 + l] = 0;
				p1 += l; p2 += l + 1;
			}
		}
		{
			char* sp = pool + dirs_bytes;
			unsigned long prev_sz = 0;
			p1 = 0;
			for( k = 0; k < h.ndirs; k++ )
			{
				unsigned l = tab[1][p1++];
				p1 += l;
			}
			for( i = 0; i < h.nfiles; i++ )
			{
				UhaEntry* e = &arc->entries[i];
				unsigned l = tab[1][p1++];
				unsigned tt, dd, w0;
				unsigned long var, sz;
				memcpy(sp, tab[1] + p1, l);
				sp[l] = 0;
				p1 += l;
				e->name[0] = 0;
				strncpy(e->name, sp, sizeof(e->name) - 1);
				e->name[sizeof(e->name) - 1] = 0;
				tt = rd16le(tab[2] + 2 * i);
				dd = rd16le(tab[2] + 2 * h.nfiles + 2 * i);
				w0 = (unsigned)(tab[3][4 * h.nfiles + i] |
					(tab[3][5 * h.nfiles + i] << 8));
				var = rd32le(tab[3] + 4 * i);
				e->time = tt;
				e->date = dd;
				e->method = tab[3][6 * h.nfiles + i];
				e->crc = rd32le(tab[0] + 4 * i);
				sz = (i == 0) ? (0UL - var) : (prev_sz - var);
				e->size = sz & 0xFFFFFFFFUL;
				prev_sz = e->size;
				e->dir[0] = 0;
				e->path[0] = 0;
				if( (w0 & 0xFFFF) < h.ndirs && h.ndirs )
				{
					strncpy(e->dir, pool + diroffs[w0 & 0xFFFF],
						sizeof(e->dir) - 1);
					e->dir[sizeof(e->dir) - 1] = 0;
				}
				if( e->dir[0] )
				{
					char* dend;
					strncpy(e->path, e->dir, sizeof(e->path) - 1);
					e->path[sizeof(e->path) - 1] = 0;
					dend = e->path + strlen(e->path);
					while( dend > e->path &&
						(dend[-1] == '/' || dend[-1] == '\\') )
						*--dend = 0;
					strncat(e->path, "/", sizeof(e->path) - strlen(e->path) - 1);
					strncat(e->path, e->name, sizeof(e->path) - strlen(e->path) - 1);
				}
				else
				{
					strncpy(e->path, e->name, sizeof(e->path) - 1);
					e->path[sizeof(e->path) - 1] = 0;
				}
				{
					char* s = e->path;
					while( *s )
					{
						if( *s == '\\' ) *s = '/';
						s++;
					}
				}
				sp += l + 1;
			}
		}
		{
			unsigned long off = 0;
			for( i = 0; i < h.nfiles; i++ )
			{
				arc->entries[i].offset = off;
				off = (off + arc->entries[i].size) & 0xFFFFFFFFUL;
			}
		}
		free(diroffs);
		free(pool);
	}
	uha_free_tables(tab, 4);
	return 0;
}

/* ---------- 0.6 header ---------- */
typedef struct Uha06Hdr
{
	unsigned ndirs;
	unsigned long total_unpacked;
	unsigned long data_off;
	unsigned long nfiles;
	unsigned long tab1_dec;
	unsigned long tab1_cmp;
	unsigned long tab2_cmp;
	unsigned tab1_crc;
	unsigned tab2_crc;
} Uha06Hdr;

static int uha06_parse(const unsigned char raw[39], Uha06Hdr* h,
	UhaArchive* arc)
{
	unsigned char b[39];
	unsigned long crc;
	memcpy(b, raw, 39);
	UhaHeaderDecrypt(b, 31, b[37]);
	crc = uha_crc_update(0xFFFFFFFFUL, b + 4, 31);
	if( ((~crc & 0xFFFFUL)) != rd16le(b + 37) )
		return 5;
	arc->method = b[10];
	arc->exe_filter = (b[9] & 1) ? 1 : 0;
	arc->mm_filter = (b[9] & 2) ? 1 : 0;
	arc->fax_filter = (b[9] & 0x80) ? 1 : 0;
	arc->flags2 = b[9];
	arc->pw_tables = (b[8] & 4) ? 1 : 0;
	arc->pw_stream = (b[8] & 2) ? 1 : 0;
	h->ndirs = rd16le(b + 11);
	h->total_unpacked = rd32le(b + 13);
	h->data_off = rd32le(b + 17);
	h->nfiles = rd24le(b + 21);
	h->tab1_dec = rd24le(b + 24);
	h->tab1_cmp = rd24le(b + 27);
	h->tab2_cmp = rd24le(b + 30);
	h->tab1_crc = rd16le(b + 33);
	h->tab2_crc = rd16le(b + 35);
	if( h->nfiles > UHA_MAX_FILES )
		return 7;
	return 0;
}

static int uha06_open(const unsigned char* img, unsigned long flen,
	const char* pwd, UhaArchive* arc)
{
	Uha06Hdr h;
	unsigned char* tab[3];
	unsigned long tablen[3];
	unsigned long crc;
	unsigned long tab0_off, tab1_off, tab2_off;
	unsigned long end;
	unsigned long i, k;
	int rc;

	if( flen < 39 )
		return 7;
	rc = uha06_parse(img, &h, arc);
	if( rc != 0 )
		return rc;
	arc->ver = 6;
	arc->hdr_size = 39;
	arc->total_unpacked = h.total_unpacked;
	arc->packed_len = h.data_off;
	if( arc->pw_tables )
	{
		if( !pwd || !*pwd )
			return 8;
	}
	tab0_off = 39 + h.data_off;
	tab1_off = tab0_off + 4 * h.nfiles;
	tab2_off = tab1_off + h.tab1_cmp;
	end = tab2_off + h.tab2_cmp;
	if( end > flen )
		return 7;

	tab[0] = 0; tab[1] = 0; tab[2] = 0;
	/* tab0: STORE, still encrypted with the table cipher (6.2;
	 * password form when pw_tables is set). */
	{
		UhaSBox sb;
		UhaSBoxInit(&sb, arc->pw_tables ? pwd : NULL, 0);
		if( 4 * h.nfiles )
		{
			tab[0] = (unsigned char*)malloc(4 * h.nfiles);
			if( !tab[0] )
				return 2;
			memcpy(tab[0], img + tab0_off, 4 * h.nfiles);
			UhaSBoxUpdate(&sb, tab[0], 4 * h.nfiles);
		}
		tablen[0] = 4 * h.nfiles;
	}
	{
		unsigned char* c;
		UhaSBox sb;
		c = (unsigned char*)malloc(h.tab1_cmp ? h.tab1_cmp : 1);
		if( !c )
		{
			uha_free_tables(tab, 3);
			return 2;
		}
		memcpy(c, img + tab1_off, h.tab1_cmp);
		UhaSBoxInit(&sb, arc->pw_tables ? pwd : NULL, 0);
		UhaSBoxUpdate(&sb, c, h.tab1_cmp);
		crc = 0xFFFFFFFFUL;
		if( tablen[0] )
			crc = uha_crc_update(crc, tab[0], tablen[0]);
		rc = uha_decode_table(17, 6, c, h.tab1_cmp, h.tab1_dec,
			&tab[1], &tablen[1], &crc);
		free(c);
		if( rc != 0 )
		{
			uha_free_tables(tab, 3);
			return rc;
		}
		if( ((~crc & 0xFFFFUL)) != h.tab1_crc )
		{
			uha_free_tables(tab, 3);
			return 5;
		}
		c = (unsigned char*)malloc(h.tab2_cmp ? h.tab2_cmp : 1);
		if( !c )
		{
			uha_free_tables(tab, 3);
			return 2;
		}
		memcpy(c, img + tab2_off, h.tab2_cmp);
		UhaSBoxInit(&sb, arc->pw_tables ? pwd : NULL, 0);
		UhaSBoxUpdate(&sb, c, h.tab2_cmp);
		crc = 0xFFFFFFFFUL;
		rc = uha_decode_table(17, 6, c, h.tab2_cmp, 11 * h.nfiles,
			&tab[2], &tablen[2], &crc);
		free(c);
		if( rc != 0 )
		{
			uha_free_tables(tab, 3);
			return rc;
		}
		if( ((~crc & 0xFFFFUL)) != h.tab2_crc )
		{
			uha_free_tables(tab, 3);
			return 5;
		}
	}
	/* entries: tab1 = dirs + names; tab2 planes time/date/dir/size/method */
	arc->entries = (UhaEntry*)calloc(h.nfiles ? h.nfiles : 1,
		sizeof(UhaEntry));
	if( !arc->entries )
	{
		uha_free_tables(tab, 3);
		return 2;
	}
	arc->nfiles = h.nfiles;
	{
		unsigned long* diroffs = 0;
		unsigned long p1, dirs_bytes, strs_bytes;
		char* pool;
		if( h.ndirs )
		{
			diroffs = (unsigned long*)malloc(sizeof(unsigned long) * h.ndirs);
			if( !diroffs )
			{
				uha_free_tables(tab, 3);
				UhaClose(arc);
				return 2;
			}
		}
		p1 = 0; dirs_bytes = 0; strs_bytes = 0;
		for( k = 0; k < h.ndirs; k++ )
		{
			unsigned l;
			if( p1 >= h.tab1_dec ) break;
			l = tab[1][p1++];
			if( p1 + l > h.tab1_dec ) break;
			p1 += l;
			dirs_bytes += l + 1;
		}
		for( i = 0; i < h.nfiles; i++ )
		{
			unsigned l;
			if( p1 >= h.tab1_dec ) break;
			l = tab[1][p1++];
			if( p1 + l > h.tab1_dec ) break;
			p1 += l;
			strs_bytes += l + 1;
		}
		pool = (char*)malloc(dirs_bytes + strs_bytes + h.nfiles * 260 + 1024);
		if( !pool )
		{
			free(diroffs);
			uha_free_tables(tab, 3);
			UhaClose(arc);
			return 2;
		}
		p1 = 0;
		{
			unsigned long p2 = 0;
			for( k = 0; k < h.ndirs; k++ )
			{
				unsigned l = tab[1][p1++];
				diroffs[k] = p2;
				memcpy(pool + p2, tab[1] + p1, l);
				pool[p2 + l] = 0;
				p1 += l; p2 += l + 1;
			}
		}
		{
			char* sp = pool + dirs_bytes;
			unsigned long prev_sz = 0;
			p1 = 0;
			for( k = 0; k < h.ndirs; k++ )
			{
				unsigned l = tab[1][p1++];
				p1 += l;
			}
			for( i = 0; i < h.nfiles; i++ )
			{
				UhaEntry* e = &arc->entries[i];
				unsigned l = tab[1][p1++];
				unsigned tt, dd, w0;
				unsigned long var, sz;
				memcpy(sp, tab[1] + p1, l);
				sp[l] = 0;
				p1 += l;
				strncpy(e->name, sp, sizeof(e->name) - 1);
				e->name[sizeof(e->name) - 1] = 0;
				tt = rd16le(tab[2] + 2 * i);
				dd = rd16le(tab[2] + 2 * h.nfiles + 2 * i);
				w0 = (unsigned)(tab[2][4 * h.nfiles + i] |
					(tab[2][5 * h.nfiles + i] << 8));
				var = (unsigned long)(tab[2][6 * h.nfiles + i] | (tab[2][7 * h.nfiles + i] << 8) | (tab[2][8 * h.nfiles + i] << 16) | (tab[2][9 * h.nfiles + i] << 24));
				e->time = tt;
				e->date = dd;
				e->method = tab[2][10 * h.nfiles + i];
				e->crc = rd32le(tab[0] + 4 * i);
				sz = (i == 0) ? (0UL - var) : (prev_sz - var);
				e->size = sz & 0xFFFFFFFFUL;
				prev_sz = e->size;
				e->dir[0] = 0;
				e->path[0] = 0;
				if( (w0 & 0xFFFF) < h.ndirs && h.ndirs )
				{
					strncpy(e->dir, pool + diroffs[w0 & 0xFFFF],
						sizeof(e->dir) - 1);
					e->dir[sizeof(e->dir) - 1] = 0;
				}
				if( e->dir[0] )
				{
					char* dend;
					strncpy(e->path, e->dir, sizeof(e->path) - 1);
					e->path[sizeof(e->path) - 1] = 0;
					dend = e->path + strlen(e->path);
					while( dend > e->path &&
						(dend[-1] == '/' || dend[-1] == '\\') )
						*--dend = 0;
					strncat(e->path, "/", sizeof(e->path) - strlen(e->path) - 1);
					strncat(e->path, e->name, sizeof(e->path) - strlen(e->path) - 1);
				}
				else
				{
					strncpy(e->path, e->name, sizeof(e->path) - 1);
					e->path[sizeof(e->path) - 1] = 0;
				}
				sp += l + 1;
			}
		}
		{
			unsigned long off = 0;
			for( i = 0; i < h.nfiles; i++ )
			{
				arc->entries[i].offset = off;
				off = (off + arc->entries[i].size) & 0xFFFFFFFFUL;
			}
		}
		free(diroffs);
		free(pool);
	}
	uha_free_tables(tab, 3);
	return 0;
}


/* ---------- 0.3 / 0.4 header (44 bytes, shared layout) ----------
 * FORMAT.md sections 3.1/4.1: the 0.3 and 0.4 headers are identical
 * except the magic byte. Encrypted range 4..39 (36 bytes), key
 * raw[42], low-16 CRC over the decrypted range. Tables are 4
 * (tab0 4N STORE, tab1 names_len method 17, tab2 4N method 2 dates
 * then times, tab3 7N method 7 byte planes). CRC chaining: crc1
 * over tab0+tab1, crc2 over tab2, crc3 over tab3.
 */
typedef struct Uha34Hdr
{
	unsigned ndirs;
	unsigned long total_unpacked;
	unsigned long base_e01;
	unsigned long nfiles;
	unsigned long names_len;
	unsigned long cmp1;
	unsigned long cmp2;
	unsigned long cmp3;
	unsigned crc1;
	unsigned crc2;
	unsigned crc3;
} Uha34Hdr;

static int uha34_parse(const unsigned char raw[44], int ver,
	Uha34Hdr* h, UhaArchive* arc)
{
	unsigned char b[44];
	unsigned long crc;
	memcpy(b, raw, 44);
	UhaHeaderDecrypt(b, 36, b[42]);
	crc = uha_crc_update(0xFFFFFFFFUL, b + 4, 36);
	if( ((~crc & 0xFFFFUL)) != rd16le(b + 42) )
		return 5;
	arc->method = b[10];
	arc->exe_filter = (b[9] & 1) ? 1 : 0;
	arc->mm_filter = (b[9] & 2) ? 1 : 0;
	arc->fax_filter = (b[9] & 0x80) ? 1 : 0;
	arc->flags2 = b[9];
	arc->pw_tables = (b[8] & 2) ? 1 : 0;
	arc->pw_stream = (b[8] & 2) ? 1 : 0;
	h->ndirs = rd16le(b + 11);
	h->total_unpacked = rd32le(b + 13);
	h->base_e01 = rd32le(b + 17);
	h->nfiles = rd24le(b + 21);
	h->names_len = rd24le(b + 24);
	h->cmp1 = rd24le(b + 27);
	h->cmp2 = rd24le(b + 30);
	h->cmp3 = rd24le(b + 33);
	h->crc1 = rd16le(b + 36);
	h->crc2 = rd16le(b + 38);
	h->crc3 = rd16le(b + 40);
	if( h->nfiles > UHA_MAX_FILES )
		return 7;
	(void)ver;
	return 0;
}

static int uha34_open(const unsigned char* img, unsigned long flen,
	const char* pwd, UhaArchive* arc)
{
	Uha34Hdr h;
	unsigned char* tab[4];
	unsigned long tablen[4];
	unsigned long crc;
	unsigned long tab0_off, tab1_off, tab2_off, tab3_off;
	unsigned long end;
	unsigned long i, k;
	int rc;

	if( flen < 44 )
		return 7;
	rc = uha34_parse(img, arc->ver, &h, arc);
	if( rc != 0 )
		return rc;
	arc->hdr_size = 44;
	arc->total_unpacked = h.total_unpacked;
	arc->packed_len = h.base_e01;
	if( arc->pw_tables )
	{
		if( !pwd || !*pwd )
			return 8;
	}
	tab0_off = 44 + h.base_e01;
	tab1_off = tab0_off + 4 * h.nfiles;
	tab2_off = tab1_off + h.cmp1;
	tab3_off = tab2_off + h.cmp2;
	end = tab3_off + h.cmp3;
	if( end > flen )
		return 7;

	tab[0] = 0; tab[1] = 0; tab[2] = 0; tab[3] = 0;
	/* tab0: STORE, still encrypted with the table cipher (6.2;
	 * password form when pw_tables is set). */
	{
		UhaSBox sb;
		UhaSBoxInit(&sb, arc->pw_tables ? pwd : NULL, 0);
		if( 4 * h.nfiles )
		{
			tab[0] = (unsigned char*)malloc(4 * h.nfiles);
			if( !tab[0] )
				return 2;
			memcpy(tab[0], img + tab0_off, 4 * h.nfiles);
			UhaSBoxUpdate(&sb, tab[0], 4 * h.nfiles);
		}
		tablen[0] = 4 * h.nfiles;
	}
	{
		unsigned char* c;
		UhaSBox sb;
		c = (unsigned char*)malloc(h.cmp1 ? h.cmp1 : 1);
		if( !c )
		{
			uha_free_tables(tab, 4);
			return 2;
		}
		memcpy(c, img + tab1_off, h.cmp1);
		UhaSBoxInit(&sb, arc->pw_tables ? pwd : NULL, 0);
		UhaSBoxUpdate(&sb, c, h.cmp1);
		crc = 0xFFFFFFFFUL;
		if( tablen[0] )
			crc = uha_crc_update(crc, tab[0], tablen[0]);
		rc = uha_decode_table(17, arc->ver, c, h.cmp1, h.names_len,
			&tab[1], &tablen[1], &crc);
		free(c);
		if( rc != 0 )
		{
			uha_free_tables(tab, 4);
			return rc;
		}
		if( ((~crc & 0xFFFFUL)) != h.crc1 )
		{
			uha_free_tables(tab, 4);
			return 5;
		}
		c = (unsigned char*)malloc(h.cmp2 ? h.cmp2 : 1);
		if( !c )
		{
			uha_free_tables(tab, 4);
			return 2;
		}
		memcpy(c, img + tab2_off, h.cmp2);
		UhaSBoxInit(&sb, arc->pw_tables ? pwd : NULL, 0);
		UhaSBoxUpdate(&sb, c, h.cmp2);
		crc = 0xFFFFFFFFUL;
		rc = uha_decode_table(2, arc->ver, c, h.cmp2, 4 * h.nfiles,
			&tab[2], &tablen[2], &crc);
		free(c);
		if( rc != 0 )
		{
			uha_free_tables(tab, 4);
			return rc;
		}
		if( ((~crc & 0xFFFFUL)) != h.crc2 )
		{
			uha_free_tables(tab, 4);
			return 5;
		}
		c = (unsigned char*)malloc(h.cmp3 ? h.cmp3 : 1);
		if( !c )
		{
			uha_free_tables(tab, 4);
			return 2;
		}
		memcpy(c, img + tab3_off, h.cmp3);
		UhaSBoxInit(&sb, arc->pw_tables ? pwd : NULL, 0);
		UhaSBoxUpdate(&sb, c, h.cmp3);
		crc = 0xFFFFFFFFUL;
		rc = uha_decode_table(7, arc->ver, c, h.cmp3, 7 * h.nfiles,
			&tab[3], &tablen[3], &crc);
		free(c);
		if( rc != 0 )
		{
			uha_free_tables(tab, 4);
			return rc;
		}
		if( ((~crc & 0xFFFFUL)) != h.crc3 )
		{
			uha_free_tables(tab, 4);
			return 5;
		}
	}
	/* entries: 0.3/0.4 tab2 = dates then times; tab3 byte planes */
	arc->entries = (UhaEntry*)calloc(h.nfiles ? h.nfiles : 1,
		sizeof(UhaEntry));
	if( !arc->entries )
	{
		uha_free_tables(tab, 4);
		return 2;
	}
	arc->nfiles = h.nfiles;
	{
		unsigned long* diroffs = 0;
		unsigned long p1, dirs_bytes, strs_bytes;
		char* pool;
		if( h.ndirs )
		{
			diroffs = (unsigned long*)malloc(sizeof(unsigned long) * h.ndirs);
			if( !diroffs )
			{
				uha_free_tables(tab, 4);
				UhaClose(arc);
				return 2;
			}
		}
		p1 = 0; dirs_bytes = 0; strs_bytes = 0;
		for( k = 0; k < h.ndirs; k++ )
		{
			unsigned l;
			if( p1 >= h.names_len ) break;
			l = tab[1][p1++];
			if( p1 + l > h.names_len ) break;
			p1 += l;
			dirs_bytes += l + 1;
		}
		for( i = 0; i < h.nfiles; i++ )
		{
			unsigned l;
			if( p1 >= h.names_len ) break;
			l = tab[1][p1++];
			if( p1 + l > h.names_len ) break;
			p1 += l;
			strs_bytes += l + 1;
		}
		pool = (char*)malloc(dirs_bytes + strs_bytes + h.nfiles * 260 + 1024);
		if( !pool )
		{
			free(diroffs);
			uha_free_tables(tab, 4);
			UhaClose(arc);
			return 2;
		}
		p1 = 0;
		{
			unsigned long p2 = 0;
			for( k = 0; k < h.ndirs; k++ )
			{
				unsigned l = tab[1][p1++];
				diroffs[k] = p2;
				memcpy(pool + p2, tab[1] + p1, l);
				pool[p2 + l] = 0;
				p1 += l; p2 += l + 1;
			}
		}
		{
			char* sp = pool + dirs_bytes;
			unsigned long prev_sz = 0;
			p1 = 0;
			for( k = 0; k < h.ndirs; k++ )
			{
				unsigned l = tab[1][p1++];
				p1 += l;
			}
			for( i = 0; i < h.nfiles; i++ )
			{
				UhaEntry* e = &arc->entries[i];
				unsigned l = tab[1][p1++];
				unsigned tt, dd, w0;
				unsigned long var, sz;
				memcpy(sp, tab[1] + p1, l);
				sp[l] = 0;
				p1 += l;
				strncpy(e->name, sp, sizeof(e->name) - 1);
				e->name[sizeof(e->name) - 1] = 0;
				dd = rd16le(tab[2] + 2 * i);
				tt = rd16le(tab[2] + 2 * h.nfiles + 2 * i);
				w0 = (unsigned)(tab[3][i] | (tab[3][h.nfiles + i] << 8));
				var = (unsigned long)(tab[3][2 * h.nfiles + i] |
					((unsigned long)tab[3][3 * h.nfiles + i] << 8) |
					((unsigned long)tab[3][4 * h.nfiles + i] << 16) |
					((unsigned long)tab[3][5 * h.nfiles + i] << 24));
				e->time = tt;
				e->date = dd;
				e->method = tab[3][6 * h.nfiles + i];
				e->crc = rd32le(tab[0] + 4 * i);
				sz = (i == 0) ? (0UL - var) : (prev_sz - var);
				e->size = sz & 0xFFFFFFFFUL;
				prev_sz = e->size;
				e->dir[0] = 0;
				e->path[0] = 0;
				if( (w0 & 0xFFFF) < h.ndirs && h.ndirs )
				{
					strncpy(e->dir, pool + diroffs[w0 & 0xFFFF],
						sizeof(e->dir) - 1);
					e->dir[sizeof(e->dir) - 1] = 0;
				}
				if( e->dir[0] )
				{
					char* dend;
					strncpy(e->path, e->dir, sizeof(e->path) - 1);
					e->path[sizeof(e->path) - 1] = 0;
					dend = e->path + strlen(e->path);
					while( dend > e->path &&
						(dend[-1] == '/' || dend[-1] == '\\') )
						*--dend = 0;
					strncat(e->path, "/", sizeof(e->path) - strlen(e->path) - 1);
					strncat(e->path, e->name, sizeof(e->path) - strlen(e->path) - 1);
				}
				else
				{
					strncpy(e->path, e->name, sizeof(e->path) - 1);
					e->path[sizeof(e->path) - 1] = 0;
				}
				sp += l + 1;
			}
		}
		{
			unsigned long off = 0;
			for( i = 0; i < h.nfiles; i++ )
			{
				arc->entries[i].offset = off;
				off = (off + arc->entries[i].size) & 0xFFFFFFFFUL;
			}
		}
		free(diroffs);
		free(pool);
	}
	uha_free_tables(tab, 4);
	return 0;
}

int UhaOpen(const char* path, const char* pwd, UhaArchive* arc)
{
	FILE* fp;
	unsigned char* img;
	long fpos;
	unsigned long flen;
	int ver;
	int rc;

	if( !path || !arc )
		return 1;
	memset(arc, 0, sizeof(*arc));
	fp = fopen(path, "rb");
	if( !fp )
		return 1;
	if( fseek(fp, 0, SEEK_END) != 0 )
	{
		fclose(fp);
		return 1;
	}
	fpos = ftell(fp);
	if( fpos < 39 )
	{
		fclose(fp);
		return 3;
	}
	flen = (unsigned long)fpos;
	if( flen > UHA_MAX_ARCHIVE )
	{
		fclose(fp);
		return 7;
	}
	if( fseek(fp, 0, SEEK_SET) != 0 )
	{
		fclose(fp);
		return 1;
	}
	img = (unsigned char*)malloc(flen);
	if( !img )
	{
		fclose(fp);
		return 2;
	}
	if( fread(img, 1, flen, fp) != flen )
	{
		free(img);
		fclose(fp);
		return 1;
	}
	fclose(fp);

	if( !(img[0] == 0x55 && img[1] == 0x48 && img[2] == 0x41) )
	{
		free(img);
		return 3;
	}
	ver = img[3];
	arc->ver = ver;
	if( ver == 2 )
		rc = uha02_open(img, flen, pwd, arc);
	else if( ver == 6 )
		rc = uha06_open(img, flen, pwd, arc);
	else if( ver == 3 || ver == 4 )
		rc = uha34_open(img, flen, pwd, arc);
	else
		rc = 4;
	if( rc == 0 )
	{
		arc->img = img;
		arc->imgsize = flen;
	}
	else
	{
		free(img);
	}
	return rc;
}

/* Solid decode dispatch: decode the whole stream into one buffer. */
int UhaDecodeSolid(UhaArchive* arc, const unsigned char* packed,
	unsigned char** out, unsigned long* outlen)
{
	UhaMem mr;
	UhaOut bw;
	unsigned long* cum = 0;
	unsigned long ncum = 0;
	unsigned long i, acc;
	int core, rc;

	if( !arc || !packed || !out || !outlen )
		return 1;
	*out = 0;
	*outlen = 0;
	if( arc->total_unpacked == 0 )
	{
		*outlen = 0;
		return 0;
	}
	if( arc->total_unpacked > UHA_MAX_MEMBER )
		return 7;
	mr.b = packed;
	mr.n = arc->packed_len;
	mr.p = 0;
	bw.b = 0; bw.n = 0; bw.cap = 0; bw.err = 0;

	/* cumulative file-end table for model resets (cores 3/5) */
	if( arc->nfiles && arc->nfiles <= UHA_MAX_FILES )
	{
		cum = (unsigned long*)malloc(sizeof(unsigned long) * arc->nfiles);
		if( !cum )
			return 2;
		acc = 0;
		for( i = 0; i < arc->nfiles; i++ )
		{
			acc = (acc + arc->entries[i].size) & 0xFFFFFFFFUL;
			cum[i] = acc;
		}
		if( cum[arc->nfiles - 1] != arc->total_unpacked )
			cum[arc->nfiles - 1] = arc->total_unpacked;
		ncum = arc->nfiles;
	}

	core = UhaCoreOf(arc->method, arc->ver);
	switch( core )
	{
	case 0:
		rc = UhaDecStore(uha_mem_read, &mr, uha_out_write, &bw,
			arc->total_unpacked);
		break;
	case 1:
		rc = UhaDecRle(uha_mem_read, &mr, uha_out_write, &bw,
			arc->total_unpacked);
		break;
	case 2:
		rc = UhaDec2(arc->method, arc->ver == 6 ? 0 : 1,
			uha_mem_read, &mr, uha_out_write, &bw,
			arc->total_unpacked);
		break;
	case 3:
		if( arc->ver == 6 )
		{
			void* ctx;
			int irc = UhaDec3_06Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec3_06(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec3_06Free(ctx);
		}
		else if( arc->ver == 2 )
		{
			void* ctx;
			int irc = UhaDec3_02Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec3_02(ctx, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec3_02Free(ctx);
		}
		else if( arc->ver == 3 )
		{
			void* ctx;
			int irc = UhaDec3_03Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec3_03(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec3_03Free(ctx);
		}
		else if( arc->ver == 4 )
		{
			void* ctx;
			int irc = UhaDec3_04Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec3_04(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec3_04Free(ctx);
		}
		else
		{
			rc = 6;
		}
		break;
	case 4:
		if( arc->ver == 6 )
		{
			void* ctx;
			UhaDec4_06SetFlags2( arc->flags2 );
			int irc = UhaDec4_06Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec4_06(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec4_06Free(ctx);
		}
		else if( arc->ver == 4 )
		{
			void* ctx;
			UhaDec4_04SetFlags2( arc->flags2 );
			int irc = UhaDec4_04Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec4_04(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec4_04Free(ctx);
		}
		else if( arc->ver == 3 )
		{
			void* ctx;
			UhaDec4_03SetFlags2( arc->flags2 );
			int irc = UhaDec4_03Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec4_03(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec4_03Free(ctx);
		}
		else
		{
			rc = 6;
		}
		break;
	case 5:
		if( arc->ver == 6 )
		{
			void* ctx;
			int irc = UhaDec5_06Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec5_06(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec5_06Free(ctx);
		}
		else if( arc->ver == 4 )
		{
			void* ctx;
			int irc = UhaDec5_04Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec5_04(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec5_04Free(ctx);
		}
		else if( arc->ver == 3 )
		{
			void* ctx;
			int irc = UhaDec5_03Init(&ctx, arc->method);
			if( irc != 0 )
			{
				free(cum);
				free(bw.b);
				return irc == 2 ? 2 : 7;
			}
			rc = UhaDec5_03(ctx, cum, ncum, uha_mem_read, &mr,
				uha_out_write, &bw, arc->total_unpacked);
			UhaDec5_03Free(ctx);
		}
		else
		{
			rc = 6;
		}
		break;
	default:
		rc = 6;
		break;
	}
	free(cum);
	if( rc != 0 )
	{
		free(bw.b);
		return rc == 2 ? 2 : (rc == 3 ? 7 : 5);
	}
	if( bw.n != arc->total_unpacked )
	{
		free(bw.b);
		return 7;
	}
	*out = bw.b;
	*outlen = bw.n;
	return 0;
}

/* EXE post-filter (FORMAT 6.4). */
static void UhaExeFilter345(UhaArchive* arc, unsigned char* buf, unsigned long len);

void UhaExeFilter(UhaArchive* arc, unsigned char* buf, unsigned long len)
{
	long v2, i, end;
	if( !arc )
		return;
	if( arc->ver != 2 )
	{
		UhaExeFilter345(arc, buf, len);
		return;
	}
	if( arc->method == 0 )
		return;
	if( !buf || len <= 15 )
		return;
	if( !((buf[0] == 0x4D && buf[1] == 0x5A) ||
		(buf[0] == 0x5A && buf[1] == 0x4D)) )
		return;
	v2 = (long)len;
	end = (long)len - 15;
	i = 0;
	while( i < end )
	{
		long v7, adv, v9;
		if( buf[i] != 0xE8 && buf[i] != 0xE9 )
		{
			i++;
			continue;
		}
		v7 = (long)((unsigned long)buf[i + 1] |
			((unsigned long)buf[i + 2] << 8) |
			((unsigned long)buf[i + 3] << 16) |
			((unsigned long)buf[i + 4] << 24));
		if( v7 >= 0x80000000L )
			v7 -= 0x100000000L;
		adv = 4;
		if( v7 != 0 )
		{
			int done = 0;
			if( v7 > 0 && v7 <= v2 - 1 )
			{
				long t = v7;
				if( t == i )
					t ^= i;
				v9 = t - i;
				done = 1;
			}
			else if( v7 < 0 && v7 >= -i )
			{
				v9 = v2 + v7;
				done = 1;
			}
			if( done )
			{
				unsigned long u = (unsigned long)v9;
				buf[i + 1] = (unsigned char)(u & 0xFFUL);
				buf[i + 2] = (unsigned char)((u >> 8) & 0xFFUL);
				buf[i + 3] = (unsigned char)((u >> 16) & 0xFFUL);
				buf[i + 4] = (unsigned char)((u >> 24) & 0xFFUL);
				adv = 5;
			}
		}
		i += adv;
	}
}


/* ---- multimedia inverse filter (BMP/WAV/VOC), FORMAT 6.5 ---- */
/* Port of the reference uha02_mm_unfilter: the delta + sign-fold
 * inverse. The first 36 bytes are signature detection and stay
 * untouched; period (1/2/3/4) and the predictor shift come from
 * the header bytes (BMP->3, VOC->1, WAV seed 1/2 doubled at 16
 * bits/sample). The fold toggle depends only on the stored byte,
 * so the transform inverts by construction. Never call for 0.2. */

#define UHA_MM_WILD 0x3F

static const unsigned char uha_mm_sig_bmp[36] = {
	0x42, 0x4D, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
	0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
	0x3F, 0x3F, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x3F
};

static const unsigned char uha_mm_sig_wav[36] = {
	0x52, 0x49, 0x46, 0x46, 0x3F, 0x3F, 0x3F, 0x3F, 0x57, 0x41, 0x56, 0x45,
	0x66, 0x6D, 0x74, 0x20, 0x3F, 0x3F, 0x3F, 0x3F, 0x01, 0x00, 0x3F, 0x00,
	0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x00
};

static const unsigned char uha_mm_sig_voc[36] = {
	0x43, 0x72, 0x65, 0x61, 0x74, 0x69, 0x76, 0x65, 0x20, 0x56, 0x6F, 0x69,
	0x63, 0x65, 0x20, 0x46, 0x69, 0x6C, 0x65, 0x1A, 0x1A, 0x00, 0x0A, 0x01,
	0x29, 0x11, 0x01, 0x3F, 0x3F, 0x3F, 0x3F, 0x00, 0x3F, 0x3F, 0x3F, 0x3F
};

void UhaMmFilter(unsigned char* buf, unsigned long len)
{
	unsigned long i;
	unsigned long v4;
	unsigned period = 0;
	unsigned shift = 0;
	unsigned char fold[8];
	unsigned is_bmp = 1, is_voc = 1, is_wav = 1;
	unsigned k;

	if( !buf || len < 16 )
		return;
	for( k = 0; k < 8; k++ )
		fold[k] = 0;
	v4 = 0;

	for( i = 0; i < 36 && i < len; i++ )
	{
		unsigned char cc = buf[i];
		if( is_bmp && uha_mm_sig_bmp[i] != UHA_MM_WILD &&
			uha_mm_sig_bmp[i] != cc )
			is_bmp = 0;
		if( is_voc && uha_mm_sig_voc[i] != UHA_MM_WILD &&
			uha_mm_sig_voc[i] != cc )
			is_voc = 0;
		if( is_wav && uha_mm_sig_wav[i] != UHA_MM_WILD &&
			uha_mm_sig_wav[i] != cc )
			is_wav = 0;
		if( !is_bmp && !is_voc && !is_wav )
			return;
		if( i == 22 )
		{
			period = cc;
			v4 = 0;
			for( k = 0; k < 8; k++ )
				fold[k] = 0;
			if( is_wav && cc != 1 && cc != 2 )
				return;
		}
		if( i == 34 )
		{
			if( is_bmp )
				period = 3;
			if( is_voc )
				period = 1;
			if( is_wav )
			{
				if( cc == 16 )
					period *= 2;
				else if( cc != 8 )
					return;
			}
			shift = (8 * period - 8) & 31;
		}
	}
	if( period == 0 )
		return;
	for( ; i < len; i++ )
	{
		unsigned s = buf[i];
		unsigned r = (unsigned)(i % period);
		unsigned f = fold[r] ? (unsigned)((0u - s) & 0xFFu) : s;
		unsigned o;
		if( s >= 0x80u )
			fold[r] = (unsigned char)(fold[r] ^ 1);
		o = (unsigned)(f + ((v4 >> shift) & 0xFFUL)) & 0xFFu;
		buf[i] = (unsigned char)o;
		v4 = ((v4 << 8) | (unsigned long)o) & 0xFFFFFFFFUL;
	}
}

/* ---- fax transpose inverse (1728x2376 bit-plane rebuild) ---- */

void UhaFaxFilter(unsigned char* buf, unsigned long len)
{
	unsigned long r, base, o;
	unsigned char* tmp;
	unsigned x, i;
	if( !buf || len != 513216UL )
		return;
	tmp = (unsigned char*)malloc(513216UL);
	if( !tmp )
		return;
	o = 0;
	for( r = 0; r < 2376UL; r++ )
	{
		unsigned mask = 1UL << (7UL - (r & 7UL));
		base = (r >> 3) * 1728UL;
		for( x = 0; x < 1728U; x += 8U )
		{
			unsigned b = 0;
			for( i = 0; i < 8U; i++ )
			{
				if( buf[base + x + i] & mask )
					b |= 0x80U >> i;
			}
			tmp[o++] = (unsigned char)b;
		}
	}
	memcpy(buf, tmp, 513216UL);
	free(tmp);
}

/* ---- full per-file post-filter chain: fax -> EXE -> mm ---- */

void UhaApplyFilters(UhaArchive* arc, unsigned char* buf,
	unsigned long len)
{
	if( !arc || !buf )
		return;
	if( arc->fax_filter && len == 513216UL )
	{
		UhaFaxFilter(buf, len);
		return;
	}
	if( arc->exe_filter )
		UhaExeFilter(arc, buf, len);
	if( arc->mm_filter )
		UhaMmFilter(buf, len);
}

unsigned long UhaFileCrc(const unsigned char* d, unsigned long n)
{
	return uha_crc32(d, n);
}

/* ------------------------------------------------------------------ */
/* 0.3/0.4/0.6 EXE post-filter (FORMAT 6.4).
 * Inverse of the compressor's EXE filter: rewrites E8/E9 operands
 * through a 128-entry move-to-front address table AND rebuilds the
 * MZ relocation table (de-transpose, inverse delta). Gate: the
 * caller applies the header flag + size gate; the routine itself
 * accepts MZ/ZM/MQ, ELF (with the e_type/entry byte check) and
 * "FBOV". The f6 pre-transform is armed per-archive for 0.6 methods
 * 24..39 (core 4); it is off for every other method/version.
 */

static unsigned uha_rd16(const unsigned char* p)
{
	return (unsigned)(p[0] | (p[1] << 8));
}

static unsigned long uha_rd32(const unsigned char* p)
{
	return (unsigned long)(p[0] | (p[1] << 8) | (p[2] << 16) |
		((unsigned long)p[3] << 24));
}

static long uha_rd32s(const unsigned char* p)
{
	unsigned long u = uha_rd32(p);
	if( u & 0x80000000UL )
		return -(long)(0xFFFFFFFFUL - u) - 1;
	return (long)u;
}

static void uha_wr16(unsigned char* p, unsigned v)
{
	p[0] = (unsigned char)(v & 0xFFU);
	p[1] = (unsigned char)((v >> 8) & 0xFFU);
}

static void uha_wr32(unsigned char* p, unsigned long v)
{
	p[0] = (unsigned char)(v & 0xFFUL);
	p[1] = (unsigned char)((v >> 8) & 0xFFUL);
	p[2] = (unsigned char)((v >> 16) & 0xFFUL);
	p[3] = (unsigned char)((v >> 24) & 0xFFUL);
}

static void UhaExeFilter345(UhaArchive* arc, unsigned char* buf, unsigned long len)
{
	unsigned long end, esi, d7c, d90;
	unsigned long i;
	unsigned char* tmp;
	int exe, elf, fbov;
	int f6;
	unsigned tab[128];
	unsigned j;

	f6 = (arc && arc->ver == 6 && arc->method >= 24 && arc->method <= 39) ? 1 : 0;
	if( !buf || len < 0x400UL || len <= 15UL )
		return;
	end = len - 15UL;

	/* gate: MZ/ZM/MQ word, ELF dword, or "FBOV" */
	{
		unsigned w0 = uha_rd16(buf);
		unsigned long d0 = uha_rd32(buf);
		exe = (w0 == 0x5A4DU || w0 == 0x4D5AU || w0 == 0x514DU);
		elf = (d0 == 0x464C457FUL);
		fbov = (d0 == 0x564F4246UL);
	}
	if( !exe && !elf && !fbov )
		return;

	esi = 0;

	/* ELF: refuse e_type/entry bytes 18/19 == 3/0 mismatch */
	if( elf )
	{
		while( esi < 20UL && esi < end )
		{
			if( (esi == 18UL && buf[esi] != 3) ||
				(esi == 19UL && buf[esi] != 0) )
				return;
			esi++;
		}
	}

	d7c = 0;
	d90 = 0;

	/* MZ header scan: e_crlc (+6) and e_lfarlc (+0x18) */
	if( exe )
	{
		while( exe && esi < 26UL && esi < end )
		{
			if( esi == 6UL )
			{
				d90 = buf[esi];
			}
			else if( esi == 7UL )
			{
				d90 += (unsigned long)buf[esi] << 8;
				if( d90 < 0x10UL || d90 > 0x2000UL )
					exe = 0;
			}
			else if( esi == 24UL )
			{
				d7c = buf[esi];
			}
			else if( esi == 25UL )
			{
				d7c += (unsigned long)buf[esi] << 8;
				d90 = d7c + 4UL * d90;
				if( d7c < 0x1CUL || d90 >= len )
					exe = 0;
			}
			esi++;
		}
	}

	/* relocation table: de-transpose, inverse delta */
	if( exe && esi >= 26UL )
	{
		while( esi < d7c && esi < end )
			esi++;
		if( esi == d7c )
		{
			unsigned long rlen;
			unsigned long p = 0;
			unsigned long k;
			int r;

			/* advance over the stored (filtered) relocation bytes */
			while( esi < d90 && esi < end )
				esi++;
			if( esi == d90 )
			{
				rlen = d90 - d7c;
				tmp = (unsigned char*)malloc((size_t)(rlen ? rlen : 1));
				if( !tmp )
					return;
				for( r = 0; r < 4; r++ )
				{
					for( k = (unsigned long)r; k < rlen; k += 4UL )
						tmp[k] = buf[d7c + p++];
				}
				/* inverse delta: word[k] += word[k-2] */
				for( k = 2; k < (rlen >> 1); k++ )
				{
					unsigned w = uha_rd16(tmp + 2 * k) + uha_rd16(tmp + 2 * k - 4);
					uha_wr16(tmp + 2 * k, w & 0xFFFFU);
				}
				memcpy(buf + d7c, tmp, (size_t)rlen);
				free(tmp);
				exe = 0;
				/* scanning resumes at d90 (esi already there) */
			}
		}
	}

	/* E8/E9 rewrite with the 128-entry address table */
	for( j = 0; j < 128; j++ )
		tab[j] = j;

	i = esi;
	while( i < end )
	{
		unsigned char c = buf[i];
		if( c != 0xE8 && c != 0xE9 )
		{
			i++;
			continue;
		}

		{
			long v = uha_rd32s(buf + i + 1);
			unsigned long adv = 4;
			/* the table path reads back what was stored */
			unsigned long cur = (unsigned long)v;
			long w = v;

			if( v != 0 )
			{
				if( f6 )
				{
					/* 0.6 core-4 pre-transform (byte_433E36): shuffle
					 * t = (B0<<16) + ((v & 0xFF00FF00) + B2) mod 2^32, where
					 * v = B3 B2 B1 B0. If v or t lies in [-i, len), store t and
					 * continue the normal path on t. */
					unsigned long t = ((((unsigned long)v << 16) &
						0x00FF0000UL) +
						((((unsigned long)v & 0xFF00FF00UL) +
							(((unsigned long)v >> 16) & 0xFFUL)) &
						0xFFFFFFFFUL)) & 0xFFFFFFFFUL;
					if( (v >= -(long)i && v < (long)len) ||
						((long)t >= -(long)i && (long)t < (long)len) )
					{
						uha_wr32(buf + i + 1, t);
						cur = t;
						w = (long)t;
						adv = 5;
					}
				}
				if( w > 0 && (unsigned long)w <= len - 1UL )
				{
					if( w == (long)i )
						w = 0;
					uha_wr32(buf + i + 1, (unsigned long)w - i);
					adv = 5;
				}
				else if( w < 0 && w >= -(long)i )
				{
					uha_wr32(buf + i + 1, len + (unsigned long)w);
					adv = 5;
				}
				else
				{
					/* hi/lo read from the current bytes */
					unsigned hi = (unsigned)((cur >> 16) & 0xFFFFUL);
					unsigned hitot = (unsigned)((len - 1UL) >> 16) & 0xFFFFU;
					unsigned hineg = (unsigned)(((0UL - i) >> 16) & 0xFFFFUL);
					if( hi != 0 && hi != hitot && hi != hineg )
					{
						unsigned low = (unsigned)cur & 0xFFFFU;
						unsigned idx = (unsigned)((low - 0x8000U) & 0xFFFFU);
						if( idx < 0x80U )
						{
							unsigned val = tab[idx];
							uha_wr16(buf + i + 1,
								(unsigned)((val - (unsigned)i) & 0xFFFFU));
							if( idx != 0 )
							{
								unsigned q = idx;
								while( q > 0 )
								{
									tab[q] = tab[q - 1];
									q--;
								}
								tab[0] = val;
							}
							adv = 4;
						}
						else
						{
							unsigned guard = 0;
							for( ;; )
							{
								unsigned target =
									(unsigned)((low + (unsigned)i) & 0xFFFFU);
								int found = -1;
								unsigned a;
								for( a = 0; a < 128U; a++ )
								{
									if( tab[a] == target )
									{
										found = (int)a;
										break;
									}
								}
								if( found < 0 )
								{
									/* insert unless the low word would
									 * look like a table reference */
									if( !(target >= 0x8000U && target < 0x8080U) )
									{
										unsigned q = 127U;
										while( q > 0 )
										{
											tab[q] = tab[q - 1];
											q--;
										}
										tab[0] = target;
									}
									break;
								}
								low = (unsigned)(0x8000U | (unsigned)found);
								uha_wr16(buf + i + 1, low);
								if( ++guard > 0x10000UL )
									break;
							}
							adv = 4;
						}
					}
				}
			}
			i += adv;
		}
	}
}
