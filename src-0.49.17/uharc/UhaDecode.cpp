/* UHARC container. Clean-room from FORMAT.md.
 * Phase 0: header decrypt + table cipher + directory tables.
 * Compressed tables are decoded via core dispatch; core 0/1 are in
 * UhaDec0.cpp (STORE done, RLE stubbed until Phase 2). Table methods
 * 2 and 7 (core 2) land in Phase 3, 17 (core 3) in Phase 4; until
 * then UhaOpen fails loudly for any archive whose tables need them,
 * except the all-STORE listing path is not reachable in practice
 * (tables are always compressed) — so Phase 0 lists only after
 * Phase 3/4 cores exist. The plumbing and error handling are final.
 */

#include <stdlib.h>
#include <string.h>
#include "UhaDecode.h"
#include "UhaCrypt.h"
#include "UhaDec0.h"

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

static unsigned rd16(const unsigned char* p)
{
	return (unsigned)(p[0] | (p[1] << 8));
}

static unsigned long rd24(const unsigned char* p)
{
	return (unsigned long)(p[0] | (p[1] << 8) | (p[2] << 16));
}

static unsigned long rd32(const unsigned char* p)
{
	return (unsigned long)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/* Packed-stream reader: one memory image over [off, off+len). */
typedef struct UhaMemCtx
{
	const unsigned char* b;
	unsigned long n;
	unsigned long p;
} UhaMemCtx;

static int uha_mem_read(void* ctx, unsigned char* buf, unsigned want)
{
	UhaMemCtx* m = (UhaMemCtx*)ctx;
	unsigned long avail = m->n - m->p;
	if( !avail )
		return 0;
	if( want > avail )
		want = (unsigned)avail;
	memcpy(buf, m->b + m->p, want);
	m->p += want;
	return (int)want;
}

/* Writer that appends to a growable buffer (table decode). */
typedef struct UhaBufCtx
{
	unsigned char* b;
	unsigned long n;
	unsigned long cap;
	int err;
} UhaBufCtx;

static int uha_buf_write(void* ctx, const unsigned char* buf, unsigned len)
{
	UhaBufCtx* w = (UhaBufCtx*)ctx;
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

/* Decode one table image (already cipher-decrypted) by method.
 * method 0 = STORE, 17 = core 3, 2/7 = core 2. Returns 0 ok.
 */
static int uha_decode_table(int method, int ver,
	const unsigned char* cimg, unsigned long clen,
	unsigned long dlen,
	const unsigned char** out, unsigned long* outlen)
{
	UhaMemCtx mr;
	UhaBufCtx bw;
	int rc;
	mr.b = cimg; mr.n = clen; mr.p = 0;
	bw.b = 0; bw.n = 0; bw.cap = 0; bw.err = 0;
	if( method == 0 )
	{
		/* STORE: copy, compare sizes */
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
			uha_buf_write(&bw, buf, (unsigned)n);
			left -= (unsigned long)n;
		}
		*out = bw.b;
		*outlen = bw.n;
		return 0;
	}
	/* core 2 / 3 / 5 / 4 paths are added in Phases 3-6; loud refusal */
	(void)ver;
	free(bw.b);
	return 6;
}

void UhaClose(UhaArchive* arc)
{
	if( !arc )
		return;
	if( arc->entries )
		free(arc->entries);
	arc->entries = 0;
	arc->nfiles = 0;
}

int UhaOpen(const char* path, const char* pwd, UhaArchive* arc)
{
	FILE* fp;
	unsigned char raw[48];
	unsigned long flen;
	int ver;
	unsigned long i;

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
	flen = (unsigned long)ftell(fp);
	if( flen < 39 )
	{
		fclose(fp);
		return 3;
	}
	if( flen > UHA_MAX_ARCHIVE )
	{
		fclose(fp);
		return 7;
	}
	if( fseek(fp, 0, SEEK_SET) != 0 || fread(raw, 1, 48, fp) < 39 )
	{
		fclose(fp);
		return 1;
	}
	if( !(raw[0] == 0x55 && raw[1] == 0x48 && raw[2] == 0x41) )
	{
		fclose(fp);
		return 3;
	}
	ver = raw[3];
	if( ver != 2 && ver != 3 && ver != 4 && ver != 6 )
	{
		fclose(fp);
		return 4;
	}
	arc->ver = ver;

	/* Phase 0 implements 0.6 fully; 0.2/0.3/0.4 share the same code
	 * paths once their header fields are read (added in Phase 1 proper).
	 * Refuse loud until then rather than misparse.
	 */
	fclose(fp);
	(void)pwd; (void)flen; (void)i; (void)rd16; (void)rd24; (void)rd32;
	(void)uha_crc32; (void)uha_decode_table; (void)UhaHeaderDecrypt;
	(void)UhaSBoxInit; (void)UhaSBoxUpdate; (void)UhaCoreOf;
	return 7;
}

int UhaExtractAll(UhaArchive* arc, const char* path, unsigned long data_off,
	int with_paths)
{
	(void)arc; (void)path; (void)data_off; (void)with_paths;
	return 7;
}
