/* LhasaBridge.cpp: glue between XacRett's FILE-based LzhTool and the
   vendored lhasa decoder framework (lhasa/public/lha_decoder.h).
   ASCII-only, C89-style declarations, VC6-clean. */

#include <stdio.h>
#include <string.h>

#include "lhasa/public/lha_decoder.h"
#include "lhasa/inttypes.h"
#include "LhasaBridge.h"

typedef struct {
	FILE *fp;
	unsigned long left;
} FileFeed;

static size_t FileFeedRead(void *buf, size_t len, void *data)
{
	FileFeed *feed = (FileFeed *)data;
	size_t want = len;
	if (want > feed->left)
		want = feed->left;
	if (want == 0)
		return 0;
	want = fread(buf, 1, want, feed->fp);
	feed->left -= (unsigned long)want;
	return want;
}

int LhasaDecode(const char *method, FILE *infile, unsigned long insize,
                FILE *outfile, unsigned long outsize)
{
	const LHADecoderType *dtype;
	LHADecoder *decoder;
	FileFeed feed;
	unsigned char outbuf[8192];
	size_t got;
	uint64_t total;
	int ok;

	dtype = lha_decoder_for_name(method);
	if (dtype == NULL)
		return 0;
	feed.fp = infile;
	feed.left = insize;
	decoder = lha_decoder_new(dtype, FileFeedRead, &feed, outsize);
	if (decoder == NULL)
		return 0;
	for (;;) {
		got = lha_decoder_read(decoder, outbuf, sizeof(outbuf));
		if (got == 0)
			break;
		if (fwrite(outbuf, 1, got, outfile) != got) {
			lha_decoder_free(decoder);
			return 0;
		}
	}
	total = lha_decoder_get_length(decoder);
	ok = (total == (uint64_t)outsize);
	lha_decoder_free(decoder);
	return ok;
}
