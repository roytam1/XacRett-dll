#ifndef VC4COMPAT_H
#define VC4COMPAT_H
/* vc4compat.h - Microsoft Visual C++ 4.x (CL 10.10) compatibility.
 *
 * OPTIONAL: nothing in the normal build includes this.  The VC4 build
 * force-includes it with /FI, and every part of it is inside
 * `#if defined(_MSC_VER) && (_MSC_VER < 1200)`, so VC6 (1200) and
 * anything newer is completely unaffected.
 *
 * Usage (VC4 only):
 *   cl /FI"<tree>\vc4compat\vc4compat.h" /I"<tree>\..\..\STL"
 *      /I"<tree>\vc4compat" /I"<tree>" ...
 *
 * Why each piece is here, measured not guessed:
 *
 *  - `bool` / `true` / `false` are "reserved for future use" in VC4 and
 *    are hard errors ("error C2501: 'bool' : missing decl-specifiers").
 *    332 of our own sources use `bool`.
 *
 *  - ORDER MATTERS.  windows.h / ole2.h / objidl.h must be seen with the
 *    REAL bool first, because VARIANT_BOOL uses it ("error C2628:
 *    'VARIANT_BOOL' followed by 'int' is illegal").  But STLport's own
 *    headers use `bool` too, so they cannot be parsed before it exists.
 *    Hence: Win32 headers, then the macro, then STL.
 *
 *  - `#define bool int` is WRONG.  It lets <vector> parse but then
 *    collides with STLport's operator[] overloads ("error C2666: '[]' :
 *    3 overloads have similar conversions").  `unsigned char` matches
 *    VC6's 1-byte bool and keeps every overload distinct.
 *
 *  - ULONG_PTR / PULONG_PTR / LONG_PTR / PLONG_PTR postdate VC4's SDK
 *    (used by IsTool.cpp).
 */
#include <windows.h>
#include <ole2.h>
#include <objidl.h>

#if defined(_MSC_VER) && (_MSC_VER < 1200)

#ifndef bool
#define bool  unsigned char
#endif
#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif

#ifndef ULONG_PTR
typedef unsigned long ULONG_PTR;
#endif
#ifndef PULONG_PTR
typedef ULONG_PTR* PULONG_PTR;
#endif
#ifndef LONG_PTR
typedef long LONG_PTR;
#endif
#ifndef PLONG_PTR
typedef LONG_PTR* PLONG_PTR;
#endif

/* This STLport + VC4 combination meets `__forceinline` in the
 * vendored zstd/sevenz C sources, which VC4 does not know as a
 * keyword (C2054).  Define it away: linkage and calls are
 * unaffected, only the inlining hint is lost. */
#ifndef __forceinline
#define __forceinline
#endif

/* VC4 has no <shlwapi.h>; we deliberately do not use SHLWAPI (the
 * official binary does import it, but dropping it is what keeps this
 * DLL loadable on stock Windows 95 / NT4). */
#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600)
#else
/* no shlwapi here by design - see the note above */
#endif

/* STLport v2 needs bool to exist before it will parse at all.  These are
 * the headers the tree actually pulls in.  C++ only: the .c files
 * are force-included with this header too, and C has no namespaces
 * (every .c file failed with C2282 before this guard). */
#ifdef __cplusplus
#include <vector>
#include <list>
#include <map>
#include <set>
#include <utility>
#include <algorithm>
#include <string>
#endif /* __cplusplus */

#endif /* _MSC_VER < 1200 */
#endif /* VC4COMPAT_H */