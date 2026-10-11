#ifndef LHASA_VC6_INTTYPES_H
#define LHASA_VC6_INTTYPES_H

/* Visual C++ 6.0 ships neither <inttypes.h> nor <stdint.h>. The vendored
   lhasa decoders only need the fixed-width spellings below, mapped here
   onto VC6's native types. This header is picked up through /I"lhasa"
   for the lhasa translation units only, so no other code is affected. */

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned __int64   uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef __int64            int64_t;

/* C99 SIZE_MAX, needed by lha_decoder.c's stream-length clamp. The
   expansion only requires size_t, which stdlib.h provides before any
   use site. */
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)~(size_t)0)
#endif

#endif /* LHASA_VC6_INTTYPES_H */
