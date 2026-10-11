///////////////////////////////////////////////////////////////////
// zdatas.h
//
// ZData API definitions (reconstructed, source-compatible).
//
// InstallShield Compression and Maintenance util
// Original binary by fOSSiL - 2000 (*Any use is authorized,
// granted the proper credit is given*).
//
// This header + zdata.c reimplement the ZData 8-function API over
// stock zlib (raw deflate, matching the original wire behavior).
// It is a drop-in source replacement for the 32-bit-only COFF
// object (zdata.lib) and is safe for AMD64 builds.
//
// API/behavior notes (verified against the original binary):
//  - ZDataSetup(2, 0xfa372dea, 0) initializes; any other magic fails.
//  - ZDataSetInfo ids: 1=buffer size, 3=callback param, 4=read func,
//    5=write func, 6=buffer ptr. Unknown ids fail.
//  - Compressed input framing ("new" style): a stream of chunks,
//    each = u16LE length + raw deflate stream (no zlib header).
//    Chunks decode independently; a zero length ends the stream.
//    A failed chunk still contributes bytes decoded before the failure,
//    matching the original's consume-all/emits-whatever-decodes behavior.
//    On malformed streams, the exact failure cutoff is determined by the
//    linked inflate implementation rather than this wrapper.
//  - Pointer-carrying SetInfo values use ULONG_PTR so the API is
//    64-bit clean (on x86 the __stdcall export names are unchanged).
//
#ifndef ZDATAS_INC
#define ZDATAS_INC

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagZCOMPRESSBUF {
	DWORD Ptr;
	WORD Len;
} ZCOMPRESSBUF, *LPZCOMPRESSBUF;

#define ZDAPI		__stdcall
#define ZDCALLBACK	__stdcall

/* Callback prototypes (buffer, in/out length, user param) */
typedef int (ZDCALLBACK *ZD_READ_CB)(void* pBuf, DWORD* pLen, void* pParam);
typedef int (ZDCALLBACK *ZD_WRITE_CB)(void* pBuf, DWORD* pLen, void* pParam);

// ZDataSetup(2, 0xfa372dea, 0)
int ZDAPI ZDataSetup(DWORD, DWORD, DWORD);
int ZDAPI ZDataUnSetup();
// ZDataSetInfo(INFO_xxx, value)
int ZDAPI ZDataSetInfo(DWORD, ULONG_PTR);
int ZDAPI ZDataGetLastError();
// ZDataStart(SessionId)
int ZDAPI ZDataStart(DWORD);
int ZDAPI ZDataEnd();
// ZDataCompress (see zdata.c; original observable behavior kept)
int ZDAPI ZDataCompress(DWORD, LPZCOMPRESSBUF);
// ZDataDecompress(SessionId)
int ZDAPI ZDataDecompress(DWORD);

#define INFO_BUFFER_SIZE	1	// Param = buffer size
#define INFO_CALLBACK_PARAM	3	// Param = param passed to callback function
#define INFO_READ_CALLBACK	4	// Param = addr of Read function
#define INFO_WRITE_CALLBACK	5	// Param = addr of Write function
#define INFO_BUFFER_PTR		6	// Param = addr of buffer

#define STD_DECOMP_BUFFER	0x7d00
#define STD_COMP_BUFFER		0x2c00

//int ZDATACALLBACK (LPVOID pBuf, LPDWORD pBufLen, DWORD CallbackParam);

#ifdef __cplusplus
}
#endif

#endif // ZDATAS_INC
