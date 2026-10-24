/* SqxUltra.h -- SQX method 7 (Ultra) decode glue.
 *
 * Method 7 ("Ultra", on-disk 0x07, SQX 2.0 only) is an LZ77 scheme driven
 * by a probabilistic range coder: binary is_match/is_rep models, 9-group
 * length tables, 50-symbol distance tables and nibble literal models.
 * The models persist across the solid members of one archive.
 *
 * Ported from Squeez ultra.c (MIT, (c) 2026 seb3773), reverse-engineered
 * from UnSqueez.exe. Only this TU knows the Ultra internals; sqx_dec.c
 * sees SqxUnUltra().
 *
 * Follows the SqxPpmd.cpp precedent: the session lives here, the member
 * driver streams through the unsqx output path (ReadDataBlock in,
 * WriteOutBuf out) so CRC/progress/volumes behave like every provider.
 */
#ifndef SQXULTRA_H_INCLUDED
#define SQXULTRA_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* Reset the solid-model session. Called once per archive. */
void SqxUltra_BeginArchive(void);

/* Decode the current member (method 7) through the unsqx output path.
 * dicSize is the 2.0 dictionary (8MB<<idx when the extended flag is set,
 * else 32KB<<idx); solid keeps the models from the previous member;
 * compFlags must be 0 (anything else is refused, like the reference).
 * Reads IoStruct.l64CompSize bytes, writes IoStruct.l64OrigSize bytes.
 * Returns nonzero on success; on failure ArchiveStatus is already loud
 * (BAD_FILE_FORMAT / OUT_OF_MEMORY) and the failure is a clean refusal. */
int SqxUnUltra(unsigned long dicSize, int solid, unsigned long compFlags);

#ifdef __cplusplus
}
#endif

#endif
