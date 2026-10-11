//////////////////////////////////////////////////////////////////////////
// yz32lib_v2.cpp -- later-generation yz32lib facade.
//
// Implemented on top of the yz32lib.h decode engine: the codec itself is
// shared (no second implementation).  yzDecHead owns its own copies of the
// entry tables, so it stays valid after the helper yzFileDecode that
// produced it has been destroyed.
//
//////////////////////////////////////////////////////////////////////////
#include "yz32lib.h"
// v1 base: the reconstruction in this file is a bridge, exposing the
// gauge_* API (nsYZ32v2) that XacRett is written against on top of the
// original flat API. The ::yz* names below refer to this global one.
#include "yz32lib_v1.h"

#include <string.h>
#include <stdlib.h>

namespace nsYZ32v2 {

namespace {

//--------------------------------------------------------------------
void ToOSSeparators(char* s)
{
	for (; *s; s = (char*)CharNextA(s)) {
		if (*s == '/') *s = '\\';
	}
}

// Mirrors the engine's name cleanup (".." and leading '\' / ':' removal).
void CleanRelName(char* t)
{
	char* p;
	ToOSSeparators(t);
	while ((p = strstr(t, "..")) != 0)
		memmove(p, p + 2, strlen(p + 2) + 1);
	while (*t == '\\' || *t == ':')
		memmove(t, t + 1, strlen(t + 1) + 1);
}

int MakePathForFile(const char* path)
{
	char tmp[MAX_PATH];
	char* s;
	strncpy(tmp, path, MAX_PATH - 1);
	tmp[MAX_PATH - 1] = 0;
	s = tmp;
	if (s[0] && s[1] == ':') s += 2;
	if (s[0] == '\\' && s[1] == '\\') {
		char* a;
		s += 2;
		a = strchr(s, '\\'); if (!a) return 0;
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

void FormatSize(char* buf, unsigned long n)
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

void SetDosTimeFromTimeT(const char* path, time_t t)
{
	struct tm* lt;
	FILETIME ft, utc;
	if (!t) return;
	lt = localtime(&t);
	if (!lt) return;
	if (!DosDateTimeToFileTime((WORD)(((lt->tm_year - 80) << 9) |
		((lt->tm_mon + 1) << 5) | lt->tm_mday),
		(WORD)((lt->tm_hour << 11) | (lt->tm_min << 5) |
		(lt->tm_sec / 2)), &ft))
		return;
	if (!LocalFileTimeToFileTime(&ft, &utc)) utc = ft;
	{
		HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h != INVALID_HANDLE_VALUE) {
			SetFileTime(h, NULL, NULL, &utc);
			CloseHandle(h);
		}
	}
}

//--------------------------------------------------------------------
class V2Bridge : public ::yzFileEv
{
public:
	nsYZ32v2::yzFileEv*  user ;
public:
	yzDecHead*            head ;
	const char*           arch_name ;
	int                   notified ;
	long                  total ;

public:
	V2Bridge()
		: user(0), head(0), arch_name(0), notified(0), total(0) {}

	void MessEX(char* s)
		{ if (user) user->MessEX(s ? s : "", YZ_MSG_MESS_EX); }
	void MessINF(char* s)
		{ if (user) user->MessINF(s ? s : "", YZ_MSG_MESS_INF); }
	int  MessYN(char* s)
		{ return user ? user->MessYN(s ? s : "", YZ_MSG_MESS_YN) : 0; }
	char* MessINPUT(char* s)
		{ return user ? user->MessINPUT(s ? s : "", YZ_MSG_MESS_INPUT) : 0; }

	void GaugeInit(int n)   { total = n; }
	void GaugeSetText(char* s)
		{ if (user) user->gauge_String(s ? s : ""); }
	int  GaugeSetInputValue(int) { return 0; }

	int  GaugeSetOutputValue(int done)
	{
		if (!user) return 0;
		if (total > 0)
			user->gauge_Value((int)((long)done * 1000 / total), 1000);
		if (head && done >= 1 && done <= (int)head->m_file_count) {
			long i = (long)done - 1;
			yzFileStat st(head->m_file_name_tbl[i],
				head->m_file_size_tbl[i], head->m_file_mtime_tbl[i]);
			user->gauge_WriteInfo(st, done, (int)head->m_file_count);
		}
		return 0;
	}

	void NotifyDecode()
	{
		if (!user || notified || !head) return;
		yzDecodeHeader dh(*head);
		FileName nm(arch_name ? arch_name : "");
		user->gauge_DecodeInit(nm.c_str(), dh);
		notified = 1;
	}
};

// Fills a v2 header from a v1 header (v1 tables are public).
void AdoptHeader(yzDecHead* dst, const ::yzDecHead* src,
                  nsYZ32v2::yzFileEv* ev)
{
	long i;
	dst->m_file_count = src->m_file_count;
	dst->m_file_name_max_length = src->m_file_name_max_length;
	dst->m_file_size_max = src->m_file_size_max;
	dst->m_file_name_tbl = src->m_file_name_tbl;
	if (src->m_file_count > 0) {
		dst->m_file_size_tbl = new long[src->m_file_count];
		dst->m_file_mtime_tbl = new YZtime_t[src->m_file_count];
		for (i = 0; i < src->m_file_count; i++) {
			dst->m_file_size_tbl[i]  = src->m_file_size_tbl[i];
			dst->m_file_mtime_tbl[i] = src->m_file_mtime_tbl[i];
		}
	}
	(void)ev;
}

} // anonymous namespace

//--------------------------------------------------------------------
yzFileStat::yzFileStat(string in_file_name, long in_file_size,
                       time_t in_file_mtime)
	: m_file_name(in_file_name), m_file_size(in_file_size),
	  m_file_mtime(in_file_mtime)
{
}

//--------------------------------------------------------------------
yzDecHead::yzDecHead(yzFileEv* yzmev, const void* src)
	: m_file_count(0), m_file_size_tbl(0), m_file_mtime_tbl(0),
	  m_file_name_max_length(0), m_file_size_max(0), yzm(yzmev)
{
	if (src) AdoptHeader(this, (const ::yzDecHead*)src, yzmev);
}

yzDecHead::yzDecHead(const unsigned char* in_area, const long& in_file_cnt,
                     yzFileEv* yzmev, const long& in_size, bool* success)
	: m_file_count(0), m_file_size_tbl(0), m_file_mtime_tbl(0),
	  m_file_name_max_length(0), m_file_size_max(0), yzm(yzmev)
{
	V2Bridge bridge;
	::yzDecHead* v1;
	bridge.user = yzmev;
	v1 = new ::yzDecHead(in_area, in_file_cnt, &bridge, in_size, success);
	if (!v1) return;
	AdoptHeader(this, v1, yzmev);
	delete v1;	// tables already copied
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
	SetDosTimeFromTimeT(out, m_file_mtime_tbl[in_n]);
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
		if (yzm) yzm->MessINF(line, YZ_MSG_MESS_INF);
	}
}

//--------------------------------------------------------------------
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

//--------------------------------------------------------------------
// YzMessage::language is *defined* by XacRett itself, in Yz1Tool.cpp:69.
// Defining it here as well produced an LNK2005 duplicate, so it is left
// to the caller.

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
	return -1;
}

//--------------------------------------------------------------------
yzFileDecode::yzFileDecode(yzFileEv* in_yzm, const char* in_dir_name)
	: yzfiledecode_output_dir_name(0), yzfiledecode_p_yz_dec_head(0),
	  p_yz_dec_head(0), dec_file_count(0), dec_head_size(0),
	  yzm(in_yzm), m_dir(in_dir_name), m_arc_name(0), m_bridge(new V2Bridge())
{
	((V2Bridge*)m_bridge)->user = in_yzm;
	yzfiledecode_output_dir_name = (char*)in_dir_name;
}

// Legacy three-argument form used by OpenArchive/OpenYz1.cpp.
// InEv carries only the archive path, so this reduces to the form above
// apart from remembering the path for the DecodeFile(int) overload.
yzFileDecode::yzFileDecode(InEv& in, yzFileEv* in_yzm, const char* in_dir_name)
	: yzfiledecode_output_dir_name(0), yzfiledecode_p_yz_dec_head(0),
	  p_yz_dec_head(0), dec_file_count(0), dec_head_size(0),
	  yzm(in_yzm), m_dir(in_dir_name), m_arc_name(0), m_bridge(new V2Bridge())
{
	((V2Bridge*)m_bridge)->user = in_yzm;
	yzfiledecode_output_dir_name = (char*)in_dir_name;
	m_arc_name = in.file_name();
}

yzFileDecode::~yzFileDecode()
{
	if (yzfiledecode_p_yz_dec_head) {
		delete yzfiledecode_p_yz_dec_head;
		yzfiledecode_p_yz_dec_head = 0;
		p_yz_dec_head = 0;
	}
	if (m_bridge) {
		delete (V2Bridge*)m_bridge;
		m_bridge = 0;
	}
}

int yzFileDecode::YzFile_DecodeHeader(const char* in_file_name)
{
	V2Bridge* b = (V2Bridge*)m_bridge;
	::yzFileDecode hd((::yzFileEv*)m_bridge, m_dir ? m_dir : "");
	if (hd.YzFile_DecodeHeader(in_file_name)) return -1;
	if (yzfiledecode_p_yz_dec_head) {
		delete yzfiledecode_p_yz_dec_head;
		yzfiledecode_p_yz_dec_head = 0;
	}
	yzfiledecode_p_yz_dec_head =
		new yzDecHead(yzm, (const void*)hd.yzfiledecode_p_yz_dec_head);
	b->head = yzfiledecode_p_yz_dec_head;
	b->arch_name = in_file_name;
	// keep the legacy aliases in step with the canonical members
	p_yz_dec_head  = yzfiledecode_p_yz_dec_head;
	dec_file_count = yzfiledecode_p_yz_dec_head ? yzfiledecode_p_yz_dec_head->m_file_count : 0;
	return 0;
}

int yzFileDecode::Print(const char* fileName)
{
	char line[MAX_PATH + 128];
	char sz[24];
	long i;
	yzDecHead* h = yzfiledecode_p_yz_dec_head;
	if (!h) return -1;
	if (!fileName || !fileName[0]) { h->Print(); return 0; }
	for (i = 0; i < h->m_file_count; i++) {
		if (h->m_file_name_tbl[i] == fileName) {
			FormatSize(sz, (unsigned long)h->m_file_size_tbl[i]);
			_snprintf(line, sizeof(line) - 1, "TestFile(%d/%d): %s (%s byte)",
				(int)(i + 1), (int)h->m_file_count, fileName, sz);
			line[sizeof(line) - 1] = 0;
			if (yzm) yzm->MessINF(line, YZ_MSG_MESS_INF);
			return 0;
		}
	}
	return -1;
}

int yzFileDecode::DecodeFile(const char* fileName, const int& in_decode_mode)
{
	V2Bridge* b = (V2Bridge*)m_bridge;
	::yzFileDecode dec((::yzFileEv*)m_bridge, m_dir ? m_dir : "");
	b->notified = 0;
	b->total = 0;
	b->head = 0;

	// Parse the header first so gauge_DecodeInit/gauge_WriteInfo have data.
	{
		::yzFileDecode hd((::yzFileEv*)m_bridge, m_dir ? m_dir : "");
		if (hd.YzFile_DecodeHeader(fileName)) return -1;
		if (yzfiledecode_p_yz_dec_head) {
			delete yzfiledecode_p_yz_dec_head;
			yzfiledecode_p_yz_dec_head = 0;
		}
		yzfiledecode_p_yz_dec_head =
			new yzDecHead(yzm, (const void*)hd.yzfiledecode_p_yz_dec_head);
		b->head = yzfiledecode_p_yz_dec_head;
		b->arch_name = fileName;
		p_yz_dec_head  = yzfiledecode_p_yz_dec_head;
		dec_file_count = yzfiledecode_p_yz_dec_head ? yzfiledecode_p_yz_dec_head->m_file_count : 0;
	}
	b->NotifyDecode();
	return dec.DecodeFile(fileName, in_decode_mode);
}

// Legacy form used by Yz1Tool.cpp:147. The archive path was supplied to
// the constructor through InEv.
int yzFileDecode::DecodeFile(const int& in_decode_mode)
{
	if (!m_arc_name) return -1;
	return DecodeFile(m_arc_name, in_decode_mode);
}

} // namespace nsYZ32v2