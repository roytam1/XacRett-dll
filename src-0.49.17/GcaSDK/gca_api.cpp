///////////////////////////////////////////////////////////////////
// gca_api.cpp -- nsGcaSDK-compatible wrapper (decode side).
///////////////////////////////////////////////////////////////////
#include "GcaSDK.h"
#include "gca_arc.h"
#include "gca_bwt.h"
#include "gca_sha1.h"
#include "../kiutil.h"   // 8.3 fallback (no-op where long names work)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace nsGcaSDK {

struct GcaSession {
    std::string arcPath;
    std::string outPath;
    std::string tmpPath;
    BOOL crypto;
    std::string spell;
    int startOff;
    BOOL (*progCb)(DWORD, DWORD, void*);
    void* progCtx;
    int (*msgCb)(int, string, void*);
    void* msgCtx;
    DWORD user;
    unsigned char* arcBytes;
    unsigned long arcLen;
    GcaParsed arc;
    int parsed;
    std::string parsedSpell;   /* spell the cached parse was made with */
    /* work */
    GcaWork w;
    GcaSession() : crypto(FALSE), startOff(0), progCb(0), progCtx(0),
        msgCb(0), msgCtx(0),
        user(0), arcBytes(0), arcLen(0), parsed(0) {
        memset(&w, 0, sizeof(w));
    }
};

static unsigned long gcaCrcTable[256];
static int gcaCrcInit = 0;
static void gcaMakeCrc()
{
    int i, j;
    if (gcaCrcInit) return;
    for (i = 0; i < 256; i++) {
        unsigned long c = (unsigned long)i;
        for (j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        gcaCrcTable[i] = c;
    }
    gcaCrcInit = 1;
}
static unsigned long gcaCrc(const unsigned char* b, unsigned long n)
{
    unsigned long c = 0xFFFFFFFFUL;
    gcaMakeCrc();
    while (n--) c = gcaCrcTable[(c ^ *b++) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFUL;
}

DWORD GetStartOffset(const string& path)
{
    /* TUNABLE behavior; probe vs original. Scan for a parseable
       GCA header (plain at 0, else SFX-embedded). */
    FILE* fp = fopen(path.c_str(), "rb");
    unsigned char* buf;
    long len, off;
    int found = 0;
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len < 36 || len > (1 << 28)) { fclose(fp); return 0; }
    buf = new unsigned char[len];
    if ((long)fread(buf, 1, len, fp) != len) {
        delete [] buf; fclose(fp); return 0;
    }
    fclose(fp);
    for (off = 0; off + 36 <= len; off++) {
        GcaParsed arc;
        int prc;
        if (memcmp(buf + off, "GCA", 3) != 0) {
            if (off > 0x100000) break; /* bounding: SFX stub < 1MB typically */
            continue;
        }
        prc = gcaParse(buf, (unsigned long)len, (unsigned long)off, arc);
        if (prc == GCA_PARSE_OK || prc == GCA_PARSE_UNSUPPORTED ||
            prc == GCA_PARSE_NEEDKEY) {
            /* UNSUPPORTED and NEEDKEY still mean "this offset holds a GCA
               archive", which is all GetStartOffset has to report. */
            found = 1;
            break;
        }
        /* skip ahead: only check plausible alignments? linear is fine */
        if (off > 0x100000) break; /* bounding: SFX stub < 1MB typically */
    }
    delete [] buf;
    return found ? (DWORD)off : 0;
}

BOOL GetSpellFromKeyFile(string& spell, const string& keyfile)
{
    /* gcac.exe hashes the key file with SHA-1 and uses the lower case
       hex digest of that hash as the archive password
       (gcac.exe.c:17433-17459: sub_412AF0/412B70/412C00/412CB0).
       `gcac a x.gca -k key.bin` is therefore byte identical to
       `gcac a x.gca -p <sha1hex(key.bin)>`. */
    FILE* fp;
    std::string data;
    char buf[4096];
    size_t n;
    char hex[41];

    fp = fopen(keyfile.c_str(), "rb");
    if (!fp) return FALSE;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
        data.append(buf, n);
    fclose(fp);
    gcaSha1Hex((const unsigned char*)data.data(),
               (unsigned long)data.size(), hex);
    spell = hex;
    return TRUE;
}

/* The original SDK allocates its context in the constructor (CGca::CGca ->
   operator new, then the context ctor, then ctx->owner = this). Callers
   such as CGcaTool::Check and COpenGca::FindFirst never call Init(), they
   go straight to SetArcFilePath/GetNumFiles, and CGca::GetNumFiles
   dereferences m_pContext unconditionally. So the context must already
   exist here; Init() only has to (re)build the work buffers. */
CGca::CGca() : m_pContext(0), m_dwUser(0), m_pfnProgress(0), m_pfnMessage(0)
{
    m_pContext = new GcaSession();
}
CGca::~CGca()
{
    Free();
}

/* Allocate (or re-allocate) the decode work buffers.
 *
 * The original SDK's callers never call Init() -- they go straight to
 * SetArcFilePath()/GetNumFiles() -- but Extract() needs the scratch
 * buffers, so they are allocated on demand here instead.
 */
static BOOL gcaAllocWork(GcaSession* s)
{
    if (s->w.bufA && s->w.bufB && s->w.hist)
        return TRUE;
    delete [] s->w.bufA;  s->w.bufA = 0;
    delete [] s->w.bufB;  s->w.bufB = 0;
    delete [] s->w.hist;  s->w.hist = 0;
    delete [] s->w.btwTmp; s->w.btwTmp = 0;
    s->w.btwTmpSize = 0;
    s->w.bufA = new unsigned char[0x400000];
    s->w.bufB = new unsigned char[0x400000];
    s->w.hist = new unsigned long[65536 + 16];
    if (!s->w.bufA || !s->w.bufB || !s->w.hist) {
        delete [] s->w.bufA;  s->w.bufA = 0;
        delete [] s->w.bufB;  s->w.bufB = 0;
        delete [] s->w.hist;  s->w.hist = 0;
        return FALSE;
    }
    s->w.bufASize = 0x400000;
    s->w.bufBSize = 0x400000;
    memset(s->w.hist, 0xFF, (65536 + 16) * sizeof(unsigned long));
    return TRUE;
}

BOOL CGca::Init(void)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s) {
        m_pContext = new GcaSession();
        s = (GcaSession*)m_pContext;
        if (!s) return FALSE;
    }
    /* Drop any previous parse result but keep arcPath/startOff/spell so a
       SetArcFilePath issued before Init() is not lost. */
    delete [] s->arcBytes;
    s->arcBytes = 0;
    s->arcLen = 0;
    s->parsed = 0;
    s->parsedSpell = "";
    s->arc.files.clear();
    s->arc.plain.clear();
    s->arc.encrypted = false;
    delete [] s->w.bufA;  s->w.bufA = 0;
    delete [] s->w.bufB;  s->w.bufB = 0;
    delete [] s->w.hist;  s->w.hist = 0;
    delete [] s->w.btwTmp; s->w.btwTmp = 0;
    s->w.btwTmpSize = 0;

    return gcaAllocWork(s);
}

void CGca::Free(void)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s) return;
    delete [] s->arcBytes;
    delete [] s->w.bufA;
    delete [] s->w.bufB;
    delete [] s->w.hist;
    delete [] s->w.btwTmp;
    delete s;
    m_pContext = 0;
}

void CGca::SetArcFilePath(const string& p) { if (m_pContext) ((GcaSession*)m_pContext)->arcPath = p; }
void CGca::SetOutputPath(const string& p) { if (m_pContext) ((GcaSession*)m_pContext)->outPath = p; }
void CGca::SetTempPath(const string& p) { if (m_pContext) ((GcaSession*)m_pContext)->tmpPath = p; }
void CGca::SetCrypto(BOOL b) { if (m_pContext) ((GcaSession*)m_pContext)->crypto = b; }
void CGca::SetSpell(const string& s) { if (m_pContext) { GcaSession* g = (GcaSession*)m_pContext; g->spell = s; g->parsed = 0; } }
void CGca::SetOrder(const vector<string>& v) { (void)v; }
void CGca::SetCompress(BOOL b) { (void)b; }
void CGca::SetHeaderCompress(BOOL b) { (void)b; }
void CGca::SetArcType(int n) { (void)n; }
void CGca::SetDataAnalyze(BOOL b) { (void)b; }
void CGca::SetStartOffset(int n) { if (m_pContext) ((GcaSession*)m_pContext)->startOff = n; }
void CGca::SetProgressProc(BOOL (*p)(DWORD, DWORD, PVOID)) { m_pfnProgress = p; if (m_pContext) { ((GcaSession*)m_pContext)->progCb = p; ((GcaSession*)m_pContext)->progCtx = (void*)this; } }
void CGca::SetMessageProc(int (*p)(int, string, PVOID)) { m_pfnMessage = p; if (m_pContext) { ((GcaSession*)m_pContext)->msgCb = p; ((GcaSession*)m_pContext)->msgCtx = (void*)this; } }

BOOL CGca::ProgressProc(DWORD a, DWORD b, void *p)
{
    (void)a; (void)b; (void)p;
    return TRUE;
}
int CGca::MessageProc(int n, string fn, void *p)
{
    (void)n; (void)fn; (void)p;
    return GCAM_OK;
}

/* The severity codes are not bit-disjoint: nERROR == nREPORT|nWARNING
   (12<<24 == 0x0C000000 overlaps nREPORT 0x04000000), so a plain
   "code & nERROR" test flags ordinary informational messages as errors.
   Compare the severity field instead. */
static const int GCA_SEVERITY_MASK = 0xFC000000;

static int gcaMsg(GcaSession* s, int code, const char* fn)
{
    if (s->msgCb)
        return s->msgCb(code, fn ? std::string(fn) : std::string(""),
                        s->msgCtx);
    if ((code & GCA_SEVERITY_MASK) == nERROR)
        return GCAM_CANCEL;
    return GCAM_OK;
}

/* Callbacks ride as plain params (CGca members pass their own);
   session is synced on every ensure (covers setter-before-Init). */
static int gcaEnsure(GcaSession* s, int (*msg)(int, string, void*),
                     void* msgCtx, BOOL (*prog)(DWORD, DWORD, void*),
                     void* progCtx)
{
    FILE* fp;
    long len;
    int prc;
    if (s) {
        s->msgCb = msg; s->msgCtx = msgCtx;
        s->progCb = prog; s->progCtx = progCtx;
    }
    if (!s || s->arcPath.empty())
        return 0;
    /* The decode work buffers are allocated lazily: the original SDK's
       callers never call Init(), but Extract() needs them. */
    if (!gcaAllocWork(s))
        return 0;
    /* An encrypted archive can only be parsed once the password is
       known, so a changed spell invalidates the cached parse. */
    if (s->parsed && s->parsedSpell == s->spell)
        return 1;
    if (!s->arcBytes) {
        fp = fopen(s->arcPath.c_str(), "rb");
        if (!fp)
            return 0;
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (len <= 0 || len > (1 << 28)) { fclose(fp); return 0; }
        s->arcBytes = new unsigned char[len];
        if ((long)fread(s->arcBytes, 1, len, fp) != len) {
            fclose(fp);
            delete [] s->arcBytes; s->arcBytes = 0;
            return 0;
        }
        fclose(fp);
        s->arcLen = (unsigned long)len;
    }
    s->arc.files.clear();
    s->arc.plain.clear();
    s->arc.encrypted = false;
    prc = gcaParse(s->arcBytes, s->arcLen, (unsigned long)s->startOff,
                   s->arc, s->spell);
    if (prc == GCA_PARSE_NEEDKEY) {
        /* encrypted archive, no (or wrong) password */
        gcaMsg(s, GCAM_ARC_CRYPTO_ERROR, s->arcPath.c_str());
        return 0;
    }
    if (prc == GCA_PARSE_UNSUPPORTED) {
        /* A real GCA container (GCA3/GCAX) that this build cannot
           decode yet. Report it as unknown rather than letting the
           caller see a half-built file list. */
        gcaMsg(s, GCAM_ARC_UNKNOWN_FORMAT, s->arcPath.c_str());
        return 0;
    }
    if (prc != GCA_PARSE_OK) {
        gcaMsg(s, GCAM_ARC_UNKNOWN_FORMAT, s->arcPath.c_str());
        return 0;
    }
    s->parsed = 1;
    s->parsedSpell = s->spell;
    return 1;
}

/* The image the payload offsets refer to.  For an encrypted archive the
   parser produced a decrypted copy; otherwise it is the file as read. */
static const unsigned char* gcaImage(const GcaSession* s)
{
    if (s->arc.encrypted && !s->arc.plain.empty())
        return &s->arc.plain[0];
    return s->arcBytes;
}

static void gcaMkDir(const char* path)
{
    char tmp[MAX_PATH];
    char* p;
    strncpy(tmp, path, MAX_PATH - 1); tmp[MAX_PATH - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '\\' || *p == '/') {
            char sv = *p; *p = 0;
            CreateDirectoryA(tmp, NULL);
            *p = sv;
        }
    }
    CreateDirectoryA(tmp, NULL);
}

int CGca::GetArcType(void)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return GCA0;
    return s->arc.arcType;
}
int CGca::GetNumFiles(void)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return 0;
    return (int)s->arc.files.size();
}
string CGca::GetFileName(int n)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return "";
    if (n < 0 || n >= (int)s->arc.files.size()) return "";
    return s->arc.files[n].name;
}
DWORD CGca::GetFileCRC(int n)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return 0;
    if (n < 0 || n >= (int)s->arc.files.size()) return 0;
    return s->arc.files[n].crc;
}
DWORD CGca::GetDataSize(int n)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return 0;
    if (n < 0 || n >= (int)s->arc.files.size()) return 0;
    return s->arc.files[n].compSize;
}
DWORD CGca::GetFileSize(int n)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return 0;
    if (n < 0 || n >= (int)s->arc.files.size()) return 0;
    return s->arc.files[n].expSize;
}
DWORD CGca::GetFileAttributes(int n)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return 0;
    if (n < 0 || n >= (int)s->arc.files.size()) return 0;
    return s->arc.files[n].attr;
}
FILETIME CGca::GetFileTime(int n)
{
    FILETIME ft = {0, 0};
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return ft;
    if (n < 0 || n >= (int)s->arc.files.size()) return ft;
    ft.dwLowDateTime = s->arc.files[n].ftimeLo;
    ft.dwHighDateTime = s->arc.files[n].ftimeHi;
    return ft;
}

BOOL CGca::OpenArchive(void)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s) return FALSE;
    return gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this) ? TRUE : FALSE;
}
BOOL CGca::CloseArchive(void)
{
    GcaSession* s = (GcaSession*)m_pContext;
    if (!s) return FALSE;
    delete [] s->arcBytes; s->arcBytes = 0; s->arcLen = 0;
    s->arc.files.clear();
    s->arc.plain.clear();
    s->arc.encrypted = false;
    s->parsed = 0;
    s->parsedSpell = "";
    return TRUE;
}

static BOOL gcaWriteOne(GcaSession* s, const GcaFileEntry& e, const std::string& data)
{
    std::string out = s->outPath;
    std::string rel = e.name;
    char* p;
    FILE* fp;
    FILETIME ft;
    if (!out.empty() && out[out.size()-1] != '\\' && out[out.size()-1] != '/')
        out += "\\";
    out += rel;
    for (p = &out[0]; *p; p++) if (*p == '/') *p = '\\';
    /* 8.3 fallback (no-op wherever long names work); must run before the
       mkdir below so the directories match the mangled file name */
    {
        char tmp83[MAX_PATH * 2];
        strncpy(tmp83, out.c_str(), sizeof(tmp83) - 1); tmp83[sizeof(tmp83)-1] = 0;
        kiutil::pathMake83(tmp83);
        out.assign(tmp83);
    }
    /* a directory member is created as a directory, not as a file */
    if (e.attr & 0x10) {
        gcaMkDir(out.c_str());
        if (e.attr)
            SetFileAttributesA(out.c_str(), e.attr);
        return TRUE;
    }
    /* mkdir -p for parent */
    {
        char tmp[MAX_PATH * 2];
        char* q;
        strncpy(tmp, out.c_str(), sizeof(tmp) - 1); tmp[sizeof(tmp)-1] = 0;
        q = strrchr(tmp, '\\');
        if (q) { *q = 0; gcaMkDir(tmp); }
    }
if (gcaMsg(s, GCAM_FILE_DECODE_BEGIN, e.name.c_str()) == GCAM_CANCEL)
        return FALSE;
    fp = fopen(out.c_str(), "wb");
    if (!fp) {
        gcaMsg(s, GCAM_FILE_WRITE_ERROR, e.name.c_str());
        return FALSE;
    }
    if (!data.empty() &&
        fwrite(data.data(), 1, data.size(), fp) != data.size()) {
        fclose(fp);
        gcaMsg(s, GCAM_FILE_WRITE_ERROR, e.name.c_str());
        return FALSE;
    }
    fclose(fp);
if (gcaCrc((const unsigned char*)data.data(),
               (unsigned long)data.size()) != e.crc) {
        if (gcaMsg(s, GCAM_FILE_CRC_ERROR, e.name.c_str()) == GCAM_CANCEL)
            return FALSE;
    }
    ft.dwLowDateTime = e.ftimeLo;
    ft.dwHighDateTime = e.ftimeHi;
    {
        HANDLE h = CreateFileA(out.c_str(), GENERIC_WRITE, 0, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            SetFileTime(h, NULL, NULL, &ft);
            CloseHandle(h);
        }
    }
    if (e.attr)
        SetFileAttributesA(out.c_str(), e.attr);
    if (gcaMsg(s, GCAM_FILE_DECODE_END, e.name.c_str()) == GCAM_CANCEL)
        return FALSE;
    return TRUE;
}

BOOL CGca::Extract(void)
{
    GcaSession* s = (GcaSession*)m_pContext;
    unsigned i;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return FALSE;
    if (s->arc.arcType == 1) {
        /* solid: one concat payload -> slice by exp sizes */
        std::string cat;
        unsigned long off = 0;
        if (gcaMsg(s, GCAM_ARC_ANALYZE_SOLID, s->arcPath.c_str()) == GCAM_CANCEL)
            return FALSE;
        if (s->arc.files.empty())
            return FALSE;
        s->w.msgProc = 0; s->w.msgCtx = 0;
        if (gcaDecodeFile(gcaImage(s),s->arc.files[0], 1, s->w, cat) != 0) {
            gcaMsg(s, GCAM_FILE_READ_ERROR, s->arc.files[0].name.c_str());
            return FALSE;
        }
        for (i = 0; i < s->arc.files.size(); i++) {
            const GcaFileEntry& e = s->arc.files[i];
            std::string data;
            if (off + e.expSize > cat.size()) {
                gcaMsg(s, GCAM_FILE_READ_ERROR, e.name.c_str());
                return FALSE;
            }
            data.assign(cat.data() + off, e.expSize);
            off += e.expSize;
            if (!gcaWriteOne(s, e, data))
                return FALSE;
        }
        return TRUE;
    }
    for (i = 0; i < s->arc.files.size(); i++) {
        const GcaFileEntry& e = s->arc.files[i];
        std::string data;
        s->w.msgProc = 0; s->w.msgCtx = 0;
        if (gcaDecodeFile(gcaImage(s),e, 0, s->w, data) != 0) {
            gcaMsg(s, GCAM_FILE_READ_ERROR, e.name.c_str());
            return FALSE;
        }
        if (!gcaWriteOne(s, e, data))
            return FALSE;
    }
    return TRUE;
}

/* Solid helper: decode concat once, slice file n. */
static BOOL gcaSolidSlice(GcaSession* s, int n, std::string& data)
{
    std::string cat;
    unsigned i;
    unsigned long off = 0;
    if (s->arc.files.empty() || n < 0 ||
        n >= (int)s->arc.files.size())
        return FALSE;
    s->w.msgProc = 0; s->w.msgCtx = 0;
    if (gcaDecodeFile(gcaImage(s),s->arc.files[0], 1, s->w, cat) != 0)
        return FALSE;
    for (i = 0; i < (unsigned)n; i++)
        off += s->arc.files[i].expSize;
    if (off + s->arc.files[n].expSize > cat.size())
        return FALSE;
    data.assign(cat.data() + off, s->arc.files[n].expSize);
    return TRUE;
}

BOOL CGca::ExtractFile(int n)
{
    GcaSession* s = (GcaSession*)m_pContext;
    std::string data;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this)) return FALSE;
    if (n < 0 || n >= (int)s->arc.files.size()) return FALSE;
    if (s->arc.arcType == 1) {
        if (!gcaSolidSlice(s, n, data)) {
            gcaMsg(s, GCAM_FILE_READ_ERROR,
                   s->arc.files[n].name.c_str());
            return FALSE;
        }
    } else if (gcaDecodeFile(gcaImage(s),s->arc.files[n],
                             0, s->w, data) != 0)
        return FALSE;
    return gcaWriteOne(s, s->arc.files[n], data);
}

BOOL CGca::ExtractFileToMemory(int n, BYTE *pBuffer)
{
    GcaSession* s = (GcaSession*)m_pContext;
    std::string data;
    if (!s || !gcaEnsure(s, m_pfnMessage, (void*)this, m_pfnProgress, (void*)this) || !pBuffer) return FALSE;
    if (n < 0 || n >= (int)s->arc.files.size()) return FALSE;
    if (s->arc.arcType == 1) {
        if (!gcaSolidSlice(s, n, data))
            return FALSE;
    } else if (gcaDecodeFile(gcaImage(s),s->arc.files[n],
                             0, s->w, data) != 0)
        return FALSE;
    memcpy(pBuffer, data.data(), data.size());
    return TRUE;
}

}
