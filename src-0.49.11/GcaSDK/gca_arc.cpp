///////////////////////////////////////////////////////////////////
// gca_arc.cpp -- container parse + per-file decode.
///////////////////////////////////////////////////////////////////
#include "gca_arc.h"
#include "gca_bwt.h"
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

int gcaParse(const unsigned char* base, unsigned long len,
             unsigned long startOff, GcaParsed& arc)
{
    const unsigned char* h;
    unsigned long ver, cnt, hdr, flags, crc, c0, e0;
    unsigned long off;
    arc.files.clear();
    if (startOff + 36 > len)
        return -1;
    h = base + startOff;
    if (memcmp(h, "GCA0", 4) != 0 && memcmp(h, "GCA1", 4) != 0)
        return -1;
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
        return -1;
    if (startOff + hdr > len)
        return -1;
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
            return -1;
        if (startOff + 44 > startOff + hdr)
            return -1;
        F0 = rd32(h + 24); F1 = rd32(h + 28); prim = rd32(h + 32);
        rleLen = rd32(base + startOff + 36);
        lzLen = rd32(base + startOff + 40);
        if (F0 != 32 * cnt)
            return -1;
        if (startOff + 44 + rleLen + lzLen > startOff + hdr)
            return -1;
        memset(&hw, 0, sizeof(hw));
        hw.hist = new unsigned long[65536 + 16];
        if (!hw.hist)
            return -1;
        if (decodeGcf1Core(base + startOff + 44, rleLen,
                           base + startOff + 44 + rleLen, lzLen,
                           F0, F1, prim, 1, hw, dec1) != 0) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return -1;
        }
        if (dec1.size() != (size_t)(32 * cnt)) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return -1;
        }
        /* frame2: names pool (framed GCF1) */
        f2 = startOff + 44 + rleLen + lzLen;
        if (f2 + 24 > startOff + hdr ||
            memcmp(base + f2, "GCF1", 4) != 0) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return -1;
        }
        n2ex = rd32(base + f2 + 4); n2ro = rd32(base + f2 + 8);
        n2prim = rd32(base + f2 + 12); n2rl = rd32(base + f2 + 16);
        n2ll = rd32(base + f2 + 20);
        if (f2 + 24 + n2rl + n2ll > startOff + hdr) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return -1;
        }
        if (decodeGcf1Core(base + f2 + 24, n2rl,
                           base + f2 + 24 + n2rl, n2ll,
                           n2ex, n2ro, n2prim, 1, hw, dec2) != 0) {
            delete [] hw.hist; delete [] hw.bufA;
            delete [] hw.bufB; delete [] hw.btwTmp;
            return -1;
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
                    return -1;
                ln = ((const unsigned char*)dec2.data())[p++];
                if (p + ln > dec2.size())
                    return -1;
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
                return -1;
            /* [+0]==32 marker tolerated */
            f0lo = rd32(base + eoff + 4);
            f0hi = rd32(base + eoff + 8);
            ln = base[eoff + 12];
            if (eoff + 13 + ln + 8 > startOff + hdr)
                return -1;
            names.push_back(std::string((const char*)(base + eoff + 13), ln));
            p = eoff + 13 + ln + 8;
            /* p should equal hdr end */
        } else {
            /* multi-file shape */
            int k2;
            for (k2 = 0; k2 < (int)cnt - 1; k2++) {
                GcaRawEnt e;
                if (eoff + 32 > startOff + hdr)
                    return -1;
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
                    return -1;
                ln = base[p++];
                if (p + ln > startOff + hdr)
                    return -1;
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
                return -1;
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

int gcaDecodeFile(const unsigned char* arcBase, const GcaFileEntry& e,
                   int solid, GcaWork& w, std::string& out)
{
    if (e.compSize == 0) {
        out.erase();
        return (e.expSize == 0) ? 0 : -1;
    }
    return decodePayload(arcBase + e.dataOff, e.compSize, e.expSize,
                         solid, w, out);
}
