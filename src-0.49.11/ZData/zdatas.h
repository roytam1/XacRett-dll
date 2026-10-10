///////////////////////////////////////////////////////////////////
// zdatas.h
//  
// Static ZData definitions
//
// InstallShield Compression and Maintenance util
// fOSSiL - 2000
//
// *Any use is authorized, granted the proper credit is given*
//
// No support will be provided for this code
//

#ifndef ZDATAS_INC
#define ZDATAS_INC

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tagZCOMPRESSBUF {
	DWORD Ptr;
	WORD Len;
} ZCOMPRESSBUF, *LPZCOMPRESSBUF;

#define ZDAPI		__stdcall
#define ZDCALLBACK	__stdcall

// ZDataSetup(2, 0xfa372dea, 0)
int ZDAPI ZDataSetup(DWORD, DWORD, DWORD);
int ZDAPI ZDataUnSetup();
// ZDataSetInfo(INFO_func, Param)
int ZDAPI ZDataSetInfo(DWORD, DWORD);
int ZDAPI ZDataGetLastError();
// ZDataStart(SessionId)
int ZDAPI ZDataStart(DWORD);
int ZDAPI ZDataEnd();
// ZDataCompress
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
