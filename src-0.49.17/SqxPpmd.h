/* SqxPpmd.h -- SQX method 6 (PPMd text) decode glue.
 *
 * Method 6 is PPMd variant I (Dmitry Shkarin) over the SQX carryless range
 * coder. The model core is the tree's vendored ppmd8/ decoder (7-Zip
 * Ppmd8 lineage, public domain); only the SQX front end (word-swapped
 * bit reader, range coder, cont/order/mem header) and the symbol loop
 * live in SqxPpmd.cpp, following the ZipPpmd.cpp precedent of keeping
 * codec glue in its own TU.
 *
 * Reference (TEMP only, never vendored): seb3773/Squeez-c (MIT), whose
 * oracle output plus encoder-generated fixtures prove this bit-exact.
 */
#ifndef SQXPPMD_H_INCLUDED
#define SQXPPMD_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/* Reset the solid-model session. Called once per archive. */
void SqxPpmd_BeginArchive(void);

/* Decode the current member (method 6) through the unsqx output path.
 * Reads IoStruct.l64CompSize bytes, writes IoStruct.l64OrigSize bytes.
 * Returns nonzero on success; on failure ArchiveStatus is already loud
 * (BAD_FILE_FORMAT) and whatever was written stays partial. */
int SqxUnPpmd(void);

#ifdef __cplusplus
}
#endif

#endif
