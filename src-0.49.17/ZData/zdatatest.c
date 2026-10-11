//////////////////////////////////////////////////////////////////////
// zdatatest.c
//
// Differential test for the ZData codec. Built twice -- once against
// the original 32-bit zdata.lib, once against the reconstructed
// zdata_recon.lib -- and the transcripts must match exactly.
//
// Test vectors are pre-generated ZData containers (see makevectors.ps1):
// [u16LE len][raw deflate] ... [u16 0]. They cannot be generated here
// because the in-tree zlib is decompression-only: zlib.h defines
// KI_GZ_NO_COMPRESSION, so deflate.c compiles to an empty object.
//////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "../ZData/zdatas.h"

#define MAXOUT (512 * 1024)

static unsigned char g_in[MAXOUT];
static unsigned long g_inLen = 0;
static unsigned long g_inPos = 0;
static unsigned char g_out[MAXOUT];
static unsigned long g_outLen = 0;
static unsigned char g_work[0x7d00];

static int __stdcall cb_read(void* pBuf, DWORD* pLen, void* pParam)
{
	unsigned long want = *pLen, avail = g_inLen - g_inPos, step;
	unsigned char* d = (unsigned char*)pBuf;
	(void)pParam;
	step = (want < avail) ? want : avail;
	if (step) memcpy(d, g_in + g_inPos, step);
	g_inPos += step;
	*pLen = (DWORD)step;
	return 0;                   /* 0 == ok; 0 bytes == EOF */
}

static int __stdcall cb_write(void* pBuf, DWORD* pLen, void* pParam)
{
	(void)pParam;
	if (g_outLen + *pLen > MAXOUT) { *pLen = 0; return -1; }
	memcpy(g_out + g_outLen, pBuf, *pLen);
	g_outLen += *pLen;
	return 0;
}

static unsigned long hash_out(void)
{
	unsigned long h = 2166136261UL, i;
	for (i = 0; i < g_outLen; i++) {
		h ^= g_out[i];
		h = (h * 16777619UL) & 0xFFFFFFFFUL;
	}
	return h;
}

static int load(const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f) return -1;
	g_inLen = (unsigned long)fread(g_in, 1, MAXOUT, f);
	fclose(f);
	g_inPos = 0;
	g_outLen = 0;
	return 0;
}

int main(int argc, char** argv)
{
	int i;
	const char* dir = (argc > 1) ? argv[1] : ".";
	char path[512];

	printf("== error paths ==\n");
	{
		int rc, err;
		rc = ZDataSetup(2, 0xDEADBEEF, 0); err = ZDataGetLastError();
		printf("setup_bad_magic      rc=%d lastErr=%d\n", rc, err);
		rc = ZDataSetup(2, 0xfa372dea, 0); err = ZDataGetLastError();
		printf("setup_ok             rc=%d lastErr=%d\n", rc, err);
		rc = ZDataSetInfo(99, 0); err = ZDataGetLastError();
		printf("setinfo_bad_id       rc=%d lastErr=%d\n", rc, err);
		rc = ZDataDecompress(0); err = ZDataGetLastError();
		printf("decompress_setup_only rc=%d lastErr=%d\n", rc, err);
		rc = ZDataStart(0); err = ZDataGetLastError();
		printf("start_nocfg          rc=%d lastErr=%d\n", rc, err);
		rc = ZDataUnSetup(); err = ZDataGetLastError();
		printf("unsetup              rc=%d lastErr=%d\n", rc, err);
		rc = ZDataDecompress(0); err = ZDataGetLastError();
		printf("decompress_unsetup   rc=%d lastErr=%d\n", rc, err);
	}

	printf("== data paths ==\n");
	for (i = 1; i <= 8; i++) {
		int rc, err;
		sprintf(path, "%s\\vec%d.bin", dir, i);
		if (load(path) != 0) {
			printf("vec%d  <missing>\n", i);
			continue;
		}
		ZDataSetup(2, 0xfa372dea, 0);
		ZDataSetInfo(INFO_BUFFER_PTR, (ULONG_PTR)g_work);
		ZDataSetInfo(INFO_BUFFER_SIZE, 0x7d00);
		ZDataSetInfo(INFO_CALLBACK_PARAM, 0);
		ZDataSetInfo(INFO_WRITE_CALLBACK, (ULONG_PTR)cb_write);
		ZDataSetInfo(INFO_READ_CALLBACK, (ULONG_PTR)cb_read);
		rc = ZDataStart(0);
		rc = ZDataDecompress(0);
		err = ZDataGetLastError();
		printf("vec%d  in=%6lu start=%d decomp=%d lastErr=%d out=%6lu hash=%08lX\n",
		       i, g_inLen, rc, rc, err, g_outLen, hash_out());
		ZDataEnd();
	}

	printf("== compress stub ==\n");
	ZDataSetup(2, 0xfa372dea, 0);
	ZDataSetInfo(INFO_BUFFER_PTR, (ULONG_PTR)g_work);
	ZDataSetInfo(INFO_BUFFER_SIZE, 0x7d00);
	ZDataSetInfo(INFO_CALLBACK_PARAM, 0);
	ZDataSetInfo(INFO_WRITE_CALLBACK, (ULONG_PTR)cb_write);
	ZDataSetInfo(INFO_READ_CALLBACK, (ULONG_PTR)cb_read);
	ZDataStart(0);
	{
		ZCOMPRESSBUF cb;
		memset(&cb, 0, sizeof(cb));
		printf("compress_configured  rc=%d lastErr=%d\n",
		       ZDataCompress(0, &cb), ZDataGetLastError());
	}
	printf("unsetup              rc=%d lastErr=%d\n",
	       ZDataUnSetup(), ZDataGetLastError());
	return 0;
}