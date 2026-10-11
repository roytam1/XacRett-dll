///////////////////////////////////////////////////////////////////
// gca_bwt.h -- BWT inverse (BlockSortDecode transliteration).
///////////////////////////////////////////////////////////////////
#ifndef GCA_BWT_H
#define GCA_BWT_H

// Inverse Burrows-Wheeler: out[i] from in[] via LF-mapping.
// tmp: scratch DWORD[n]. Returns 1.
int gcaBlockSortDecode(unsigned char* out, const unsigned char* in,
                        int n, int primary, unsigned long* tmp);

#endif
