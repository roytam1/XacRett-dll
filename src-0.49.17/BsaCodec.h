#ifndef AFX_BSACODEC_H__BSA_CODEC_INCLUDED__
#define AFX_BSACODEC_H__BSA_CODEC_INCLUDED__

// Decompressors for the BSA/BSN archives.
//
// Three methods are decoded:
//
//   method 7   LZ77 + dynamic canonical Huffman (bsa32.exe sub_413940).
//              Used by BSN and by BSA flag 7.
//   methods 2/3/5/6
//              LZSS + dynamic Huffman, the BSA200 family (sub_208C4).
//   method 9   PPM range coder (sub_405160).  Used by BSN when the entry
//              flags carry 0x4000; the BSA writer stores 7 for every
//              compressed flavor, so a PPM stream can arrive marked as
//              method 7 and is retried as method 9 when LZ fails.
//
// Every decoder returns a freshly allocated buffer of the original size
// and the number of bytes produced in *produced, or 0 on failure.  The
// caller owns the buffer.
//
// Method 7 keeps a 128 KiB window and the method 2/3/5/6 decoder a 64 KiB
// one, and BOTH ARE PERSISTENT ACROSS THE ENTRIES OF ONE ARCHIVE (the
// original never clears them), so the archive must call BsaCodecReset
// once before its first entry.

// Zero the sliding windows.  Call once per archive, before decoding.
void BsaCodecReset( void );

// The trail bytes are the archive bytes that follow the packed stream of
// the member (up to 64 KiB); the bit readers of methods 2/3/5/6 and 9
// over-read into them exactly like the original does.  It may be 0.
unsigned char* BsaDecodeMethod7( const unsigned char* packed,
                                 unsigned long psize,
                                 unsigned long outsize,
                                 unsigned long* produced );

unsigned char* BsaDecodeMethod23( const unsigned char* packed,
                                  unsigned long psize,
                                  const unsigned char* trail,
                                  unsigned long tsize,
                                  unsigned long outsize,
                                  unsigned long* produced,
                                  int method );

unsigned char* BsaDecodeMethod9( const unsigned char* packed,
                                 unsigned long psize,
                                 const unsigned char* trail,
                                 unsigned long tsize,
                                 unsigned long outsize,
                                 unsigned long* produced,
                                 int textmode );

#endif
