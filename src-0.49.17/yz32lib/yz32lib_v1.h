//////////////////////////////////////////////////////////////////////////
// yz32lib.h
//
// Reconstructed YZ32 decoding library interface for XacRett #47.
//
// The original external dependency was yz32lib 1.05 / yz32_vc42m.lib
// (Yamazaki Satoshi, BinaryTechnology), referenced by src/Yz1Tool.cpp as
//   #include "yz32lib/yz32lib.h"
//   #pragma comment( lib, "yz32lib/yz32_vc42m.lib" )
// Neither header nor lib shipped with the source drop, so both are
// reconstructed here.
//
// Which API this header reproduces
// --------------------------------
// The interface below is dictated by XacRett's own CKiYzEv
// (src/Yz1Tool.cpp, unchanged since the original import), which overrides
//   MessEX/MessINF/MessYN/MessINPUT(char*) and
//   GaugeInit(int)/GaugeSetText(char*)/GaugeSetInputValue/GaugeSetOutputValue
// i.e. the single-argument callback generation.
//
// A later yz32lib release (gauge_* + two-argument Mess*(const char*,
// const int&), DLL import/export macros, YZ_MsgTbl.h) also exists and is
// preserved verbatim as yz32lib-orig.h.  Source written against THAT release
// will not compile against this header; use yz32lib_v2.h for it.
//
// Notes:
//  - Password-protected (encrypted) YZ1 archives are supported: the
//    MessINPUT hook is used to obtain the password, which is verified
//    against the header check byte and then used to XOR-decrypt the
//    payload (see YzDeriveKey/YzApplyPassword in yz32lib_v1.cpp).
//    A wrong or absent password fails with -1 instead of producing
//    garbage - the official 0.49.17 binary crashes on these.
//  - SFX (self-extracting) archives, with the yz01 image embedded near EOF
//    behind a 4-byte size trailer, are supported.
//  - Encoding is not implemented (yzFileEncode::EncodeFile returns -1).
//  - VC++ 6.0 /MT compatible.
//
//////////////////////////////////////////////////////////////////////////
#ifndef __yz32lib_h__
#define __yz32lib_h__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <time.h>

#ifdef WIN32
#pragma warning(disable:4786)	// std::string in exported class
#pragma warning(disable:4996)	// _s
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

typedef time_t YZtime_t;

typedef std::string             FileName ;  // file name
typedef std::vector< FileName > FileTbl  ;  // file name

#if defined(_MSC_VER) && (_MSC_VER < 1200)
/* VC4 collapses same-named classes across scopes: an out-of-line
   `yzFileDecodeV1::X` definition inside `namespace nsYZ32v2` binds to
   the GLOBAL twin from this header (LNK2005, proven by dumpbin).
   Tuck the whole flat v1 API into its own namespace on VC4 only;
   VC6 keeps the historical global spelling. */
namespace nsYZ32v1 {
#endif

//-------------------------------------------------------------------
// Event interface (callback sink)
class yzFileEv
{
public:
	yzFileEv() {}
	virtual ~yzFileEv() {}

	// [!] error
	virtual void MessEX(char*) {}
	// [i] information
	virtual void MessINF(char*) {}
	// [?] query   (1 = no, 0 = yes)
	virtual int  MessYN(char*) { return 0; }
	// [INPUT] password prompt
	virtual char* MessINPUT(char*) { return 0; }

	// gauge open, with the number of entries
	virtual void GaugeInit(int) {}
	// gauge caption
	virtual void GaugeSetText(char*) {}
	// gauge current value (entry index)
	virtual int  GaugeSetInputValue(int) { return 0; }
	// gauge completed entries
	virtual int  GaugeSetOutputValue(int) { return 0; }
};

//-------------------------------------------------------------------
// One entry, as reported to the caller
class yzFileStat
{
public:
	string  m_file_name ;    // file name
	long    m_file_size ;    // expanded size
	time_t  m_file_mtime ;   // last modified

public:
	/* Default ctor: only exists so STLport (eager member
	   instantiation) accepts std::vector<yzFileStat>. */
	yzFileStat() : m_file_size(0), m_file_mtime(0) {}
	yzFileStat(
		string  in_file_name,
		long    in_file_size,
		time_t  in_file_mtime
	);
};

//-------------------------------------------------------------------
// Archive header (entry table)
class yzDecHead
{
public:
	long         m_file_count ;            // number of entries
	long *       m_file_size_tbl ;         // expanded sizes
	YZtime_t *   m_file_mtime_tbl ;        // last modified
	FileTbl      m_file_name_tbl ;         // file names
	int          m_file_name_max_length ;  // longest name
	long         m_file_size_max ;         // largest expanded size

private:
	yzFileEv*    yzm ;                     // event object
private:
	// Builds the tables from an already decoded image (used internally).
	yzDecHead(yzFileEv* yzmev, const void* entries, long count);
private:
	// Not copyable.
	yzDecHead(const yzDecHead&);
	yzDecHead& operator=(const yzDecHead&);

public:
	~yzDecHead();

public:
	// in_area     : yz01 archive image
	// in_file_cnt : expected entry count
	// yzmev       : event object (may be NULL)
	// in_size     : length of in_area
	// success     : out, 1 on success
	yzDecHead(
		const unsigned char*  in_area,
		const long&           in_file_cnt,
		yzFileEv*             yzmev,
		const long&           in_size,
		bool*                 success
	);

public:
	// Writes entry `in_n` (0-based) out of `in_mem` under in_dir_name.
	// Returns 1: written, 0: no such entry, -1: write error.
	int  MemChk(
		const char*           in_dir_name,
		const int&            in_n,
		const unsigned char*  in_mem
	);

public:
	// Dumps the entry table through MessINF.
	void Print();

private:
	// Writes entry `in_n` (0-based) from `in_data`.
	int  FileWrite(
		const char*           in_dir_name,
		const int&            in_n,
		const unsigned char*  in_data
	);

	friend class yzDecodeHeader;
	friend class yzFileDecodeV1;
};

/* Destroys a v1 header from TUs that also know nsYZ32v2::yzDecHead.
   VC4 emits a single __delDtor identity for same-named classes in
   different scopes when both are deleted in one TU (C2084), so the
   delete lives here, where only ::yzDecHead exists. */
/* Takes void*: a class-typed parameter mangles differently per TU
   on VC4 (back-reference numbering), so the helper would never link.
   The cast back is exact (callers pass yzDecHead*). */
void yzDestroyDecHead(void* head);

//-------------------------------------------------------------------
// Header accessor
class yzDecodeHeader
{
private:
	yzDecHead&  m_yz_dec_head ;

public:
	yzDecodeHeader( yzDecHead& in_yz_dec_head );

public:
	std::vector< yzFileStat >  filestat_Get();
	// The original's gauge_DecodeInit() receives a `const yzDecodeHeader&`,
	// so a const overload is required to be usable from there.
	std::vector< yzFileStat >  filestat_Get() const;
};

//-------------------------------------------------------------------
class YzMessage
{
public:
	enum language_type
	{
		language_japanese = 1,
		language_english  = 2,
	};

public:
	static language_type  language ;
};

//-------------------------------------------------------------------
enum YZ_MAIN_ENCODE_PASSWD_MODE
{
	YZ_MAIN_ENCODE_PASSWD_OFF = 0,	// no password
	YZ_MAIN_ENCODE_PASSWD_ON  = 1,	// password
	YZ_MAIN_ENCODE_PASSWD_PUB = 2	// public password
};

//-------------------------------------------------------------------
// Encoder.  Present for source compatibility only: this reconstruction
// decodes, it does not encode, so EncodeFile always fails.
class yzFileEncode
{
public:
	FileName  m_output_name ;

private:
	yzFileEv*                    yzm ;
private:
	const char*                  dirName ;
private:
	const char*                  SelfEncodeFile ;
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

	// Always returns -1: encoding is not implemented here.
	int  EncodeFile(
		const char*  in_file_name
	);
};

//-------------------------------------------------------------------
class yzFileDecodeV1
{
public:
	char*        yzfiledecode_output_dir_name ;	// output directory
	yzDecHead*   yzfiledecode_p_yz_dec_head ;	// parsed header (or NULL)

private:
	yzFileEv*    yzm ;

private:
	char         m_outdir[MAX_PATH];

	// decoded image, retained between YzFile_DecodeHeader and DecodeFile
	unsigned char*  m_image ;
	unsigned long   m_image_len ;
	long            m_image_count ;

private:
	yzFileDecodeV1(const yzFileDecodeV1&);
	yzFileDecodeV1& operator=(const yzFileDecodeV1&);

public:
	yzFileDecodeV1(
		yzFileEv*    in_yzm,
		const char*  in_dir_name
	);
	~yzFileDecodeV1();

	// Reads and parses only the entry table. 0 on success.
	int  YzFile_DecodeHeader(
		const char*  in_file_name
	);

	// Reports the entry table (or one entry) through MessINF.
	int  Print(
		const char*  fileName
	);

	// Extracts every entry. in_decode_mode: 0 = always extract.
	int  DecodeFile(
		const char*   fileName,
		const int&    in_decode_mode
	);
};

#ifdef WIN32
#pragma warning(default:4786)
#pragma warning(default:4996)
#endif
#if defined(_MSC_VER) && (_MSC_VER < 1200)
} // namespace nsYZ32v1
/* Import v1 here: v1.cpp and yz32lib.cpp name these unqualified.
   Harmless - no other yz* scope exists in their TUs alongside. */
using namespace nsYZ32v1;
/* Spelling macro for yz32lib.cpp: YZV1::yzFileDecodeV1 names the v1
   engine on VC4 and plain ::yzFileDecodeV1 everywhere else. */
#define YZV1 nsYZ32v1
#else
#define YZV1 /* empty: YZV1::yzFileDecodeV1 == ::yzFileDecodeV1 */
#endif
#endif // __yz32lib_h__