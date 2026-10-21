///////////////////////////////////////////////////////////////////
// gca_arc.cpp -- container parse + per-file decode.
///////////////////////////////////////////////////////////////////
#include "gca_arc.h"
#include "gca_bwt.h"
#include "gca_crypt.h"
#include <string.h>
#include <stdlib.h>

static unsigned long rd32(const unsigned char* p)
{
    return ((unsigned long)p[0]) | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

struct GcaRawEnt {
    unsigned long ftlo, fthi, attr, crc, comp, exp;
};

static int decodeGcf1Core(const unsigned char* rsp, unsigned long rleLen,
                          const unsigned char* lsp, unsigned long lzLen,
                          unsigned long ex, unsigned long rleOut,
                          unsigned long prim, int resetHist,
                          GcaWork& w, std::string& out);

/* One framed/raw payload; defined below. */
static int decodePayload(const unsigned char* p, unsigned long compLen,
                         unsigned long expLen, int solid,
                         GcaWork& w, std::string& out);

/* ------------------------------------------------------------------ */
/* GCAX container - the layout 'gcac' (GCA SDK 0.9k) actually writes.
 *
 *   +0x00 "GCAX"
 *   +0x04 0
 *   +0x08 dirOffset   64 bit, RELATIVE to the start of this header
 *   +0x0C (high dword of dirOffset)
 *   +0x10 .. dirOffset   one payload frame per entry, in entry order,
 *                         entry.storedLen bytes each (GCF0/GCF1/GCF2)
 *   dirOffset: GCA3 directory block
 *        +0x00 "GCA3"   +0x04 flags (0x10 raw blocks / 0x11 GCF1 framed)
 *        +0x08 blockSize  +0x14 CRC32  +0x18 numFiles
 *        +0x20 entry table, numFiles*32 bytes  [raw or GCF1]
 *              names block, u16 len + name * numFiles [raw or GCF1]
 *
 *   32-byte entry:
 *        +0x00 content CRC32      +0x04 dwFileAttributes
 *        +0x08 FILETIME low       +0x0C FILETIME high
 *        +0x10 original size      +0x14 (high dword of the original size)
 *        +0x18 stored length      +0x1C (high dword of the stored length)
 *
 * NOTE: dirOffset is relative, not absolute.  gcac.exe writes
 * (directory position - header position) and its own reader adds the
 * archive start back on; verified by prefixing an archive with a
 * filler blob and re-parsing it.
 * ------------------------------------------------------------------ */

/* Read a directory block that is GCF0/GCF1/GCF2 framed or stored raw.
   rawLen is only used for the unframed case. *consumed = bytes used.
   Framed blocks go through the same decodePayload() the payload path
   uses, so all three codecs behave identically here. */
static int gcaReadBlock(const unsigned char* base, unsigned long len,
                        unsigned long off, unsigned long rawLen,
                        GcaWork& w, std::string& out, unsigned long* consumed)
{
    unsigned long clen, elen;
    if (off + 4 > len)
        return GCA_PARSE_ERROR;
    if (memcmp(base + off, "GCF0", 4) == 0) {
        if (off + 8 > len) return GCA_PARSE_ERROR;
        elen = rd32(base + off + 4);
        clen = 8 + elen;
    } else if (memcmp(base + off, "GCF1", 4) == 0) {
        if (off + 24 > len) return GCA_PARSE_ERROR;
        elen = rd32(base + off + 4);
        clen = 24 + rd32(base + off + 16) + rd32(base + off + 20);
    } else if (memcmp(base + off, "GCF2", 4) == 0) {
        if (off + 28 > len) return GCA_PARSE_ERROR;
        elen = rd32(base + off + 4);
        clen = 28 + rd32(base + off + 16) + rd32(base + off + 20);
    } else {
        if (off + rawLen > len) return GCA_PARSE_ERROR;
        out.assign((const char*)(base + off), rawLen);
        if (consumed) *consumed = rawLen;
        return GCA_PARSE_OK;
    }
    if (off + clen > len) return GCA_PARSE_ERROR;
    if (decodePayload(base + off, clen, elen, 0, w, out) != 0)
        return GCA_PARSE_ERROR;
    if (out.size() != (size_t)elen) return GCA_PARSE_ERROR;
    if (consumed) *consumed = clen;
    return GCA_PARSE_OK;
}

/* Locate the GCA3 directory of a GCAX container.
 *
 * dirOffset lives at GCAX+0x08 as a 64 bit value and is RELATIVE to
 * the start of the GCAX header (gcac.exe writes
 * "directory position - header position" and its own reader adds the
 * archive start back on).  Only the low 32 bits are honoured here:
 * the SDK caps archives at 256 MB, so a non-zero high dword means the
 * container is out of range.
 */
static int gcaLocateDir(const unsigned char* base, unsigned long len,
                        unsigned long startOff, unsigned long* dirAbsOut)
{
    unsigned long dirLo, dirHi, dirOff;
    if (startOff + 16 > len)
        return GCA_PARSE_ERROR;
    dirLo = rd32(base + startOff + 8);
    dirHi = rd32(base + startOff + 12);
    if (dirHi != 0)
        return GCA_PARSE_ERROR;
    dirOff = dirLo;
    if (dirOff < 16)
        return GCA_PARSE_ERROR;
    if (startOff + dirOff + 0x20 > len)
        return GCA_PARSE_ERROR;
    *dirAbsOut = startOff + dirOff;
    return GCA_PARSE_OK;
}

/* Parse a GCAX container whose payload and directory are plaintext. */
static int gcaParseGcaxPlain(const unsigned char* base, unsigned long len,
                              unsigned long startOff, GcaParsed& arc)
{
    unsigned long d, dirAbs, numFiles, i, pos, entsUsed = 0;
    unsigned long pos2, flags = 0, hdr = 0x20;
    unsigned long firstFrameOff;
    std::string ents, names;
    GcaWork w;
    int solid = 0;

    memset(&w, 0, sizeof(w));
    w.hist = new unsigned long[65536 + 16];
    if (!w.hist)
        return GCA_PARSE_ERROR;

    if (gcaLocateDir(base, len, startOff, &dirAbs) != GCA_PARSE_OK)
        goto fail;
    d = dirAbs;
    firstFrameOff = startOff + 0x10;
    if (memcmp(base + d, "GCA3", 4) != 0) goto fail;

    numFiles = rd32(base + d + 0x18);
    if (numFiles == 0 || numFiles > 100000) goto fail;

    /* flags bit0: blocks are GCF-framed. That case carries one extra
       header DWORD (+0x20 = combined uncompressed size) and so has a
       0x28-byte header instead of 0x20. blockSize corroborates:
       raw  = 0x20 + entries + names
       frame= 0x28 + frame(entries) + frame(names) */
    flags = rd32(base + d + 4);
    hdr = (flags & GCA_DIRF_COMPRESSED) ? 0x28u : 0x20u;

    /* entry table */
    if (gcaReadBlock(base, len, d + hdr, numFiles * 32, w, ents, &entsUsed) != 0)
        goto fail;
    if (ents.size() != (size_t)(numFiles * 32)) goto fail;

    /* names block: framed, else read raw in place (self-delimiting) */
    pos2 = d + hdr + entsUsed;
    if (pos2 + 4 <= len &&
        (memcmp(base + pos2, "GCF0", 4) == 0 ||
         memcmp(base + pos2, "GCF1", 4) == 0 ||
         memcmp(base + pos2, "GCF2", 4) == 0)) {
        if (gcaReadBlock(base, len, pos2, 0, w, names, NULL) != 0) goto fail;
    } else {
        names.assign((const char*)(base + pos2), len - pos2);
    }

    /* Solid archives store one concatenated stream shared by every member, so
       all per-entry stored lengths are 0. Sequential archives sum exactly to
       the data region (dirOff - 0x10). Verified against gcac output for both
       modes, compressed and stored, small and chunked.  The SOLID flag is
       authoritative when it is present; the sum is only a fallback. */
    solid = 0;
    if (flags & GCA_DIRF_SOLID) {
        solid = 1;
    } else {
        unsigned long sum = 0;
        for (i = 0; i < numFiles; i++)
            sum += rd32((const unsigned char*)ents.data() + i * 32 + 0x18);
        if (sum == 0 && dirAbs > firstFrameOff + 8)
            solid = 1;
    }
    pos = firstFrameOff;
    if (solid) {
        /* One concatenated stream for every member; per-entry stored
           length is 0, so the extent is the whole data region (it may
           itself be several GCF2 chunks). */
        unsigned long flen;
        if (firstFrameOff + 28 > len || dirAbs < firstFrameOff + 28) goto fail;
        flen = dirAbs - firstFrameOff;
        for (i = 0; i < numFiles; i++) {
            const unsigned char* e = (const unsigned char*)ents.data() + i * 32;
            GcaFileEntry fe;
            fe.crc     = rd32(e + 0x00);
            fe.attr    = rd32(e + 0x04);   /* dwFileAttributes */
            fe.ftimeLo = rd32(e + 0x08);
            fe.ftimeHi = rd32(e + 0x0C);
            fe.ftime   = 0;
            fe.hasTime = 1;
            fe.expSize = rd32(e + 0x10);
            fe.dataOff  = firstFrameOff;
            fe.compSize = flen;
            arc.files.push_back(fe);
        }
    } else
    for (i = 0; i < numFiles; i++) {
        const unsigned char* e = (const unsigned char*)ents.data() + i * 32;
        GcaFileEntry fe;
        unsigned long stored = rd32(e + 0x18);
        fe.crc       = rd32(e + 0x00);
        fe.attr      = rd32(e + 0x04);   /* dwFileAttributes */
        fe.ftimeLo   = rd32(e + 0x08);
        fe.ftimeHi   = rd32(e + 0x0C);
        fe.ftime     = 0;
        fe.hasTime   = 1;
        fe.expSize   = rd32(e + 0x10);
        if (pos + stored > len) goto fail;
        fe.dataOff  = pos;
        fe.compSize = stored;
        pos += stored;
        arc.files.push_back(fe);
    }

    /* names -> entries, in order */
    {
        size_t p = 0;
        for (i = 0; i < numFiles; i++) {
            unsigned long n;
            if (p + 2 > names.size()) goto fail;
            n = (unsigned char)names[p] | ((unsigned long)(unsigned char)names[p + 1] << 8);
            p += 2;
            if (n == 0 || p + n > names.size()) goto fail;
            arc.files[i].name.assign(names.data() + p, n);
            p += n;
        }
    }

    arc.arcType = solid ? 1 : 0;
    arc.fmt     = GCA_FMT_X;
    arc.flags   = (int)flags;
    arc.crc     = rd32(base + d + 0x14);

    delete[] w.hist;
    delete[] w.bufA;
    delete[] w.bufB;
    delete[] w.btwTmp;
    return GCA_PARSE_OK;

fail:
    delete[] w.hist;
    delete[] w.bufA;
    delete[] w.bufB;
    delete[] w.btwTmp;
    arc.files.clear();
    return GCA_PARSE_ERROR;
}

/* Build a plaintext image of an encrypted GCAX archive.
 *
 * gcac.exe encrypts three things:
 *   - the payload region.  Sequential archives use one keystream per
 *     member keyed with "<password><member name>"; solid archives use
 *     a single keystream keyed with "<password>".
 *   - the 16 bytes at GCA3+0x10 (keystream bytes 0..15).  Their
 *     plaintext is (0, dirCRC32, numFiles, 0).
 *   - the directory body (keystream bytes 16..).
 * The GCAX header and GCA3+0x00..0x0F stay plaintext.
 *
 * The result has exactly the same layout and length as the input, so
 * every offset computed by the plain parser stays valid.
 */
static int gcaDecryptImage(const unsigned char* base, unsigned long len,
                           unsigned long startOff, const std::string& spell,
                           std::vector<unsigned char>& out)
{
    static const unsigned char kEmpty[1] = { 0 };
    unsigned long dirAbs, flags, blockSize, bodyLen, payloadOff, payloadLen;
    unsigned long i, numFiles, bodyBase, entsUsed = 0, pos2, off;
    unsigned long plainFlags;
    const unsigned char* bodyPtr;
    std::string ents, names, key;
    std::vector<unsigned char> body;
    std::vector<std::string> fileNames;
    GcaWork w;
    GcaCrypt ks;
    int rc = GCA_PARSE_ERROR;

    if (gcaLocateDir(base, len, startOff, &dirAbs) != GCA_PARSE_OK)
        return GCA_PARSE_ERROR;
    if (memcmp(base + dirAbs, "GCA3", 4) != 0)
        return GCA_PARSE_ERROR;
    flags = rd32(base + dirAbs + 4);
    blockSize = rd32(base + dirAbs + 8);
    if (blockSize < 0x20)
        return GCA_PARSE_ERROR;
    bodyLen = blockSize - 0x20;
    if (dirAbs + 0x20 + bodyLen > len)
        return GCA_PARSE_ERROR;

    memset(&w, 0, sizeof(w));
    w.hist = new unsigned long[65536 + 16];
    if (!w.hist)
        return GCA_PARSE_ERROR;

    /* the 1997 STLport that the VC4 build uses has no iterator
       range form of assign(), so the copy is done by hand */
    out.clear();
    out.resize(len);
    if (len) memcpy((unsigned char*)&out[0], base, len);

    /* --- the directory ------------------------------------------- */
    key = gcaCryptDirKey(spell);
    ks.setKey((const unsigned char*)key.data(), (unsigned long)key.size());
    ks.crypt(&out[dirAbs + 0x10], 16);            /* keystream bytes 0..15 */
    body.resize(bodyLen);
    if (bodyLen) {
        memcpy(&body[0], base + dirAbs + 0x20, bodyLen);
        ks.crypt(&body[0], bodyLen);               /* keystream bytes 16..  */
        memcpy(&out[dirAbs + 0x20], &body[0], bodyLen);
    }
    /* clear the crypto bits: the plain parser must not try again */
    plainFlags = flags & ~(unsigned long)(GCA_DIRF_CRYPTO | GCA_DIRF_DIRENC);
    out[dirAbs + 4] = (unsigned char)(plainFlags & 0xFF);
    out[dirAbs + 5] = (unsigned char)((plainFlags >> 8) & 0xFF);
    out[dirAbs + 6] = (unsigned char)((plainFlags >> 16) & 0xFF);
    out[dirAbs + 7] = (unsigned char)((plainFlags >> 24) & 0xFF);

    numFiles = rd32(&out[dirAbs + 0x18]);
    if (numFiles == 0 || numFiles > 100000) goto done;

    /* the entry table + names: needed for the per member payload keys */
    bodyPtr = bodyLen ? &body[0] : kEmpty;
    bodyBase = (flags & GCA_DIRF_COMPRESSED) ? 8u : 0u;
    if (gcaReadBlock(bodyPtr, bodyLen, bodyBase, numFiles * 32,
                     w, ents, &entsUsed) != 0)
        goto done;
    if (ents.size() != (size_t)(numFiles * 32)) goto done;
    pos2 = bodyBase + entsUsed;
    if (pos2 + 4 <= bodyLen &&
        (memcmp(&body[pos2], "GCF0", 4) == 0 ||
         memcmp(&body[pos2], "GCF1", 4) == 0 ||
         memcmp(&body[pos2], "GCF2", 4) == 0)) {
        if (gcaReadBlock(bodyPtr, bodyLen, pos2, 0, w, names, NULL) != 0)
            goto done;
    } else {
        if (pos2 > bodyLen) goto done;
        names.assign((const char*)&body[pos2], bodyLen - pos2);
    }
    {
        size_t p = 0;
        for (i = 0; i < numFiles; i++) {
            unsigned long n;
            if (p + 2 > names.size()) goto done;
            n = (unsigned char)names[p] |
                ((unsigned long)(unsigned char)names[p + 1] << 8);
            p += 2;
            if (n == 0 || p + n > names.size()) goto done;
            fileNames.push_back(names.substr(p, n));
            p += n;
        }
    }

    /* --- the payload --------------------------------------------- */
    payloadOff = startOff + 0x10;
    if (dirAbs < payloadOff) goto done;
    payloadLen = dirAbs - payloadOff;
    if (flags & GCA_DIRF_SOLID) {
        key = gcaCryptSolidKey(spell);
        ks.setKey((const unsigned char*)key.data(), (unsigned long)key.size());
        if (payloadLen) ks.crypt(&out[payloadOff], payloadLen);
    } else {
        off = payloadOff;
        for (i = 0; i < numFiles; i++) {
            unsigned long stored =
                rd32((const unsigned char*)ents.data() + i * 32 + 0x18);
            key = gcaCryptPayloadKey(spell, fileNames[i]);
            ks.setKey((const unsigned char*)key.data(), (unsigned long)key.size());
            if (stored) {
                if (off + stored > dirAbs) goto done;
                ks.crypt(&out[off], stored);
            }
            off += stored;
        }
    }
    rc = GCA_PARSE_OK;

done:
    delete[] w.hist;
    delete[] w.bufA;
    delete[] w.bufB;
    delete[] w.btwTmp;
    if (rc != GCA_PARSE_OK)
        out.clear();
    return rc;
}

static int gcaParseGcax(const unsigned char* base, unsigned long len,
                        unsigned long startOff, GcaParsed& arc,
                        const std::string& spell)
{
    unsigned long dirAbs, flags;
    std::vector<unsigned char> plain;
    int rc;

    if (gcaLocateDir(base, len, startOff, &dirAbs) != GCA_PARSE_OK)
        return GCA_PARSE_ERROR;
    if (memcmp(base + dirAbs, "GCA3", 4) != 0)
        return GCA_PARSE_ERROR;
    flags = rd32(base + dirAbs + 4);

    if (flags & (GCA_DIRF_CRYPTO | GCA_DIRF_DIRENC)) {
        if (spell.empty())
            return GCA_PARSE_NEEDKEY;
        if (gcaDecryptImage(base, len, startOff, spell, plain) != GCA_PARSE_OK)
            return GCA_PARSE_ERROR;
        arc.encrypted = true;
        arc.plain.swap(plain);
        rc = gcaParseGcaxPlain(&arc.plain[0], (unsigned long)arc.plain.size(),
                               startOff, arc);
        arc.crypto       = (flags & GCA_DIRF_CRYPTO) != 0;
        arc.dirEncrypted = (flags & GCA_DIRF_DIRENC) != 0;
        return rc;
    }

    arc.encrypted = false;
    rc = gcaParseGcaxPlain(base, len, startOff, arc);
    arc.crypto       = (flags & GCA_DIRF_CRYPTO) != 0;
    arc.dirEncrypted = (flags & GCA_DIRF_DIRENC) != 0;
    return rc;
}


int gcaParse(const unsigned char* base, unsigned long len,
             unsigned long startOff, GcaParsed& arc)
{
    return gcaParse(base, len, startOff, arc, std::string());
}

int gcaParse(const unsigned char* base, unsigned long len,
             unsigned long startOff, GcaParsed& arc, const std::string& spell)
{
    const unsigned char* h;
    unsigned long ver, cnt, hdr, flags, crc, c0, e0;
    unsigned long off;
    arc.files.clear();
    arc.fmt = GCA_FMT_UNKNOWN;
    arc.encrypted = false;
    arc.plain.clear();
    arc.crypto = 0;
    arc.dirEncrypted = 0;
    if (startOff + 36 > len)
        return GCA_PARSE_ERROR;
    h = base + startOff;
    /* Mirror the DLL's sniffer order (sub_10075960): a "GCA" prefix is
       required, then the 4th byte selects the container. */
    if (memcmp(h, "GCA", 3) != 0)
        return GCA_PARSE_ERROR;
    if (memcmp(h, "GCAX", 4) == 0) {
        arc.fmt = GCA_FMT_X;
        return gcaParseGcax(base, len, startOff, arc, spell);
    }
    if (memcmp(h, "GCA3", 4) == 0) {
        arc.fmt = GCA_FMT_V3;
        return GCA_PARSE_UNSUPPORTED;
    }
    if (memcmp(h, "GCA0", 4) != 0 && memcmp(h, "GCA1", 4) != 0)
        return GCA_PARSE_ERROR;
    arc.fmt = (h[3] == '1') ? GCA_FMT_V1 : GCA_FMT_V0;
    arc.arcType = (h[3] == '1') ? 1 : 0;
    ver = rd32(h + 4); (void)ver;
    cnt = rd32(h + 8);
    hdr = rd32(h + 12);
    flags = rd32(h + 16);
    crc = rd32(h + 20);
    c0 = rd32(h + 28);
    e0 = rd32(h + 32);
    arc.flags = (int)flags;
    arc.crc = crc;
    if (cnt == 0 || cnt > 100000)
        return GCA_PARSE_ERROR;
    if (startOff + hdr > len)
        return GCA_PARSE_ERROR;
    if (arc.arcType == 1) {
        /* solid: single concat payload runs to EOF; the c0 slot
           reads 0. Derive from the file length. */
        c0 = len - (startOff + hdr);
    }
    if (ver == 3) {
        /* header-compress (-h): main36 carries a GCF1 frame header
           ([magic][F0][F1][prim] at h+20..h+32, [rleLen][lzLen]
           at +36/+40); frame1 decodes to 32*cnt bytes
           ([file#1 20B][(cnt-1) 32B entries][12B footer]);
           frame2 (framed GCF1) holds the names pool; 8B trailer
           (4 zeros + header CRC32, coverage unknown: unchecked). */
        unsigned long F0, F1, prim, rleLen, lzLen, f2;
        unsigned long n2ex, n2ro, n2prim, n2rl, n2ll;
        std::string dec1, dec2;
        std::vector<std::string> names;
        std::vector<GcaRawEnt> ents;
        unsigned long f0lo = 0, f0hi = 0;
        unsigned long p;
        GcaWork hw;
        int k, k2;
        if (memcmp(h + 20, "GCF1", 4) != 0)
            return GCA_PARSE_ERROR;
        if (startOff + 44 > startOff + hdr)
            return GCA_PARSE_ERROR;
        F0 = rd32(h + 24); F1 = rd32(h + 28); prim = rd32(h + 32);
        rleLen = rd32(base + startOff + 36);
        lzLen = rd32(base + startOff + 40);
        if (F0 != 32 * cnt)
            return GCA_PARSE_ERROR;
        if (startOff + 44 + rleLen + lzLen > startOff + hdr)
            return GCA_PARSE_ERROR;
        memset(&hw, 0, sizeof(hw));
        hw.hist = new unsigned long[65536 + 16];
        if (!hw.hist)
            return GCA_PARSE_ERROR;
        if (decodeGcf1Core(base + startOff + 44, rleLen,
                           base + startOff + 44 + rleLen, lzLen,
                           F0, F1, prim, 1, hw, dec1) != 0) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return GCA_PARSE_ERROR;
        }
        if (dec1.size() != (size_t)(32 * cnt)) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return GCA_PARSE_ERROR;
        }
        /* frame2: names pool (framed GCF1) */
        f2 = startOff + 44 + rleLen + lzLen;
        if (f2 + 24 > startOff + hdr ||
            memcmp(base + f2, "GCF1", 4) != 0) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return GCA_PARSE_ERROR;
        }
        n2ex = rd32(base + f2 + 4); n2ro = rd32(base + f2 + 8);
        n2prim = rd32(base + f2 + 12); n2rl = rd32(base + f2 + 16);
        n2ll = rd32(base + f2 + 20);
        if (f2 + 24 + n2rl + n2ll > startOff + hdr) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return GCA_PARSE_ERROR;
        }
        if (decodeGcf1Core(base + f2 + 24, n2rl,
                           base + f2 + 24 + n2rl, n2ll,
                           n2ex, n2ro, n2prim, 1, hw, dec2) != 0) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return GCA_PARSE_ERROR;
        }
        delete [] hw.hist; delete [] hw.bufA;
        delete [] hw.bufB; delete [] hw.btwTmp;
        /* entries from dec1 */
        {
            const unsigned char* d =
                (const unsigned char*)dec1.data();
            unsigned long eoff = 20;
            for (k2 = 0; k2 < (int)cnt - 1; k2++) {
                GcaRawEnt e;
                e.ftlo = rd32(d + eoff + 4);
                e.fthi = rd32(d + eoff + 8);
                e.attr = rd32(d + eoff + 12);
                e.crc = rd32(d + eoff + 16);
                e.comp = rd32(d + eoff + 24);
                e.exp = rd32(d + eoff + 28);
                ents.push_back(e);
                eoff += 32;
            }
            f0lo = rd32(d + eoff + 4);
            f0hi = rd32(d + eoff + 8);
            /* names from dec2 pool */
            p = 0;
            for (k = 0; k < (int)cnt; k++) {
                unsigned ln;
                if (p >= dec2.size())
                    return GCA_PARSE_ERROR;
                ln = ((const unsigned char*)dec2.data())[p++];
                if (p + ln > dec2.size())
                    return GCA_PARSE_ERROR;
                names.push_back(std::string(dec2.data() + p, ln));
                p += ln;
            }
            /* file#1 (decompressed slots; solid comp via EOF) */
            {
                GcaFileEntry fe;
                unsigned long comp0 = rd32(d + 12);
                if (arc.arcType == 1)
                    comp0 = len - (startOff + hdr);
                fe.name = names[0];
                fe.expSize = rd32(d + 16);
                fe.compSize = comp0;
                fe.ftimeLo = f0lo; fe.ftimeHi = f0hi; fe.hasTime = 1;
                fe.crc = rd32(d + 4);
                fe.attr = 0;
                fe.dataOff = 0;
                arc.files.push_back(fe);
            }
            for (k = 0; k < (int)ents.size(); k++) {
                GcaFileEntry fe;
                fe.name = names[k + 1];
                fe.expSize = ents[k].exp;
                fe.compSize = ents[k].comp;
                fe.ftimeLo = ents[k].ftlo; fe.ftimeHi = ents[k].fthi;
                fe.hasTime = 1;
                fe.crc = ents[k].crc;
                fe.attr = ents[k].attr;
                fe.dataOff = 0;
                arc.files.push_back(fe);
            }
        }
    } else {
    off = startOff + 36;
    /* file#1 abbreviated entry (12B) comes AFTER the (cnt-1) full
       entries; sizes live in header. So parse full entries first. */
    {
        /* names are needed for entries; parse names first by
           scanning: entries + 12B footer, then pool. */
        unsigned long eoff = off;
        std::vector<std::string> names;
        std::vector<GcaRawEnt> ents;
        unsigned long f0lo = 0, f0hi = 0;
        unsigned long p;
        int k;
        if (cnt == 1) {
            /* single-file shape: [32][FILETIME][len][name][0][CRC] */
            unsigned long ln;
            if (eoff + 13 > startOff + hdr)
                return GCA_PARSE_ERROR;
            /* [+0]==32 marker tolerated */
            f0lo = rd32(base + eoff + 4);
            f0hi = rd32(base + eoff + 8);
            ln = base[eoff + 12];
            if (eoff + 13 + ln + 8 > startOff + hdr)
                return GCA_PARSE_ERROR;
            names.push_back(std::string((const char*)(base + eoff + 13), ln));
            p = eoff + 13 + ln + 8;
            /* p should equal hdr end */
        } else {
            /* multi-file shape */
            int k2;
            for (k2 = 0; k2 < (int)cnt - 1; k2++) {
                GcaRawEnt e;
                if (eoff + 32 > startOff + hdr)
                    return GCA_PARSE_ERROR;
                e.ftlo = rd32(base + eoff + 4);
                e.fthi = rd32(base + eoff + 8);
                e.attr = rd32(base + eoff + 12);
                e.crc = rd32(base + eoff + 16);
                e.comp = rd32(base + eoff + 24);
                e.exp = rd32(base + eoff + 28);
                ents.push_back(e);
                eoff += 32;
            }
            /* 12B footer: file#1 time */
            if (eoff + 12 <= startOff + hdr) {
                f0lo = rd32(base + eoff + 4);
                f0hi = rd32(base + eoff + 8);
            }
            eoff += 12;
            /* names */
            p = eoff;
            for (k = 0; k < (int)cnt; k++) {
                unsigned ln;
                if (p >= startOff + hdr)
                    return GCA_PARSE_ERROR;
                ln = base[p++];
                if (p + ln > startOff + hdr)
                    return GCA_PARSE_ERROR;
                names.push_back(std::string((const char*)(base + p), ln));
                p += ln;
            }
            /* 8B trailer, then payloads at hdr */
        }
            /* file#1 */
            {
                GcaFileEntry fe;
                fe.name = names[0];
                fe.expSize = e0;
                fe.compSize = c0;
                fe.ftimeLo = f0lo; fe.ftimeHi = f0hi; fe.hasTime = 1;
                /* header[+20] is file#1's content CRC32 (verified
                   vs corpus: zeros/binary). No per-entry slot. */
                fe.crc = crc;
                fe.attr = 0;
                fe.dataOff = 0; /* filled by caller (sequential layout) */
                arc.files.push_back(fe);
            }
            for (k = 0; k < (int)ents.size(); k++) {
                GcaFileEntry fe;
                fe.name = names[k + 1];
                fe.expSize = ents[k].exp;
                fe.compSize = ents[k].comp;
                fe.ftimeLo = ents[k].ftlo; fe.ftimeHi = ents[k].fthi;
                fe.hasTime = 1;
                fe.crc = ents[k].crc;
                fe.attr = ents[k].attr;
                fe.dataOff = 0;
                arc.files.push_back(fe);
            }
        }
    }
    /* payload offsets: sequential from hdr in pool order */
    {
        unsigned long po = startOff + hdr;
        unsigned i;
        for (i = 0; i < arc.files.size(); i++) {
            arc.files[i].dataOff = po;
            po += arc.files[i].compSize;
            if (po > len)
                return GCA_PARSE_ERROR;
        }
    }
    return 0;
}

/* GCF1 core: RLE(rsp) -> BWT -> LZ(lsp) -> out(ex bytes).
   resetHist: clear LZ history first (non-solid per-file state). */
static int decodeGcf1Core(const unsigned char* rsp, unsigned long rleLen,
                          const unsigned char* lsp, unsigned long lzLen,
                          unsigned long ex, unsigned long rleOut,
                          unsigned long prim, int resetHist,
                          GcaWork& w, std::string& out)
{
    GcaStages st;
    GcaMemStream ms;
    GcaMemStream ms2;
    unsigned long* tmp;
    unsigned long hi;
    st.pt = &w.pt;
    /* LZ history: fresh per file (encoder-side state);
       solid archives share it across files. -1 = empty. */
    if (resetHist && w.hist) {
        for (hi = 0; hi < 65536; hi++)
            w.hist[hi] = 0xFFFFFFFFUL;
    }
    /* stage 1: RLE -> bufA (rleOut bytes) */
    if (rleOut > w.bufASize) {
        delete [] w.bufA;
        w.bufA = new unsigned char[rleOut ? rleOut : 1];
        w.bufASize = rleOut;
        if (!w.bufA) return -1;
    }
    if (rleOut > w.bufBSize) {
        delete [] w.bufB;
        w.bufB = new unsigned char[rleOut ? rleOut : 1];
        w.bufBSize = rleOut;
        if (!w.bufB) return -1;
    }
    ms.open((unsigned char*)rsp, rleLen);
    if (st.runLengthDecode(w.bufA, &ms, rleOut) != 1)
        return -1;
    /* stage 2: BWT(bufB, bufA, rleOut, prim) */
    if (w.btwTmpSize < rleOut) {
        delete [] w.btwTmp;
        w.btwTmp = new unsigned long[rleOut ? rleOut : 1];
        w.btwTmpSize = rleOut;
        if (!w.btwTmp) return -1;
    }
    tmp = w.btwTmp;
    if (!gcaBlockSortDecode(w.bufB, w.bufA, (int)rleOut, (int)prim, tmp))
        return -1;
    /* stage 3: LZ(final, literals=bufB, LZ stream, ex) */
    ms2.open((unsigned char*)lsp, lzLen);
    out.resize(ex ? ex : 1);
    if (st.iterationDecode((unsigned char*)&out[0], w.bufB, &ms2, (int)ex, w.hist) != 1)
        return -1;
    out.resize(ex);
    return 0;
}

/* Decode one framed/raw payload. Returns 0 ok, nonzero fail. */
static int decodePayload(const unsigned char* p, unsigned long compLen,
                         unsigned long expLen, int solid,
                         GcaWork& w, std::string& out)
{
    out.erase();
    if (expLen == 0)
        return 0;
    if (compLen >= 4 && memcmp(p, "GCF0", 4) == 0) {
        unsigned long n;
        if (compLen < 8)
            return -1;
        n = rd32(p + 4);
        if (8 + n > compLen)
            return -1;
        out.assign((const char*)(p + 8), n);
        return 0;
    }
    if (compLen >= 4 && memcmp(p, "GCF1", 4) == 0) {
        /* [exp][rleOut][prim][rleLen][lzLen] + RLE[rleLen] + LZ[lzLen] */
        unsigned long ex, rleOut, prim, rleLen, lzLen;
        if (compLen < 24)
            return -1;
        ex = rd32(p + 4); rleOut = rd32(p + 8); prim = rd32(p + 12);
        rleLen = rd32(p + 16); lzLen = rd32(p + 20);
        if (solid)
            expLen = ex; /* concat total, not file#1's size */
        else if (ex != expLen)
            return -1;
        if (24 + rleLen + lzLen != compLen)
            return -1;
        if (decodeGcf1Core(p + 24, rleLen, p + 24 + rleLen, lzLen,
                           ex, rleOut, prim, !solid, w, out) != 0)
            return -1;
        if (out.size() != expLen)
            return -1;
        return 0;
    }
    if (compLen >= 4 && memcmp(p, "GCF2", 4) == 0) {
        /* [F0=total][F1=varOut][prim][s1len][s2len][mode]
           + VAR[s1len] + LZ[s2len]. Solid/analyze path. */
        unsigned long F0, F1, prim, s1len, s2len, mode;
        const unsigned char* vsp;
        const unsigned char* lsp;
        if (compLen < 28)
            return -1;
        F0 = rd32(p + 4); F1 = rd32(p + 8); prim = rd32(p + 12);
        s1len = rd32(p + 16); s2len = rd32(p + 20); mode = rd32(p + 24);
        if (solid)
            expLen = F0; /* concat total */
        else if (F0 != expLen)
            return -1;
        if (28 + s1len + s2len != compLen)
            return -1;
        vsp = p + 28;
        lsp = p + 28 + s1len;
        {
            GcaStages st;
            GcaMemStream ms;
            GcaMemStream ms2;
            unsigned long* tmp;
            unsigned long hi;
            unsigned long xe;
            st.pt = &w.pt;
            if (!solid && w.hist) {
                for (hi = 0; hi < 65536; hi++)
                    w.hist[hi] = 0xFFFFFFFFUL;
            }
            if (F1 > w.bufASize) {
                delete [] w.bufA;
                w.bufA = new unsigned char[F1 ? F1 : 1];
                w.bufASize = F1;
                if (!w.bufA) return -1;
            }
            if (F1 > w.bufBSize) {
                delete [] w.bufB;
                w.bufB = new unsigned char[F1 ? F1 : 1];
                w.bufBSize = F1;
                if (!w.bufB) return -1;
            }
            /* stage 1: VariableDecode -> bufA (F1 bytes) */
            ms.open((unsigned char*)vsp, s1len);
            if (st.variableDecode(w.bufA, &ms, F1, mode) != 1)
                return -1;
            /* stage 2: BWT(bufB, bufA, F1, prim) */
            if (w.btwTmpSize < F1) {
                delete [] w.btwTmp;
                w.btwTmp = new unsigned long[F1 ? F1 : 1];
                w.btwTmpSize = F1;
                if (!w.btwTmp) return -1;
            }
            tmp = w.btwTmp;
            if (!gcaBlockSortDecode(w.bufB, w.bufA, (int)F1, (int)prim, tmp))
                return -1;
            /* rolling XOR when mode bit31 set */
            if (mode & 0x80000000UL) {
                for (xe = 0; xe < F1; ) {
                    w.bufB[xe] ^= (unsigned char)(xe & 0xFF);
                    xe += 0x3FF;
                }
            }
            /* stage 3: LZ(final, literals=bufB, LZ stream, F0) */
            ms2.open((unsigned char*)lsp, s2len);
            out.resize(F0 ? F0 : 1);
            if (st.iterationDecode((unsigned char*)&out[0], w.bufB, &ms2, (int)F0, w.hist) != 1)
                return -1;
            out.resize(F0);
        }
        if (out.size() != expLen)
            return -1;
        return 0;
    }
    /* raw fallback (noncompress mode) */
    if (compLen < expLen)
        return -1;
    out.assign((const char*)p, expLen);
    return 0;
}

/* Frame extent for the chunked payload codecs. Returns 0 if p is not a
   recognised frame. *expOut = bytes this frame expands to. */
static unsigned long gcaFrameExtent(const unsigned char* p, unsigned long avail,
                                    unsigned long* expOut)
{
    unsigned long n;
    if (avail < 8) return 0;
    if (memcmp(p, "GCF0", 4) == 0) {
        n = rd32(p + 4);
        *expOut = n;
        return 8 + n;
    }
    if (avail >= 24 && memcmp(p, "GCF1", 4) == 0) {
        *expOut = rd32(p + 4);
        return 24 + rd32(p + 16) + rd32(p + 20);
    }
    if (avail >= 28 && memcmp(p, "GCF2", 4) == 0) {
        *expOut = rd32(p + 4);
        return 28 + rd32(p + 16) + rd32(p + 20);
    }
    return 0;
}

int gcaDecodeFile(const unsigned char* arcBase, const GcaFileEntry& e,
                   int solid, GcaWork& w, std::string& out)
{
    const unsigned char* p = arcBase + e.dataOff;
    unsigned long left = e.compSize;
    unsigned long got = 0;
    out.erase();
    if (e.expSize == 0)
        return 0;
    /* Large members are stored as several consecutive frames (the encoder
       splits at ~1 MiB); concatenate until expSize bytes are produced.
       A solid archive is a single GCF2 frame holding the whole concat. */
    while (left > 0) {
        unsigned long fexp = 0, flen;
        std::string part;
        flen = gcaFrameExtent(p, left, &fexp);
        if (flen == 0) {
            /* No frame magic: a "non compress" member is stored raw, so
               it occupies exactly its expanded size. */
            if (solid)
                return -1;
            fexp = e.expSize - got;
            flen = fexp;
            if (flen == 0)
                break;
        }
        if (flen > left)
            return -1;
        if (fexp > 0) {
            if (decodePayload(p, flen, fexp, solid, w, part) != 0)
                return -1;
            if (!solid && got + (unsigned long)part.size() > e.expSize)
                return -1;
            out += part;
            got += (unsigned long)part.size();
        }
        p += flen;
        left -= flen;
        if (!solid && got >= e.expSize)
            break;
    }
    return (solid || got == e.expSize) ? 0 : -1;
}
