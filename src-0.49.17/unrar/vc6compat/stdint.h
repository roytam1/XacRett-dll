/* stdint.h shim for MSVC 6.0 (which predates C99 stdint.h).
   Only what UnRAR 7.x rartypes.hpp needs. */
#ifndef _VC6_COMPAT_STDINT_H
#define _VC6_COMPAT_STDINT_H

typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef short              int16_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef __int64            int64_t;
typedef unsigned __int64   uint64_t;

#endif
