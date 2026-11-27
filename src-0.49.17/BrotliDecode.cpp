// Shared Brotli stream engine.  See BrotliDecode.h.
//
// The decoder state is created afresh for each stream in the file; the
// static dictionary (inflated once from a compressed blob) is global and
// shared.  Raw deflate is used for the blob because this tree patches
// zlib MAX_WBITS to 16 for Deflate64, so uncompress() cannot read a zlib
// wrapper here.

#include "stdafx.h"
#include "BrotliDecode.h"

#include <brotli/decode.h>
#include "brotli/common/dictionary.h"
#include "brotli/common/dictionary_blob.h"
#include "zlib/zlib.h"

#define BR_IN_BLOCK		(1<<16)
#define BR_OUT_BLOCK	(1<<16)

static BYTE *xacBrotliDict = NULL;

void BrotliEnsureDictionary( void )
{
	if( xacBrotliDict ) return;
	BYTE *p = new BYTE[XAC_BROTLI_DICT_SIZE];
	z_stream s;
	memset( &s, 0, sizeof(s) );
	s.next_in = (Bytef*)kXacBrotliDictionaryZ;
	s.avail_in = (uInt)sizeof(kXacBrotliDictionaryZ);
	s.next_out = p;
	s.avail_out = XAC_BROTLI_DICT_SIZE;
	if( Z_OK!=inflateInit2( &s, -15 ) ||
		Z_STREAM_END!=inflate( &s, Z_FINISH ) ||
		s.total_out!=XAC_BROTLI_DICT_SIZE )
		{ inflateEnd( &s ); delete [] p; return; }
	inflateEnd( &s );
	BrotliSetDictionaryData( (const uint8_t*)p );
	xacBrotliDict = p;
}

static int brFileFill( void *ctx, BYTE *buf, int want )
{
	int n = (int)fread( buf, 1, want, (FILE*)ctx );
	return n > 0 ? n : 0;
}

static void brFileWrite( void *ctx, const BYTE *buf, int len )
{
	fwrite( buf, 1, len, (FILE*)ctx );
}

struct BrStreamIO
{
	BrFillFn	fill;
	void*		ictx;
	BrWriteFn	write;
	void*		octx;
	BYTE*		in;
	DWORD		maxOut;
	DWORD		total;
};

bool BrotliDecodeStream( BrFillFn fill, void *ictx,
                         BrWriteFn write, void *octx, DWORD maxOut )
{
	BrStreamIO io;
	BYTE inbuf[BR_IN_BLOCK], outbuf[BR_OUT_BLOCK];
	DWORD inpos = 0, inlen = 0;
	bool any = false;

	BrotliEnsureDictionary();

	memset( &io, 0, sizeof(io) );
	io.fill = fill; io.ictx = ictx;
	io.write = write; io.octx = octx;
	io.in = inbuf; io.maxOut = maxOut;

	for(;;)
	{
		BrotliDecoderState *st;
		size_t avail_in, avail_out;
		const uint8_t *next_in;
		uint8_t *next_out;
		BrotliDecoderResult r;
		bool streamOk = false;

		// start of a stream: one byte is needed for the stream header
		if( inpos>=inlen )
		{
			inlen = (DWORD)io.fill( io.ictx, inbuf, BR_IN_BLOCK );
			inpos = 0;
		}
		if( inlen==0 ) break;
		next_in = (const uint8_t*)(inbuf + inpos);
		avail_in = inlen - inpos;
		st = BrotliDecoderCreateInstance( NULL, NULL, NULL );
		if( !st ) return false;

		for(;;)
		{
			avail_out = sizeof(outbuf);
			next_out = (uint8_t*)outbuf;
			r = BrotliDecoderDecompressStream( st, &avail_in, &next_in,
													   &avail_out, &next_out, NULL );
			inpos = inlen - (DWORD)avail_in;
			if( next_out!=(uint8_t*)outbuf )
			{
				DWORD n = (DWORD)(next_out - (uint8_t*)outbuf);
				if( io.write ) io.write( io.octx, outbuf, (int)n );
				io.total += n;
				any = true;
				if( io.maxOut && io.total>=io.maxOut )
					{ BrotliDecoderDestroyInstance( st ); return true; }
			}
			if( r==BROTLI_DECODER_RESULT_ERROR )
				{ BrotliDecoderDestroyInstance( st ); return false; }
			if( r==BROTLI_DECODER_RESULT_SUCCESS )
				{ streamOk = true; break; }
			if( r==BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT )
			{
				DWORD got;
				if( avail_in!=0 )
					{ BrotliDecoderDestroyInstance( st ); return false; }
				got = (DWORD)io.fill( io.ictx, inbuf, BR_IN_BLOCK );
				if( got==0 )
					{ BrotliDecoderDestroyInstance( st ); return false; }
				inpos = 0; inlen = got;
				next_in = (const uint8_t*)inbuf; avail_in = got;
			}
			// NEEDS_MORE_OUTPUT: loop with a fresh output block
		}
		BrotliDecoderDestroyInstance( st );
		if( !streamOk ) return false;
		// inpos points just past this stream; the outer loop continues with
		// any concatenated stream bytes still in the buffer.
	}
	return any;
}

bool BrotliDecodeFile( const char *src, const char *dst )
{
	FILE *fin, *fout;
	bool ok;

	fin = fopen( src, "rb" );
	if( !fin ) return false;
	fout = fopen( dst, "wb" );
	if( !fout ) { fclose( fin ); return false; }
	ok = BrotliDecodeStream( brFileFill, fin, brFileWrite, fout, 0 );
	fclose( fin );
	fclose( fout );
	return ok;
}

static void brCountWrite( void *ctx, const BYTE *buf, int len )
{
	(void)buf; (void)len;
	(void)ctx;
}

bool BrotliCheckFile( const char *fname, unsigned long fsize )
{
	FILE *fp;
	bool ok;
	(void)fsize;

	fp = fopen( fname, "rb" );
	if( !fp ) return false;
	ok = BrotliDecodeStream( brFileFill, fp, brCountWrite, NULL, 4096 );
	fclose( fp );
	return ok;
}
