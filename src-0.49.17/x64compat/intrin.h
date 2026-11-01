/* intrin.h for the 2003 Platform SDK AMD64 toolset.
 *
 * Visual C++ 14.00 for AMD64 implements every intrinsic below, but the
 * C runtime of that Platform SDK predates the header that declares them
 * (zstd's common/compiler.h includes <intrin.h> for _MSC_VER >= 1400).
 * Only what the vendored trees actually call is declared here; there is
 * no code, so nothing new is linked.
 *
 * Used by the Win64 build only (see Makefile.x64); the x86 builds never
 * put this directory on the include path.
 */
#ifndef XACRETT_X64COMPAT_INTRIN_H
#define XACRETT_X64COMPAT_INTRIN_H

#if !defined(_M_AMD64) && !defined(_M_X64)
#error "x64compat/intrin.h is for the AMD64 toolset only"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Bit scan: return non-zero and store the index when a set bit exists. */
unsigned char _BitScanForward( unsigned long* index, unsigned long mask );
unsigned char _BitScanReverse( unsigned long* index, unsigned long mask );
unsigned char _BitScanForward64( unsigned long* index, unsigned __int64 mask );
unsigned char _BitScanReverse64( unsigned long* index, unsigned __int64 mask );

/* CPU information. */
void __cpuid( int cpuInfo[4], int function_id );
void __cpuidex( int cpuInfo[4], int function_id, int subfunction_id );
unsigned __int64 __readgsqword( unsigned long offset );

#ifdef __cplusplus
}
#endif

#endif /* XACRETT_X64COMPAT_INTRIN_H */
