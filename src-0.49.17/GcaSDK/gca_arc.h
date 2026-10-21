/////////////////////////////////////////////////////////////////
// gca_arc.h -- GCA container + per-file decode orchestration.
/////////////////////////////////////////////////////////////////
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
    unsigned long compSize; // bytes this member occupies in the payload
                            // region (frame(s) for sequential archives, the
                            // whole region for solid archives).  0 = empty.
    unsigned long dataOff;  // absolute file offset of payload
    unsigned long crc;
    unsigned long attr;     // Win32 dwFileAttributes (0x10 = directory)
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
    GCA_PARSE_UNSUPPORTED= -2,  /* valid GCA3/GCAX, decoder not built yet */
    GCA_PARSE_NEEDKEY    = -3   /* encrypted archive, no password given   */
};

/* GCA3 directory flags (gcac.exe writes these; see GCA_FORMAT.md) */
#define GCA_DIRF_COMPRESSED  0x001u /* the directory body is compressed      */
#define GCA_DIRF_DIRENC      0x002u /* the directory body is encrypted      */
#define GCA_DIRF_PAYCOMP     0x010u /* the payload is compressed            */
#define GCA_DIRF_CRYPTO      0x020u /* crypto is enabled                    */
#define GCA_DIRF_SOLID       0x100u /* solid archive (one shared payload)   */

struct GcaParsed {
    int arcType; // 0=GCA0, 1=GCA1
    int fmt;     // GcaFormat
    int flags;   // raw GCA3 flags word
    int crypto;      /* (flags & GCA_DIRF_CRYPTO) != 0      */
    int dirEncrypted;/* (flags & GCA_DIRF_DIRENC) != 0      */
    unsigned long crc;
    std::vector<GcaFileEntry> files;
    /* When the archive was encrypted the parser decrypts it into `plain`
     * and all offsets in `files` refer to that image instead of the
     * caller's buffer.  `plain` is empty for plain archives. */
    bool encrypted;
    std::vector<unsigned char> plain;
};

// Parse container. base/len = whole file image, startOff = SFX offset.
// Returns a GCA_PARSE_* code.
//
// `spell` is the archive password (the raw -p text, or the hex SHA-1
// digest of a -k key file).  It is only needed for encrypted archives;
// when one is encountered and no spell is supplied the call fails with
// GCA_PARSE_NEEDKEY.
int gcaParse(const unsigned char* base, unsigned long len,
             unsigned long startOff, GcaParsed& arc,
             const std::string& spell);

// Convenience overload for unencrypted archives.
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
