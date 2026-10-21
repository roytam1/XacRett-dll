///////////////////////////////////////////////////////////////////
// gca_sha1.h -- SHA-1 (FIPS 180-1) used by the GCA "key file" mode.
//
// gcac.exe's -k<keyfile> switch hashes the key file with SHA-1 and
// uses the lower-case hex digest as the archive password.
//
//   gcac.exe.c:17433  main(): -k branch -> sub_412AF0 (init)
//   gcac.exe.c:17454  sub_412B70 (update)
//   gcac.exe.c:17456  sub_412C00 (final)
//   gcac.exe.c:17457  sub_412CB0 (digest -> 40 char lower case hex)
//
// The digest text is what ends up in the archive's key stream, so the
// hex encoding must be lower case and zero padded ("%08x" per word).
///////////////////////////////////////////////////////////////////
#ifndef GCA_SHA1_H
#define GCA_SHA1_H

/* Compute the SHA-1 digest of `len` bytes at `data`.
 * `digest` receives the 20 raw bytes. */
void gcaSha1(const unsigned char* data, unsigned long len,
             unsigned char digest[20]);

/* Same, but writes 40 lower-case hex characters + NUL into `out`
 * (out must hold at least 41 bytes). */
void gcaSha1Hex(const unsigned char* data, unsigned long len, char* out);

#endif
