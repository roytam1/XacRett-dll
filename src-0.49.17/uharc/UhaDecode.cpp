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
	else if( method == 17 && ver == 6 )
	{
		void* ctx;
		int irc;
		irc = UhaDec3_06Init(&ctx, 17);
		if( irc != 0 )
		{
			free(bw.b);
			return irc == 2 ? 2 : 7;
		}
		rc = UhaDec3_06(ctx, NULL, 0, uha_mem_read, &mr,
			uha_out_write, &bw, dlen);
		UhaDec3_06Free(ctx);
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
	arc->exe_filter = 0;	/* 0.2: always gated on MZ + len */
	arc->mm_filter = 0;
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
	if( ver == 2 )
		rc = uha02_open(img, flen, pwd, arc);
	else if( ver == 6 )
		rc = uha06_open(img, flen, pwd, arc);
	else if( ver == 3 || ver == 4 )
		rc = 7;	/* Phase 1b: container next */
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

/* EXE post-filter placeholder (Phase 2 with core 1 RLE). */
void UhaExeFilter(UhaArchive* arc, unsigned char* buf, unsigned long len)
{
	(void)arc; (void)buf; (void)len;
}

unsigned long UhaFileCrc(const unsigned char* d, unsigned long n)
{
	return uha_crc32(d, n);
}
