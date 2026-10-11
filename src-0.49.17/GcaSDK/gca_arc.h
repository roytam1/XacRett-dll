///////////////////////////////////////////////////////////////////
// gca_arc.h -- GCA container + per-file decode orchestration.
///////////////////////////////////////////////////////////////////
#ifndef GCA_ARC_H
#define GCA_ARC_H

#include "gca_cod.h"
#include "gca_numv.h"
#include "gca_numb.h"
#include "gca_stage.h"
#include "gca_bwt.h"
#include <string>
#include <vector>

struct GcaFileEntry {
    std::string name;
    unsigned long expSize;
    unsigned long compSize; // framed length (0 = empty, no payload)
    unsigned long dataOff;  // absolute file offset of payload
    unsigned long crc;
    unsigned long attr;
    unsigned long ftime;    // DOS date<<16|time (from FILETIME conv)
    unsigned long ftimeHi, ftimeLo;
    int           hasTime;
};

/* Container flavours the DLL's sniffer (sub_10075960) dispatches on:
     "GCA0" -> arcType 0          "GCA1" -> arcType 1 (solid)
     "GCA3" -> seek + own reader (sub_10076930)
     "GCAX" -> "GflGCACryptograph" crypto path
   GCA_FMT_V3/GCA_FMT_X are recognised but not decoded here yet. */
enum GcaFormat {
    GCA_FMT_UNKNOWN = -1,
    GCA_FMT_V0      = 0,
    GCA_FMT_V1      = 1,
    GCA_FMT_V3      = 3,
    GCA_FMT_X       = 4
};

/* gcaParse return codes */
enum {
    GCA_PARSE_OK         =  0,
    GCA_PARSE_ERROR      = -1,  /* not a GCA archive, or malformed       */
    GCA_PARSE_UNSUPPORTED= -2   /* valid GCA3/GCAX, decoder not built yet */
};

struct GcaParsed {
    int arcType; // 0=GCA0, 1=GCA1
    int fmt;     // GcaFormat
    int flags;
    unsigned long crc;
    std::vector<GcaFileEntry> files;
};

// Parse container. base/len = whole file image, startOff = SFX offset.
// Returns a GCA_PARSE_* code.
int gcaParse(const unsigned char* base, unsigned long len,
             unsigned long startOff, GcaParsed& arc);

// Decode one file payload to out (resized to expSize).
// work: scratch buffers (owned by caller, see gca_api).
// Returns 0 ok, 1 cancel/error-ish (see GCAM codes via msg).
struct GcaWork {
    GcaProbTables pt;
    unsigned char* bufA;      // >= max needed (caller sized)
    unsigned long  bufASize;
    unsigned char* bufB;
    unsigned long  bufBSize;
    unsigned long* hist;      // 65536
    unsigned long* btwTmp;    // block-sized DWORD tmp
    unsigned long  btwTmpSize;
    int (*msgProc)(int, std::string, void*);
    void* msgCtx;
};

int gcaDecodeFile(const unsigned char* arcBase, const GcaFileEntry& e,
                  int solid, GcaWork& w, std::string& out);

#endif
