#ifndef AFX_BROTLIDECODE_H__BROTLI_STREAM_ENGINE_INCLUDED__
#define AFX_BROTLIDECODE_H__BROTLI_STREAM_ENGINE_INCLUDED__

// Shared Brotli stream engine, used by the standalone .br provider and
// by the .tar.br / .cpio.br temp-delegation providers.  A .br file has no
// magic and no size field, so the decoder is also the format check.
//
// Concatenated streams are accepted, like .zst.  The RFC 7932 static
// dictionary is held once in a compressed blob and installed lazily
// through BrotliSetDictionaryData (see brotli/common/KI.README).  This
// function must be called before any decode; it is safe to call repeatedly.

#include "Tool.h"

typedef int (*BrFillFn)( void *ctx, BYTE *buf, int want );
typedef void (*BrWriteFn)( void *ctx, const BYTE *buf, int len );

void BrotliEnsureDictionary( void );

// Decode to the write callback.  maxOut>0 means "valid prefix": return
// true as soon as that many output bytes have been produced (used by the
// routing Check, which must not decode a large member just to identify
// it).  maxOut==0 decodes every stream and requires a clean tail.
bool BrotliDecodeStream( BrFillFn fill, void *ictx,
                         BrWriteFn write, void *octx, DWORD maxOut );

bool BrotliDecodeFile( const char *src, const char *dst );
bool BrotliCheckFile( const char *fname, unsigned long fsize );

#endif
