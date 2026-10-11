///////////////////////////////////////////////////////////////////
// gca_bwt.cpp -- see gca_bwt.h.
///////////////////////////////////////////////////////////////////
#include "gca_bwt.h"
#include <string.h>

int gcaBlockSortDecode(unsigned char* out, const unsigned char* in,
                        int n, int primary, unsigned long* tmp)
{
    unsigned count[256];
    int i;
    unsigned j;
    if (n <= 0)
        return 1;
    for (i = 0; i < 256; i++)
        count[i] = 0;
    for (i = 0; i < n; i++)
        count[in[i]]++;
    { unsigned sum = 0;
      for (i = 0; i < 256; i++) {
          unsigned t = count[i];
          sum += t;
          count[i] = sum - t;
      } }
    for (i = 0; i < n; i++) {
        unsigned c = in[i];
        tmp[count[c]] = (unsigned)i;
        count[c]++;
    }
    j = (unsigned)primary;
    for (i = 0; i < n; i++) {
        j = tmp[j];
        out[i] = in[j];
    }
    return 1;
}
