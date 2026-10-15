#ifndef VC4_BASETSD_H
#define VC4_BASETSD_H
/* basetsd.h - minimal stand-in for MSVC 4.x, which ships none.
 *
 * Reached ONLY on the VC4 path (via /I vc4compat); VC6 and later
 * use the real SDK header and never see this file.  Needed because
 * fdi/Fdi.h does `#include <basetsd.h>` unconditionally (fatal
 * C1083 on VC4 otherwise). */
#if defined(_MSC_VER) && (_MSC_VER < 1200)

#ifndef INT8
typedef signed char INT8, *PINT8;
#endif
#ifndef INT16
typedef signed short INT16, *PINT16;
#endif
#ifndef INT32
typedef signed int INT32, *PINT32;
#endif
#ifndef INT64
typedef signed __int64 INT64, *PINT64;
#endif
#ifndef UINT8
typedef unsigned char UINT8, *PUINT8;
#endif
#ifndef UINT16
typedef unsigned short UINT16, *PUINT16;
#endif
#ifndef UINT32
typedef unsigned int UINT32, *PUINT32;
#endif
#ifndef UINT64
typedef unsigned __int64 UINT64, *PUINT64;
#endif
#ifndef LONG32
typedef long LONG32, *PLONG32;
#endif
#ifndef ULONG32
typedef unsigned long ULONG32, *PULONG32;
#endif

#ifndef INT_PTR
typedef int INT_PTR, *PINT_PTR;
#endif
#ifndef UINT_PTR
typedef unsigned int UINT_PTR, *PUINT_PTR;
#endif

#endif /* _MSC_VER < 1200 */
#endif /* VC4_BASETSD_H */
