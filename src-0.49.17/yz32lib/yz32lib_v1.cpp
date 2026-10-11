//////////////////////////////////////////////////////////////////////////
// yz32lib.cpp -- reconstructed YZ32 decoding engine (VC6 /MT).
//
// YZ1 bitstream decoder ported from the MIT-licensed Free Pascal
// reverse engineering of Yz1.dll:
//   https://github.com/Xelitan/Yamazaki-YZ1-unpacker-in-Free-Pascal
// (UnYZ1.pas, MIT License). Exposes the yz32lib 1.05 compatible API
// (yzFileEv / yzFileDecode) used by XacRett's Yz1Tool.
//
// Limitation: password-protected archives are not supported.
//////////////////////////////////////////////////////////////////////////

#include "yz32lib_v1.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//--------------------------------------------------------------------
// Bit reader (Pascal TBitReader)
//--------------------------------------------------------------------
struct BitReader {
	const unsigned char* data;
	long dataLen;
	long bitPos;
	int primed;
	unsigned long lookAhead;
};

static unsigned long RdPeek32(BitReader* r, long bitPos)
{
	unsigned long v = 0;
	for (int i = 0; i < 32; i++) {
		long p = bitPos + i;
		unsigned long bit = 0;
		long idx = p >> 3;
		if (idx < r->dataLen)
			bit = (r->data[(unsigned long)idx] >> (7 - (p & 7))) & 1u;
		v = (v << 1) | bit;
	}
	return v;
}

// Returns 0 on success, -1 on prime error.
static int RdConsume(BitReader* r, int n, unsigned long* out)
{
	if (!r->primed) {
		if (n != 32) return -1;
		r->primed = 1;
		r->bitPos = 0;
	} else {
		r->bitPos += n;
	}
	r->lookAhead = RdPeek32(r, r->bitPos);
	*out = r->lookAhead;
	return 0;
}

//--------------------------------------------------------------------
// Huffman tables
//--------------------------------------------------------------------
struct WorkNode {
	unsigned long weight;
	unsigned short symbol;
	unsigned short next; // 0xFFFF = nil
};

struct CodeNode {
	unsigned long code;   // left-aligned in 32 bits
	unsigned short bits;
	unsigned short symbol;
};

struct HuffTable {
	long count;
	unsigned long* codes;
	unsigned short* bits;
	CodeNode* nodes;
	unsigned short lookup12[4096];
};

static void HuffFree(HuffTable* h)
{
	if (h->codes) delete [] h->codes;
	if (h->bits) delete [] h->bits;
	if (h->nodes) delete [] h->nodes;
	h->codes = 0; h->bits = 0; h->nodes = 0; h->count = 0;
}

static int WorkLess(const WorkNode* a, const WorkNode* b)
{
	if (a->weight != b->weight) return a->weight < b->weight;
	return a->symbol < b->symbol;
}

static void SortWork(WorkNode* a, long n)
{
	for (long i = 1; i < n; i++) {
		WorkNode t = a[i];
		long j = i - 1;
		while (j >= 0 && WorkLess(&t, &a[j])) { a[j + 1] = a[j]; j--; }
		a[j + 1] = t;
	}
}

static int CodeLess(const CodeNode* a, const CodeNode* b)
{
	if (a->code != b->code) return a->code < b->code;
	unsigned long pa = ((unsigned long)a->symbol << 16) | a->bits;
	unsigned long pb = ((unsigned long)b->symbol << 16) | b->bits;
	return pa < pb;
}

static void SortCodes(CodeNode* a, long n)
{
	for (long i = 1; i < n; i++) {
		CodeNode t = a[i];
		long j = i - 1;
		while (j >= 0 && CodeLess(&t, &a[j])) { a[j + 1] = a[j]; j--; }
		a[j + 1] = t;
	}
}

static void AddChain(HuffTable* h, WorkNode* w, long idx, int oneBit)
{
	long c = idx;
	while (c != 0xFFFF) {
		long s = w[c].symbol;
		h->bits[s]++;
		h->codes[s] >>= 1;
		if (oneBit) h->codes[s] |= 0x80000000u;
		c = w[c].next;
	}
}

// Returns 0 on success, -1 on alloc failure.
static int NormalizeTable(HuffTable* h, const unsigned char* raw, long n)
{
	h->count = n;
	h->codes = new unsigned long[n];
	h->bits = new unsigned short[n];
	h->nodes = new CodeNode[n];
	WorkNode* w = new WorkNode[n];
	if (!h->codes || !h->bits || !h->nodes || !w) {
		HuffFree(h); if (w) delete [] w; return -1;
	}
	long i;
	for (i = 0; i < n; i++) {
		h->codes[i] = 0; h->bits[i] = 0;
		w[i].weight = raw[i]; w[i].symbol = (unsigned short)i;
		w[i].next = 0xFFFF;
	}
	SortWork(w, n);
	for (long bp = n - 2; bp >= 0; bp--) {
		AddChain(h, w, bp, 0);
		AddChain(h, w, bp + 1, 1);
		w[bp].weight--;
		long a = w[bp].next;
		long last = bp;
		while (a != 0xFFFF) { last = a; a = w[a].next; }
		w[last].next = (unsigned short)(bp + 1);
		WorkNode saved = w[bp];
		long j = bp - 1;
		while (j >= 0 && saved.weight <= w[j].weight) { w[j + 1] = w[j]; j--; }
		w[j + 1] = saved;
	}
	for (i = 0; i < n; i++) {
		h->nodes[i].code = h->codes[i];
		h->nodes[i].bits = h->bits[i];
		h->nodes[i].symbol = (unsigned short)i;
	}
	SortCodes(h->nodes, n);
	for (i = 0; i < 4096; i++) h->lookup12[i] = 0xFFFF;
	for (i = n - 1; i >= 1; i--)
		h->lookup12[(h->nodes[i].code >> 20) & 0xFFF] = (unsigned short)i;
	h->lookup12[0] = 0;
	for (i = 1; i < 4096; i++)
		if (h->lookup12[i] == 0xFFFF) h->lookup12[i] = h->lookup12[i - 1];
	delete [] w;
	return 0;
}

// Returns 0 on success, -1 on failure.
static int DecodeMainSym(BitReader* r, const HuffTable* h, unsigned short* out)
{
	unsigned long a;
	if (RdConsume(r, 0, &a)) return -1;
	long idx = h->lookup12[(a >> 20) & 0xFFF];
	if (idx < 0 || idx >= h->count) return -1;
	unsigned short bits = h->nodes[idx].bits;
	if (bits < 12) {
		if (RdConsume(r, bits, &a)) return -1;
		*out = h->nodes[idx].symbol;
		return 0;
	}
	while (idx < h->count) {
		bits = h->nodes[idx].bits;
		if (bits == 0) return -1;
		unsigned long ma = a >> (32 - bits);
		unsigned long mc = h->nodes[idx].code >> (32 - bits);
		if (ma == mc) {
			if (RdConsume(r, bits, &a)) return -1;
			*out = h->nodes[idx].symbol;
			return 0;
		}
		idx++;
	}
	return -1;
}

// Returns 0 on success, -1 on failure.
static int DecodeExtra(BitReader* r, const HuffTable* h, long* out)
{
	unsigned long a;
	if (RdConsume(r, 0, &a)) return -1;
	for (long i = 0; i <= 0x7E; i++) {
		unsigned short b = h->bits[i];
		if (b != 0 && (a >> (32 - b)) == (h->codes[i] >> (32 - b))) {
			if (RdConsume(r, b, &a)) return -1;
			*out = i;
			return 0;
		}
	}
	unsigned short b = h->bits[0x7F];
	if (b == 0 || (a >> (32 - b)) != (h->codes[0x7F] >> (32 - b))) return -1;
	if (RdConsume(r, b, &a)) return -1;
	if (RdConsume(r, 0, &a)) return -1;
	long nn = (long)(a >> 27) + 1;
	if (RdConsume(r, 5, &a)) return -1;
	if (RdConsume(r, 0, &a)) return -1;
	*out = (long)(a >> (32 - nn));
	if (RdConsume(r, (int)nn, &a)) return -1;
	return 0;
}

static int ReadTables(BitReader* r, HuffTable* mainT, HuffTable* extraT)
{
	unsigned long a;
	if (RdConsume(r, 32, &a)) return -1;
	unsigned char* mraw = new unsigned char[0x300];
	unsigned char* eraw = new unsigned char[0x80];
	if (!mraw || !eraw) {
		if (mraw) delete [] mraw; if (eraw) delete [] eraw; return -1;
	}
	int i;
	for (i = 0; i < 0x300; i++) {
		mraw[i] = (unsigned char)(a >> 27);
		if (RdConsume(r, 5, &a)) { delete [] mraw; delete [] eraw; return -1; }
	}
	for (i = 0; i < 0x80; i++) {
		eraw[i] = (unsigned char)(a >> 27);
		if (RdConsume(r, 5, &a)) { delete [] mraw; delete [] eraw; return -1; }
	}
	memset(mainT, 0, sizeof(*mainT));
	memset(extraT, 0, sizeof(*extraT));
	int rc = NormalizeTable(mainT, mraw, 0x300);
	if (rc == 0) rc = NormalizeTable(extraT, eraw, 0x80);
	delete [] mraw; delete [] eraw;
	if (rc) { HuffFree(mainT); HuffFree(extraT); return -1; }
	return 0;
}

// Decode one LZ block. Returns 0 on success.
static int DecodeBlock(BitReader* r, const HuffTable* mainT,
	const HuffTable* extraT, unsigned long blockSize, unsigned char* blk)
{
	if (blockSize > 0x7FFFFFFFu) return -1;
	if (blockSize) memset(blk, 0, blockSize);
	long* posTab = new long[256 * 256];
	long* lenTab = new long[256 * 256];
	if (!posTab || !lenTab) {
		if (posTab) delete [] posTab; if (lenTab) delete [] lenTab; return -1;
	}
	memset(posTab, 0, sizeof(long) * 256 * 256);
	memset(lenTab, 0, sizeof(long) * 256 * 256);
	unsigned short cur[256];
	memset(cur, 0, sizeof(cur));

	long outPos = 0, lastUpd = 0;
	long idle = 0; // guard against zero-bit stall
	while (outPos < (long)blockSize) {
		long bitBefore = r->bitPos;
		long outBefore = outPos;
		unsigned short sym = 0;
		if (DecodeMainSym(r, mainT, &sym)) {
			delete [] posTab; delete [] lenTab; return -1;
		}
		long len = 1, src = -1;
		if (sym >= 2 * 256) {
			if (outPos >= (long)blockSize) {
				delete [] posTab; delete [] lenTab; return -1;
			}
			blk[outPos++] = (unsigned char)(sym - 2 * 256);
		} else if (sym < 256) {
			long ev = 0;
			if (DecodeExtra(r, extraT, &ev)) {
				delete [] posTab; delete [] lenTab; return -1;
			}
			len = ev + 2;
			long ctx = blk[lastUpd];
			long idx = (cur[ctx] + sym) % 256;
			src = posTab[ctx * 256 + idx];
			if (src < 0 || src >= (long)blockSize ||
				outPos + len > (long)blockSize) {
				delete [] posTab; delete [] lenTab; return -1;
			}
			// NOTE: forward byte-by-byte copy (NOT memmove): LZ matches
			// may overlap the output position (run patterns), in which
			// case freshly written bytes feed back into the copy.
			for (long k = 0; k < len; k++) blk[outPos + k] = blk[src + k];
			outPos += len;
		} else {
			long ctx = blk[lastUpd];
			long idx = (cur[ctx] + sym) % 256;
			len = lenTab[ctx * 256 + idx];
			src = posTab[ctx * 256 + ((cur[ctx] + sym - 256) % 256)];
			long oldP = posTab[ctx * 256 + cur[ctx]];
			long oldL = lenTab[ctx * 256 + cur[ctx]];
			if (oldP != 0) {
				posTab[ctx * 256 + idx] = oldP;
				lenTab[ctx * 256 + idx] = oldL;
			}
			if (src < 0 || src >= (long)blockSize) {
				delete [] posTab; delete [] lenTab; return -1;
			}
			if (len > 0) {
				if (outPos + len > (long)blockSize) {
					delete [] posTab; delete [] lenTab; return -1;
				}
				// Forward copy with feedback (see above).
				for (long k = 0; k < len; k++) blk[outPos + k] = blk[src + k];
				outPos += len;
			}
		}
		if (lastUpd < outPos - 1) {
			long ctx = blk[lastUpd];
			long idx = cur[ctx];
			posTab[ctx * 256 + idx] = lastUpd + 1;
			lenTab[ctx * 256 + idx] = len;
			cur[ctx] = (unsigned short)((cur[ctx] + 1) % 256);
			lastUpd = outPos - 1;
		}
		if (r->bitPos == bitBefore && outPos == outBefore) {
			if (++idle > 100000) {
				delete [] posTab; delete [] lenTab; return -1;
			}
		} else idle = 0;
	}
	delete [] posTab; delete [] lenTab;
	return 0;
}

static unsigned long BE32(const unsigned char* p)
{
	return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
	       ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

//--------------------------------------------------------------------
// Archive decode to memory
//--------------------------------------------------------------------
struct YZEntry {
	char* name;              // NUL-terminated (Shift-JIS, may hold '\')
	unsigned long size;
	unsigned long dosTime;
	unsigned long dataOff;   // offset into decoded image
};

static void FreeEntries(YZEntry* e, long n)
{
	if (e) {
		for (long i = 0; i < n; i++) if (e[i].name) delete [] e[i].name;
		delete [] e;
	}
}

// Decodes archive image (starting at yz01) into entries.
// Returns 0 on success. Caller frees entries + image.
static int YZDecodeImage(const unsigned char* arc, long arcLen,
	YZEntry** outEntries, long* outCount,
	unsigned char** outImage, unsigned long* outTotal)
{
	*outEntries = 0; *outCount = 0; *outImage = 0; *outTotal = 0;
	if (arcLen < 20) return -1;
	if (arc[0] != 'y' || arc[1] != 'z' || arc[2] != '0' || arc[3] != '1')
		return -1;
	unsigned long total = BE32(arc + 8);
	unsigned long metaSize = BE32(arc + 12);
	unsigned long fileCount = BE32(arc + 16);
	if (metaSize > total || fileCount > 65535) return -1;
	if ((unsigned long)arcLen < 20) return -1;

	BitReader r;
	r.data = arc + 20; r.dataLen = arcLen - 20;
	r.bitPos = 0; r.primed = 0; r.lookAhead = 0;

	HuffTable mainT, extraT;
	memset(&mainT, 0, sizeof(mainT)); memset(&extraT, 0, sizeof(extraT));
	if (ReadTables(&r, &mainT, &extraT)) return -1;

	unsigned char* meta = 0;
	if (metaSize) {
		meta = new unsigned char[metaSize];
		if (!meta) { HuffFree(&mainT); HuffFree(&extraT); return -1; }
	}
	int rc = -1;
	if (metaSize == 0 || DecodeBlock(&r, &mainT, &extraT, metaSize, meta) == 0) {
		if ((unsigned long)fileCount * 8 > metaSize) goto done;
		unsigned long* sizes = 0;
		if (fileCount) {
			sizes = new unsigned long[fileCount];
			if (!sizes) goto done;
		}
		unsigned long sum = 0, mx = 0;
		unsigned long k;
		for (k = 0; k < fileCount; k++) {
			sizes[k] = BE32(meta + k * 4);
			sum += sizes[k];
			if (sizes[k] > mx) mx = sizes[k];
		}
		if (metaSize + sum != total) { if (sizes) delete [] sizes; goto done; }
		unsigned char* img = new unsigned char[total ? total : 1];
		if (!img) { if (sizes) delete [] sizes; goto done; }
		if (metaSize) memcpy(img, meta, metaSize);
		unsigned long dpos = metaSize;
		unsigned long fi = 0;
		rc = 0;
		while (fi < fileCount) {
			unsigned long gstart = fi, gsize = 0;
			while (fi < fileCount && (gsize + sizes[fi] <= mx || gsize == 0)) {
				gsize += sizes[fi]; fi++;
			}
			if (gsize > 0) {
				unsigned char* blk = new unsigned char[gsize];
				if (!blk) { rc = -1; delete [] img; delete [] sizes; break; }
				if (DecodeBlock(&r, &mainT, &extraT, gsize, blk)) {
					delete [] blk; delete [] img; delete [] sizes; rc = -1; break;
				}
				memcpy(img + dpos, blk, gsize);
				delete [] blk;
				dpos += gsize;
			} else if (fi == gstart) fi++; // zero-length group
		}
		if (rc == 0) {
			YZEntry* en = new YZEntry[fileCount ? fileCount : 1];
			if (!en) { delete [] img; delete [] sizes; rc = -1; }
			else {
				memset(en, 0, sizeof(YZEntry) * (fileCount ? fileCount : 1));
				long npos = (long)(fileCount * 8);
				unsigned long i;
				rc = 0;
				for (i = 0; i < fileCount; i++) {
					en[i].size = sizes[i];
					en[i].dosTime = BE32(meta + fileCount * 4 + i * 4);
				}
				for (i = 0; i < fileCount && rc == 0; i++) {
					long st = npos;
					while (npos < (long)metaSize && meta[npos] != 0) npos++;
					if (npos >= (long)metaSize) rc = -1;
					else {
						long L = npos - st;
						en[i].name = new char[L + 1];
						if (!en[i].name) rc = -1;
						else {
							if (L) memcpy(en[i].name, meta + st, L);
							en[i].name[L] = 0;
						}
						npos++;
					}
				}
				unsigned long dp = metaSize;
				for (i = 0; i < fileCount && rc == 0; i++) {
					if (dp + en[i].size > total) rc = -1;
					else { en[i].dataOff = dp; dp += en[i].size; }
				}
				if (rc == 0) {
					*outEntries = en; *outCount = (long)fileCount;
					*outImage = img; *outTotal = total;
					img = 0; en = 0;
				}
				if (en) FreeEntries(en, (long)fileCount);
				if (img) delete [] img;
			}
		}
		if (sizes) delete [] sizes;
	}
done:
	if (meta) delete [] meta;
	HuffFree(&mainT); HuffFree(&extraT);
	return rc;
}

//--------------------------------------------------------------------
// Output helpers
//--------------------------------------------------------------------
static void ToOSSeparators(char* s)
{
	for (; *s; s = (char*)CharNextA(s)) {
		if (*s == '/') *s = '\\';
	}
}

// Removes ".." and leading '**\' / ':' (mirrors Pascal CleanRelativeName).
static void CleanRelName(char* t)
{
	ToOSSeparators(t);
	char* p;
	while ((p = strstr(t, "..")) != 0) memmove(p, p + 2, strlen(p + 2) + 1);
	while (*t == '\\' || *t == ':') memmove(t, t + 1, strlen(t + 1) + 1);
}

static int MakePathForFile(const char* path)
{
	char tmp[MAX_PATH];
	strncpy(tmp, path, MAX_PATH - 1); tmp[MAX_PATH - 1] = 0;
	char* s = tmp;
	if (s[0] && s[1] == ':') s += 2;
	if (s[0] == '\\' && s[1] == '\\') {
		// UNC: skip server\share
		s += 2;
		char* a = strchr(s, '\\'); if (!a) return 0;
		a = strchr(a + 1, '\\'); if (!a) return 0;
		s = a;
	}
	for (char* p = s + 1; *p; p++) {
		if (*p == '\\') {
			*p = 0;
			CreateDirectoryA(tmp, NULL);
			*p = '\\';
		}
	}
	return 0;
}

static void SetDosTime(const char* path, unsigned long dos)
{
	unsigned short date = (unsigned short)(dos >> 16);
	unsigned short time = (unsigned short)(dos & 0xFFFF);
	FILETIME ft;
	if (!DosDateTimeToFileTime(date, time, &ft)) return;
	FILETIME utc;
	if (!LocalFileTimeToFileTime(&ft, &utc)) utc = ft;
	HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h != INVALID_HANDLE_VALUE) {
		SetFileTime(h, NULL, NULL, &utc);
		CloseHandle(h);
	}
}

//--------------------------------------------------------------------
// Time / formatting helpers
//--------------------------------------------------------------------
static time_t DosToTimeT(unsigned long dos)
{
	FILETIME ft;
	SYSTEMTIME st;
	struct tm t;
	if (!dos) return (time_t)0;
	if (!DosDateTimeToFileTime((WORD)(dos >> 16), (WORD)(dos & 0xFFFF), &ft))
		return (time_t)0;
	if (!FileTimeToSystemTime(&ft, &st)) return (time_t)0;
	memset(&t, 0, sizeof(t));
	t.tm_sec   = st.wSecond;
	t.tm_min   = st.wMinute;
	t.tm_hour  = st.wHour;
	t.tm_mday  = st.wDay;
	t.tm_mon   = st.wMonth - 1;
	t.tm_year  = st.wYear - 1900;
	t.tm_isdst = -1;
	return mktime(&t);
}

// "%d", "%d,%03d", "%d,%03d,%03d" or "%d,%03d,%03d,%03d"
static void FormatSize(char* buf, unsigned long n)
{
	if (n < 1000) _snprintf(buf, 16, "%d", (int)n);
	else if (n < 1000000UL)
		_snprintf(buf, 16, "%d,%03d", (int)(n / 1000), (int)(n % 1000));
	else if (n < 1000000000UL)
		_snprintf(buf, 16, "%d,%03d,%03d", (int)(n / 1000000),
			(int)((n / 1000) % 1000), (int)(n % 1000));
	else
		_snprintf(buf, 16, "%d,%03d,%03d,%03d", (int)(n / 1000000000UL),
			(int)((n / 1000000UL) % 1000), (int)((n / 1000) % 1000),
			(int)(n % 1000));
	buf[15] = 0;
}

//--------------------------------------------------------------------
// Archive loading (plain or SFX-embedded)
//--------------------------------------------------------------------
static int LoadArchive(const char* fname,
	unsigned char** outFile, const unsigned char** outArc, long* outArcLen)
{
	FILE* fp;
	long flen;
	unsigned char* file;
	const unsigned char* arc;
	if (!fname || !fname[0]) return -1;
	fp = fopen(fname, "rb");
	if (!fp) return -1;
	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (flen < 20) { fclose(fp); return -1; }
	file = new unsigned char[flen];
	if (!file) { fclose(fp); return -1; }
	if ((long)fread(file, 1, flen, fp) != flen) {
		delete [] file; fclose(fp); return -1;
	}
	fclose(fp);

	arc = file;
	if (file[0] == 'y' && file[1] == 'z' && file[2] == '0' && file[3] == '1') {
		*outArcLen = flen;
	} else {
		unsigned long asiz = BE32(file + flen - 4);
		long off;
		if (asiz + 4 >= (unsigned long)flen || asiz < 20) {
			delete [] file; return -1;
		}
		off = flen - (long)(asiz + 4);
		if (!(file[off] == 'y' && file[off + 1] == 'z' &&
		      file[off + 2] == '0' && file[off + 3] == '1')) {
			delete [] file; return -1;
		}
		arc = file + off;
		*outArcLen = (long)asiz;
	}
	*outFile = file;
	*outArc = arc;
	return 0;
}

//--------------------------------------------------------------------
// yzFileStat / yzDecHead / yzDecodeHeader
//--------------------------------------------------------------------
yzFileStat::yzFileStat(string in_file_name, long in_file_size,
                       time_t in_file_mtime)
	: m_file_name(in_file_name), m_file_size(in_file_size),
	  m_file_mtime(in_file_mtime)
{
}

yzDecHead::yzDecHead(yzFileEv* yzmev, const void* entries, long count)
	: m_file_count(count), m_file_size_tbl(0), m_file_mtime_tbl(0),
	  m_file_name_max_length(0), m_file_size_max(0), yzm(yzmev)
{
	const YZEntry* en = (const YZEntry*)entries;
	long i;
	if (count < 0) count = 0;
	if (count) {
		m_file_size_tbl  = new long[count];
		m_file_mtime_tbl = new YZtime_t[count];
	}
	if (!m_file_size_tbl || (count && !m_file_mtime_tbl)) {
		m_file_count = 0;
		return;
	}
	for (i = 0; i < count; i++) {
		const char* nm = (en && en[i].name) ? en[i].name : "";
		int nl = (int)strlen(nm);
		m_file_size_tbl[i]  = (long)en[i].size;
		m_file_mtime_tbl[i] = DosToTimeT(en[i].dosTime);
		m_file_name_tbl.push_back(FileName(nm));
		if (nl > m_file_name_max_length) m_file_name_max_length = nl;
		if ((long)en[i].size > m_file_size_max) m_file_size_max = (long)en[i].size;
	}
}

yzDecHead::yzDecHead(const unsigned char* in_area, const long& in_file_cnt,
                     yzFileEv* yzmev, const long& in_size, bool* success)
	: m_file_count(0), m_file_size_tbl(0), m_file_mtime_tbl(0),
	  m_file_name_max_length(0), m_file_size_max(0), yzm(yzmev)
{
	YZEntry* en = 0;
	unsigned char* img = 0;
	unsigned long total = 0;
	long n = 0;
	if (success) *success = false;
	if (!in_area || in_size < 20) {
		if (success) *success = true;	// nothing to do counts as success
		return;
	}
	if (YZDecodeImage(in_area, in_size, &en, &n, &img, &total)) {
		if (success) *success = true;
		return;
	}
	if (in_file_cnt >= 0 && n != in_file_cnt) {
		FreeEntries(en, n);
		delete [] img;
		if (success) *success = true;
		return;
	}
	{
		yzDecHead tmp(yzmev, en, n);
		m_file_count = tmp.m_file_count;
		m_file_size_tbl = tmp.m_file_size_tbl;
		m_file_mtime_tbl = tmp.m_file_mtime_tbl;
		m_file_name_tbl.swap(tmp.m_file_name_tbl);
		m_file_name_max_length = tmp.m_file_name_max_length;
		m_file_size_max = tmp.m_file_size_max;
		tmp.m_file_count = 0;
		tmp.m_file_size_tbl = 0;
		tmp.m_file_mtime_tbl = 0;
	}
	FreeEntries(en, n);
	delete [] img;
	if (success) *success = true;
}

yzDecHead::~yzDecHead()
{
	if (m_file_size_tbl) delete [] m_file_size_tbl;
	if (m_file_mtime_tbl) delete [] m_file_mtime_tbl;
}

int yzDecHead::FileWrite(const char* in_dir_name, const int& in_n,
                         const unsigned char* in_data)
{
	char base[MAX_PATH];
	char rel[MAX_PATH];
	char out[MAX_PATH * 2];
	FILE* wp;
	long L;
	if (!in_dir_name) return -1;
	if (in_n < 0 || in_n >= m_file_count) return 0;
	if (m_file_size_tbl[in_n] <= 0) return 1;
	if (!in_data) return -1;

	strncpy(base, in_dir_name, MAX_PATH - 1); base[MAX_PATH - 1] = 0;
	L = (long)strlen(base);
	while (L > 0 && (base[L - 1] == '\\' || base[L - 1] == '/')) base[--L] = 0;
	strncpy(rel, m_file_name_tbl[in_n].c_str(), MAX_PATH - 1);
	rel[MAX_PATH - 1] = 0;
	CleanRelName(rel);
	if (!rel[0]) strncpy(rel, "unnamed", MAX_PATH - 1);
	_snprintf(out, sizeof(out) - 1, "%s\\%s", base, rel);
	out[sizeof(out) - 1] = 0;
	MakePathForFile(out);
	wp = fopen(out, "wb");
	if (!wp) return -1;
	if (fwrite(in_data, 1, (size_t)m_file_size_tbl[in_n], wp) !=
	    (size_t)m_file_size_tbl[in_n]) {
		fclose(wp);
		return -1;
	}
	fclose(wp);
	return 1;
}

int yzDecHead::MemChk(const char* in_dir_name, const int& in_n,
                      const unsigned char* in_mem)
{
	return FileWrite(in_dir_name, in_n, in_mem);
}

void yzDecHead::Print()
{
	long i;
	char line[MAX_PATH + 128];
	char sz[24];
	for (i = 0; i < m_file_count; i++) {
		FormatSize(sz, (unsigned long)m_file_size_tbl[i]);
		_snprintf(line, sizeof(line) - 1, "OutFile(%d/%d): %s (%s byte)",
			(int)(i + 1), (int)m_file_count,
			m_file_name_tbl[i].c_str(), sz);
		line[sizeof(line) - 1] = 0;
		if (yzm) yzm->MessINF(line);
	}
}

yzDecodeHeader::yzDecodeHeader(yzDecHead& in_yz_dec_head)
	: m_yz_dec_head(in_yz_dec_head)
{
}

vector<yzFileStat> yzDecodeHeader::filestat_Get()
{
	vector<yzFileStat> v;
	long i;
	for (i = 0; i < m_yz_dec_head.m_file_count; i++) {
		v.push_back(yzFileStat(m_yz_dec_head.m_file_name_tbl[i],
			m_yz_dec_head.m_file_size_tbl[i],
			m_yz_dec_head.m_file_mtime_tbl[i]));
	}
	return v;
}

vector<yzFileStat> yzDecodeHeader::filestat_Get() const
{
	vector<yzFileStat> v;
	long i;
	for (i = 0; i < m_yz_dec_head.m_file_count; i++) {
		v.push_back(yzFileStat(m_yz_dec_head.m_file_name_tbl[i],
			m_yz_dec_head.m_file_size_tbl[i],
			m_yz_dec_head.m_file_mtime_tbl[i]));
	}
	return v;
}

YzMessage::language_type YzMessage::language = YzMessage::language_japanese;

//--------------------------------------------------------------------
// yzFileEncode (decode-only reconstruction)
//--------------------------------------------------------------------
yzFileEncode::yzFileEncode(yzFileEv* in_yzm, const char* in_dir_name,
                           const char* in_self_encode_file,
                           const enum YZ_MAIN_ENCODE_PASSWD_MODE in_passwd_mode)
	: m_output_name(), yzm(in_yzm), dirName(in_dir_name),
	  SelfEncodeFile(in_self_encode_file), passwd_mode(in_passwd_mode)
{
}

int yzFileEncode::EncodeFile(const char* /*in_file_name*/)
{
	return -1;	// encoding is not implemented in this reconstruction
}

//--------------------------------------------------------------------
// yzFileDecode
//--------------------------------------------------------------------
yzFileDecode::yzFileDecode(yzFileEv* ev, const char* outdir)
	: yzfiledecode_output_dir_name(0), yzfiledecode_p_yz_dec_head(0),
	  yzm(ev), m_image(0), m_image_len(0), m_image_count(0)
{
	m_outdir[0] = 0;
	if (outdir) {
		strncpy(m_outdir, outdir, MAX_PATH - 1);
		m_outdir[MAX_PATH - 1] = 0;
	}
	yzfiledecode_output_dir_name = m_outdir;
}

yzFileDecode::~yzFileDecode()
{
	if (yzfiledecode_p_yz_dec_head) {
		delete yzfiledecode_p_yz_dec_head;
		yzfiledecode_p_yz_dec_head = 0;
	}
	if (m_image) { delete [] m_image; m_image = 0; }
	m_image_len = 0;
	m_image_count = 0;
}

int yzFileDecode::YzFile_DecodeHeader(const char* in_file_name)
{
	unsigned char* file = 0;
	const unsigned char* arc = 0;
	long arcLen = 0;
	YZEntry* en = 0;
	unsigned char* img = 0;
	unsigned long total = 0;
	long n = 0;
	if (!in_file_name || !in_file_name[0]) return -1;
	if (LoadArchive(in_file_name, &file, &arc, &arcLen)) return -1;
	if (YZDecodeImage(arc, arcLen, &en, &n, &img, &total)) {
		delete [] file;
		return -1;
	}
	if (yzfiledecode_p_yz_dec_head) {
		delete yzfiledecode_p_yz_dec_head;
		yzfiledecode_p_yz_dec_head = 0;
	}
	yzfiledecode_p_yz_dec_head = new yzDecHead(yzm, en, n);
	FreeEntries(en, n);
	delete [] file;
	return 0;
}

int yzFileDecode::Print(const char* fileName)
{
	char line[MAX_PATH + 128];
	char sz[24];
	long i;
	if (!yzfiledecode_p_yz_dec_head) return -1;
	yzDecHead* h = yzfiledecode_p_yz_dec_head;
	if (!h) return -1;
	if (!fileName || !fileName[0]) { h->Print(); return 0; }
	for (i = 0; i < h->m_file_count; i++) {
		if (h->m_file_name_tbl[i] == fileName) {
			FormatSize(sz, (unsigned long)h->m_file_size_tbl[i]);
			_snprintf(line, sizeof(line) - 1, "TestFile(%d/%d): %s (%s byte)",
				(int)(i + 1), (int)h->m_file_count, fileName, sz);
			line[sizeof(line) - 1] = 0;
			if (yzm) yzm->MessINF(line);
			return 0;
		}
	}
	return -1;
}

int yzFileDecode::DecodeFile(const char* fname, const int& /*mode*/)
{
	unsigned char* file = 0;
	const unsigned char* arc = 0;
	long arcLen = 0;
	YZEntry* en = 0;
	unsigned char* img = 0;
	unsigned long total = 0;
	long n = 0, i;
	char base[MAX_PATH];
	long L;
	int fail = 0;

	if (!fname || !fname[0]) return -1;
	if (LoadArchive(fname, &file, &arc, &arcLen)) return -1;
	if (YZDecodeImage(arc, arcLen, &en, &n, &img, &total)) {
		delete [] file;
		return -1;
	}
	if (yzfiledecode_p_yz_dec_head) {
		delete yzfiledecode_p_yz_dec_head;
	}
	yzfiledecode_p_yz_dec_head = new yzDecHead(yzm, en, n);
	m_image = img; m_image_len = total; m_image_count = n;
	img = 0;	// owned by the object from here on

	if (yzm) yzm->GaugeInit((int)n);
	strncpy(base, m_outdir, MAX_PATH - 1); base[MAX_PATH - 1] = 0;
	L = (long)strlen(base);
	while (L > 0 && (base[L - 1] == '\\' || base[L - 1] == '/')) base[--L] = 0;

	for (i = 0; i < n; i++) {
		char rel[MAX_PATH];
		char out[MAX_PATH * 2];
		FILE* wp;
		const char* nm = (en[i].name && en[i].name[0]) ? en[i].name : "unnamed";
		strncpy(rel, nm, MAX_PATH - 1); rel[MAX_PATH - 1] = 0;
		CleanRelName(rel);
		if (!rel[0]) strncpy(rel, "unnamed", MAX_PATH - 1);
		_snprintf(out, sizeof(out) - 1, "%s\\%s", base, rel);
		out[sizeof(out) - 1] = 0;
		MakePathForFile(out);
		wp = fopen(out, "wb");
		if (!wp) { fail = 1; break; }
		if (en[i].size) {
			if (fwrite(m_image + en[i].dataOff, 1, en[i].size, wp) != en[i].size) {
				fclose(wp); fail = 1; break;
			}
		}
		fclose(wp);
		if (en[i].dosTime) SetDosTime(out, en[i].dosTime);
		if (yzm) yzm->GaugeSetOutputValue((int)(i + 1));
	}

	FreeEntries(en, n);
	delete [] file;
	return fail ? -1 : 0;
}
