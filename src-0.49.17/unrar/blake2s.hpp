// Based on public domain code written in 2012 by Samuel Neves
#ifndef _RAR_BLAKE2_
#define _RAR_BLAKE2_

#define BLAKE2_DIGEST_SIZE 32
#define BLAKE2_THREADS_NUMBER 8

// Use constexpr instead of enums for -std=c++20 compatibility.
#if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
const size_t BLAKE2S_BLOCKBYTES = 64; /* VC6: no constexpr (VC6_CONSTEXPR) */
const size_t BLAKE2S_OUTBYTES = 32;
#else
constexpr size_t BLAKE2S_BLOCKBYTES = 64;
constexpr size_t BLAKE2S_OUTBYTES = 32;
#endif /* VC6 */


// Alignment to 64 improves performance of both SSE and non-SSE versions.
// Alignment to n*16 is required for SSE version, so we selected 64.
// We use the custom alignment scheme instead of __declspec(align(x)),
// because it is less compiler dependent. Also the compiler directive
// does not help if structure is a member of class allocated through
// 'new' operator.
struct blake2s_state
{
  // Use constexpr instead of enums, because otherwise clang -std=c++20
  // issues a warning about "arithmetic between different enumeration types"
  // in ubuf[BLAKE_DATA_SIZE + BLAKE_ALIGNMENT] declaration.
  #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
  enum { BLAKE_ALIGNMENT = 64 };
  #else
  static constexpr size_t BLAKE_ALIGNMENT = 64;
  #endif /* VC6 */


  // buffer and uint32 h[8], t[2], f[2];
  // 2 * BLAKE2S_BLOCKBYTES is the buf size in blake2_code_20140114.zip.
  // It might differ in later versions.
  #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
  enum { BLAKE_DATA_SIZE = 48 + 2 * BLAKE2S_BLOCKBYTES };
  #else
  static constexpr size_t BLAKE_DATA_SIZE = 48 + 2 * BLAKE2S_BLOCKBYTES;
  #endif /* VC6 */


  byte ubuf[BLAKE_DATA_SIZE + BLAKE_ALIGNMENT];

  byte   *buf;       // byte   buf[2 * BLAKE2S_BLOCKBYTES].
  uint32 *h, *t, *f; // uint32 h[8], t[2], f[2].

  size_t   buflen;
  byte  last_node;

  blake2s_state()
  {
    set_pointers();
  }

  // Required when we declare and assign in the same command.
  blake2s_state(blake2s_state &st)
  {
    set_pointers();
    *this=st;
  }

  void set_pointers()
  {
    // Set aligned pointers. Must be done in constructor, not in Init(),
    // so assignments like 'blake2sp_state res=blake2ctx' work correctly
    // even if blake2sp_init is not called for 'res'.
    buf = (byte *) ALIGN_VALUE(ubuf, BLAKE_ALIGNMENT);
    h   = (uint32 *) (buf + 2 * BLAKE2S_BLOCKBYTES);
    t   = h + 8;
    f   = t + 2;
  }

  void init()
  {
    memset( ubuf, 0, sizeof( ubuf ) );
    buflen = 0;
    last_node = 0;
  }

  // Since we use pointers, the default = would work incorrectly.
  blake2s_state& operator = (blake2s_state &st)
  {
    if (this != &st)
    {
      memcpy(buf, st.buf, BLAKE_DATA_SIZE);
      buflen = st.buflen;
      last_node = st.last_node;
    }
    return *this;
  }
};


#ifdef RAR_SMP
class ThreadPool;
#endif

struct blake2sp_state
{
  blake2s_state S[8];
  blake2s_state R;
  byte buf[8 * BLAKE2S_BLOCKBYTES];
  size_t buflen;

#ifdef RAR_SMP
  ThreadPool *ThPool;
  uint MaxThreads;
#endif
};

void blake2sp_init( blake2sp_state *S );
void blake2sp_update( blake2sp_state *S, const byte *in, size_t inlen );
void blake2sp_final( blake2sp_state *S, byte *digest );

#endif

