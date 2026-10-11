///////////////////////////////////////////////////////////////////
// zdata.c
//
// Portable reimplementation of InstallShield's ZData codec.
//
// Original: 32-bit COFF object by fOSSiL - 2000 (zdata.lib).
// This file: clean-room implementation over stock zlib, verified
// behavior-compatible with the original via black-box probing
// (return codes, callback protocol, chunk framing, error paths).
//
// Wire format ("new" style): [u16LE len][raw deflate]... A zero
// length terminates the stream. Each chunk is an independent raw
// deflate stream (inflateInit2(-MAX_WBITS)). Output decoded before a
// chunk-decode failure is retained; Decompress consumes all input.
//
// Return codes (match original): 0 ok, -1 compress-stub,
// -2 setup magic, -4 not configured/started, -5 bad SetInfo id,
// -6 I/O (read-callback or short-write) / resource failure.
//
// Notes:
//  - ZDataCompress is intentionally a stub: the original returns
//    -1 once configured (-4 otherwise) without touching the buffer
//    in every probed configuration, and XacRett never calls it.
//    The stub preserves exactly that observable behavior.
//  - ULONG_PTR keeps SetInfo pointer params 64-bit clean. On x86
//    the __stdcall export names are unchanged (_ZDataSetInfo@8...).
//  - C89 + Win32 only; links against the in-tree zlib (1.2.2).
//
#include <windows.h>
#include "zdatas.h"
#include "../zlib/zlib.h"
#include <stdlib.h>
#include <string.h>

#define ZD_OK		0
#define ZD_E_COMPRESS	-1
#define ZD_E_SETUP	-2
#define ZD_E_NOCFG	-4
#define ZD_E_PARAM	-5
#define ZD_E_IO		-6

#define ZD_MAGIC	0xfa372dea

static int g_lastErr = 0;
static int g_setup = 0;
static unsigned char* g_buf = NULL;
static unsigned long g_bufSize = 0;
static void* g_param = NULL;
static ZD_READ_CB g_rcb = NULL;
static ZD_WRITE_CB g_wcb = NULL;
static int g_started = 0;

static int zdSetErr(int e)
{
	g_lastErr = e;
	return e;
}

/* Pull exactly 'want' bytes via the read callback unless EOF/error.
 * Returns 0 ok (even short at EOF), -1 on callback error. */
static int zdPull(unsigned char* dst, unsigned long want, unsigned long* got)
{
	unsigned long have = 0;
	*got = 0;
	while (have < want) {
		DWORD n;
		unsigned char tmp[4096];
		unsigned long step;
		int rc;
		step = want - have;
		if (step > (unsigned long)sizeof(tmp))
			step = (unsigned long)sizeof(tmp);
		n = (DWORD)step;
		rc = g_rcb(tmp, &n, g_param);
		if (rc != 0)
			return -1;
		if (n == 0)
			break; /* EOF */
		memcpy(dst + have, tmp, n);
		have += n;
	}
	*got = have;
	return 0;
}

/* Push 'len' bytes via the write callback using g_buf as staging.
 * Returns 0 ok, -1 on callback error or short write. */
static int zdPush(const unsigned char* src, unsigned long len)
{
	unsigned long done = 0;
	while (done < len) {
		DWORD n;
		int rc;
		unsigned long step = len - done;
		if (step > g_bufSize)
			step = g_bufSize;
		if (step == 0)
			return -1; /* no work buffer (shouldn't happen; size checked) */
		memcpy(g_buf, src + done, step);
		n = (DWORD)step;
		rc = g_wcb(g_buf, &n, g_param);
		if (rc != 0)
			return -1;
		if ((unsigned long)n != step)
			return -1; /* short write */
		done += step;
	}
	return 0;
}

/* Decode one chunk (independent raw deflate stream). Whichever output is
 * decoded before an inflate error is pushed, so a corrupt chunk contributes
 * partial output rather than being discarded. This preserves the observed
 * behavior of the original zdata.lib. Returns 0 on success, -1 on failure. */
static int zdDecodeChunk(const unsigned char* in, unsigned long inLen)
{
	z_stream zs;
	unsigned char* out = NULL;
	unsigned long outCap = 65536;
	unsigned long outLen = 0;
	int ret;
	int failed = 0;

	memset(&zs, 0, sizeof(zs));
	if (inflateInit2_(&zs, -MAX_WBITS, ZLIB_VERSION, sizeof(zs)) != Z_OK)
		return -1;
	out = (unsigned char*)malloc(outCap ? outCap : 1);
	if (!out) {
		inflateEnd(&zs);
		return -1;
	}
	zs.next_in = (Bytef*)in;
	zs.avail_in = (uInt)inLen; /* chunks fit (u16 length) */
	for (;;) {
		if (outLen == outCap) {
			unsigned char* bigger;
			outCap *= 2;
			bigger = (unsigned char*)realloc(out, outCap);
			if (!bigger) {
				failed = 1;
				break;
			}
			out = bigger;
		}
		zs.next_out = out + outLen;
		zs.avail_out = (uInt)(outCap - outLen);
		ret = inflate(&zs, Z_NO_FLUSH);
		outLen = (unsigned long)zs.total_out;
		if (ret == Z_STREAM_END)
			break;
		if (ret != Z_OK) {
			failed = 1;
			break;
		}
		if (zs.avail_in == 0) {
			/* Needs more input than the chunk holds: truncated. */
			failed = 1;
			break;
		}
	}
	inflateEnd(&zs);
	if (outLen > 0) {
		if (zdPush(out, outLen) != 0)
			failed = 1;
	}
	free(out);
	return failed ? -1 : 0;
}

int ZDAPI ZDataSetup(DWORD a, DWORD magic, DWORD c)
{
	(void)a;
	(void)c;
	g_buf = NULL;
	g_bufSize = 0;
	g_param = NULL;
	g_rcb = NULL;
	g_wcb = NULL;
	g_started = 0;
	if (magic != ZD_MAGIC)
		return zdSetErr(ZD_E_SETUP);
	g_setup = 1;
	return zdSetErr(ZD_OK);
}

int ZDAPI ZDataUnSetup()
{
	g_setup = 0;
	g_buf = NULL;
	g_bufSize = 0;
	g_param = NULL;
	g_rcb = NULL;
	g_wcb = NULL;
	g_started = 0;
	return zdSetErr(ZD_OK);
}

int ZDAPI ZDataSetInfo(DWORD id, ULONG_PTR val)
{
	switch (id) {
	case INFO_BUFFER_SIZE:
		g_bufSize = (unsigned long)val;
		break;
	case INFO_CALLBACK_PARAM:
		g_param = (void*)val;
		break;
	case INFO_READ_CALLBACK:
		g_rcb = (ZD_READ_CB)val;
		break;
	case INFO_WRITE_CALLBACK:
		g_wcb = (ZD_WRITE_CB)val;
		break;
	case INFO_BUFFER_PTR:
		g_buf = (unsigned char*)val;
		break;
	default:
		return zdSetErr(ZD_E_PARAM);
	}
	return zdSetErr(ZD_OK);
}

int ZDAPI ZDataGetLastError()
{
	return g_lastErr;
}

int ZDAPI ZDataStart(DWORD session)
{
	(void)session;
	if (!g_setup || !g_buf || !g_bufSize)
		return zdSetErr(ZD_E_NOCFG);
	g_started = 1;
	return zdSetErr(ZD_OK);
}

int ZDAPI ZDataEnd()
{
	return zdSetErr(ZD_OK);
}

int ZDAPI ZDataCompress(DWORD session, LPZCOMPRESSBUF buf)
{
	(void)session;
	(void)buf;
	if (!g_setup || !g_buf || !g_bufSize || !g_started)
		return zdSetErr(ZD_E_NOCFG);
	/* Stub: the original yields -1 in every configured state probed. */
	return zdSetErr(ZD_E_COMPRESS);
}

int ZDAPI ZDataDecompress(DWORD session)
{
	unsigned char lb[2];
	unsigned long got;
	(void)session;
	if (!g_setup || !g_buf || !g_bufSize)
		return zdSetErr(ZD_E_NOCFG);
	if (!g_rcb || !g_wcb)
		return zdSetErr(ZD_E_IO);
	for (;;) {
		unsigned n;
		unsigned char* in;
		unsigned long have;
		if (zdPull(lb, 2, &got) != 0)
			return zdSetErr(ZD_E_IO);
		if (got == 0)
			break; /* clean EOF at chunk boundary */
		if (got == 1)
			break; /* truncated length: lenient stop */
		n = (unsigned)lb[0] | ((unsigned)lb[1] << 8);
		if (n == 0)
			break; /* zero-length chunk terminates */
		in = (unsigned char*)malloc(n ? n : 1);
		if (!in)
			return zdSetErr(ZD_E_IO);
		have = 0;
		for (;;) {
			unsigned char tmp[4096];
			unsigned long step;
			DWORD m;
			int rc;
			if (have >= n)
				break;
			step = n - have;
			if (step > (unsigned long)sizeof(tmp))
				step = (unsigned long)sizeof(tmp);
			m = (DWORD)step;
			rc = g_rcb(tmp, &m, g_param);
			if (rc != 0) {
				free(in);
				return zdSetErr(ZD_E_IO);
			}
			if (m == 0)
				break; /* EOF (possibly short chunk) */
			memcpy(in + have, tmp, m);
			have += m;
		}
		/* Swallow chunk decode failures (verified original behavior). */
		zdDecodeChunk(in, have);
		free(in);
	}
	return zdSetErr(ZD_OK);
}
