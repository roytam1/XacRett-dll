//////////////////////////////////////////////////////////////////////////
// yz32lib_v2.h
//
// Source-compatibility facade for the LATER yz32lib release, whose original
// header is preserved verbatim as yz32lib-orig.h.
//
// Why this exists
// ---------------
// Two different yz32lib APIs exist:
//
//   yz32lib.h      single-argument callbacks (MessEX(char*) ...), plus
//                  GaugeInit/GaugeSetText/GaugeSetInputValue/
//                  GaugeSetOutputValue.  This is the API XacRett's own
//                  CKiYzEv (src/Yz1Tool.cpp) overrides, i.e. the generation
//                  shipped with yz32_vc42m.lib.  USE THIS for XacRett.
//
//   yz32lib_v2.h   two-argument callbacks (Mess*(const char*, const int&))
//                  plus gauge_EncodeInit/gauge_DecodeInit/gauge_String/
//                  gauge_Value/gauge_WriteInfo.  Use this for source written
//                  against the later DLL-capable release.
//
// Both live in this static, decode-only reconstruction.  This facade is
// implemented on top of yz32lib.h (no duplicated codec), and lives in
// namespace nsYZ32v2 so both headers can be included together.
//
// Documented deviations from yz32lib-orig.h
// -----------------------------------------
//  * YZ1DLLEXP expands to nothing (static library), rather than dllimport.
//    Pre-define YZ1DLLEXP to override.
//  * The original pulls message IDs from YZ_MsgTbl.h, which was not
//    distributed with the header.  The YZ_MSG_* values below are therefore
//    reconstruction placeholders, NOT the original numbering.
//  * Encoding is not implemented: yzFileEncode::EncodeFile always fails,
//    and gauge_EncodeInit is never called.
//  * nsYZ32v2 namespace wrapper.
//
//////////////////////////////////////////////////////////////////////////
#ifndef __yz32lib_v2_h__
#define __yz32lib_v2_h__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>

#ifdef WIN32
#pragma warning(disable:4786)
#pragma warning(disable:4251)
#pragma warning(disable:4996)
#endif

#include <string>
#include <vector>
using std::string;
/* `using std::vector;` is ill-formed: a using-declaration may not name
   a class template.  VC6 tolerates it; VC4 rejects it outright
   (error C2955: vector : class template name expecting parameter
   list), so keep it only where the compiler accepts it and spell out
   std::vector everywhere below. */
#if !defined(_MSC_VER) || (_MSC_VER >= 1200)
using std::vector;
#endif

#ifndef YZ1DLLEXP
	#ifdef YZ1_EXPORTS
		#define YZ1DLLEXP __declspec(dllexport)
	#else
		#define YZ1DLLEXP
	#endif
#endif

typedef time_t YZtime_t;

typedef std::string             FileName;
typedef std::vector< FileName > FileTbl;

//-------------------------------------------------------------------
// Reconstruction placeholders for the missing YZ_MsgTbl.h numbering.
enum YZ_MSG_ID
{
	YZ_MSG_MESS_EX    = 1,
	YZ_MSG_MESS_INF   = 2,
	YZ_MSG_MESS_YN    = 3,
	YZ_MSG_MESS_INPUT = 4,
	YZ_MSG_ENCODE_INIT = 10,
	YZ_MSG_DECODE_INIT = 11,
	YZ_MSG_STRING      = 12,
	YZ_MSG_VALUE       = 13,
	YZ_MSG_WRITE_INFO  = 14
};

namespace nsYZ32v2 {

class yzFileEv;		// forward: yzDecHead holds a pointer to it

//-------------------------------------------------------------------
// One entry, as reported to the caller
class YZ1DLLEXP yzFileStat
{
public:
	std::string  m_file_name ;
	long    m_file_size ;
	time_t  m_file_mtime ;

public:
	/* Default ctor: only exists so STLport (eager member
	   instantiation) accepts std::vector<yzFileStat>. */
	yzFileStat() : m_file_size(0), m_file_mtime(0) {}
	yzFileStat(std::string in_file_name, long in_file_size, time_t in_file_mtime);
};

//-------------------------------------------------------------------
// Archive header (entry table)
class YZ1DLLEXP yzDecHead
{
public:
	long         m_file_count ;
	long *       m_file_size_tbl ;
	YZtime_t *   m_file_mtime_tbl ;
	FileTbl      m_file_name_tbl ;
	int          m_file_name_max_length ;
	long         m_file_size_max ;

private:
	yzFileEv*    yzm ;

private:
	// Adopts an already parsed engine header (internal).
	yzDecHead(yzFileEv* yzmev, const void* src);
	yzDecHead(const yzDecHead&);
	yzDecHead& operator=(const yzDecHead&);

public:
	~yzDecHead();

public:
	yzDecHead(
		const unsigned char*  in_area,
		const long&           in_file_cnt,
		yzFileEv*             yzmev,
		const long&           in_size,
		bool*                 success
	);

public:
	int  MemChk(const char* in_dir_name, const int& in_n,
	            const unsigned char* in_mem);

public:
	void Print();

private:
	int  FileWrite(const char* in_dir_name, const int& in_n,
	               const unsigned char* in_data);

	friend class yzDecodeHeader;
	friend class yzFileDecode;
};

//-------------------------------------------------------------------
class YZ1DLLEXP yzDecodeHeader
{
private:
	yzDecHead&  m_yz_dec_head ;

public:
	yzDecodeHeader(yzDecHead& in_yz_dec_head);

public:
	std::vector< yzFileStat >  filestat_Get();
	// gauge_DecodeInit() receives a `const yzDecodeHeader&`.
	std::vector< yzFileStat >  filestat_Get() const;
};

//-------------------------------------------------------------------
// Event interface, later-generation signatures.
class YZ1DLLEXP yzFileEv
{
public:
	virtual void           MessEX   (const char* in_str, const int& in_msg_no) = 0;
	virtual void           MessINF  (const char* in_str, const int& in_msg_no) = 0;
	virtual int            MessYN   (const char* in_str, const int& in_msg_no) = 0;
	virtual char*          MessINPUT(const char* in_str, const int& in_msg_no) = 0;
	virtual int  gauge_EncodeInit(const char* in_input_file_name,
	                              FileName& output_name,
	                              FileTbl&  fullpath_list,
	                              FileTbl&  file_list) = 0;
	virtual int  gauge_DecodeInit(const char* in_file_name,
	                              const yzDecodeHeader& in_yz_dec_head) = 0;
	virtual int  gauge_String   (const char* in_str) = 0;
	virtual int  gauge_Value    (const int& in_per, const int& in_end_time) = 0;
	virtual int  gauge_WriteInfo(const yzFileStat& in_yzfstat,
	                              const int& in_no, const int& in_max) = 0;
	// NB: the original declares no virtual destructor; mirrored for
	// vtable compatibility. Never delete through this pointer.
};

//-------------------------------------------------------------------
class YZ1DLLEXP YzMessage
{
public:
	enum language_type
	{
		language_japanese = 1,
		language_english  = 2
	};
public:
	static language_type  language ;
};

//-------------------------------------------------------------------
enum YZ_MAIN_ENCODE_PASSWD_MODE
{
	YZ_MAIN_ENCODE_PASSWD_OFF = 0,
	YZ_MAIN_ENCODE_PASSWD_ON  = 1,
	YZ_MAIN_ENCODE_PASSWD_PUB = 2
};

//-------------------------------------------------------------------
class YZ1DLLEXP yzFileEncode
{
public:
	FileName  m_output_name ;

private:
	yzFileEv*  yzm ;
private:
	const char* dirName ;
private:
	const char* SelfEncodeFile ;
private:
	const enum YZ_MAIN_ENCODE_PASSWD_MODE  passwd_mode ;

private:
	yzFileEncode(const yzFileEncode&);
	yzFileEncode& operator=(const yzFileEncode&);

public:
	yzFileEncode(
		yzFileEv*    in_yzm,
		const char*  in_dir_name         = NULL,
		const char*  in_self_encode_file = NULL,
		const enum YZ_MAIN_ENCODE_PASSWD_MODE  in_passwd_mode =
			 YZ_MAIN_ENCODE_PASSWD_OFF
	);

public:
	int  EncodeFile(const char* in_file_name);	// always -1
};

//-------------------------------------------------------------------
// Legacy input handle.
//
// XacRett builds the decoder as:
//     InEv  in(fname);
//     yzFileDecode dec(in, &ev, "");
// (Yz1Tool.cpp:144, OpenArchive/OpenYz1.cpp:7). InEv only carries the
// archive path; the third argument is the output directory, which is
// what the two-argument constructor already takes.
class InEv
{
public:
	explicit InEv(const char* in_file_name)
		: m_file_name(in_file_name ? in_file_name : "") {}
	const char* file_name() const { return m_file_name.c_str(); }
private:
	std::string m_file_name;
};

//-------------------------------------------------------------------
class YZ1DLLEXP yzFileDecode
{
	friend class yzDLLArcHandler;
	friend class yzDLLArcHeaderHandler;

public:
	char*        yzfiledecode_output_dir_name ;
	yzDecHead*   yzfiledecode_p_yz_dec_head ;

public:
	// Legacy spellings used by OpenArchive/OpenYz1.cpp. Kept in sync with
	// the members above so both call styles observe the same header.
	yzDecHead*   p_yz_dec_head ;      // alias of yzfiledecode_p_yz_dec_head
	long         dec_file_count ;     // entries; valid after YzFile_DecodeHeader
	long         dec_head_size ;      // archive header size (0 = uncompressed)

private:
	yzFileEv*    yzm ;
private:
	const char*  m_dir ;
private:
	const char*  m_arc_name ;	// archive path from InEv, if supplied
private:
	void*        m_bridge ;		// v1 yzFileEv adapter

private:
	yzFileDecode(const yzFileDecode&);
	yzFileDecode& operator=(const yzFileDecode&);

public:
	yzFileDecode(yzFileEv* in_yzm, const char* in_dir_name);
	// Legacy form: (InEv&, yzFileEv*, const char*). InEv supplies the
	// archive path and is passed on unchanged to YzFile_DecodeHeader.
	yzFileDecode(InEv& in, yzFileEv* in_yzm, const char* in_dir_name);
	~yzFileDecode();

public:
	int  YzFile_DecodeHeader(const char* in_file_name);

public:
	int  Print(const char* fileName);

public:
	int  DecodeFile(const char* fileName, const int& in_decode_mode);

	// Legacy form used by Yz1Tool.cpp:147 -- `decoder.DecodeFile(0)`.
	// The archive path comes from the InEv given to the constructor.
	int  DecodeFile(const int& in_decode_mode);
};

} // namespace nsYZ32v2

// XacRett's sources refer to these names unqualified. The using-directive
// lives in yzFileDe.cxx rather than here: yz32lib.cpp also includes the
// flat yz32lib_v1.h, whose global yzFileEv/yzDecHead would otherwise
// become ambiguous against nsYZ32v2's.

#ifdef WIN32
#pragma warning(default:4786)
#pragma warning(default:4251)
#pragma warning(default:4996)
#endif
#endif // __yz32lib_v2_h__