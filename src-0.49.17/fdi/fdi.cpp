//////////////////////////////////////////////////////////////////////
// fdi.cpp
//
// Cabinet FDI entry points for XacRett.dll.
//
// The original project linked a static "fdi/fdi.lib" that wrapped the
// Microsoft Cabinet decompression interface (CabTool.cpp). That library
// was not redistributable with the 0.49.11 source drop, and cabinet.lib
// is not present on this machine either.
//
// The FDI implementation itself is exported by %SystemRoot%\system32\
// cabinet.dll on every supported Windows version, so this shim resolves
// the entry points at run time and forwards to them. Linking fdi.lib
// therefore keeps CabTool.cpp completely unchanged -- it still uses
// FDICreate/FDICopy through "#pragma comment(lib, "fdi/fdi.lib")".
//////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Fdi.h"

static HMODULE g_cabinet = 0;
static BOOL    g_tried    = FALSE;

typedef HFDI (FAR DIAMONDAPI *PFN_FDICreate)(PFNALLOC, PFNFREE, PFNOPEN,
                                            PFNREAD, PFNWRITE, PFNCLOSE,
                                            PFNSEEK, int, PERF);
typedef BOOL (FAR DIAMONDAPI *PFN_FDICopy)(HFDI, char FAR *, char FAR *, int,
                                           PFNFDINOTIFY, PFNFDIDECRYPT,
                                           void FAR *);
typedef BOOL (FAR DIAMONDAPI *PFN_FDIIsCabinet)(HFDI, INT_PTR, PFDICABINETINFO);
typedef BOOL (FAR DIAMONDAPI *PFN_FDIDestroy)(HFDI);
typedef BOOL (FAR DIAMONDAPI *PFN_FDITruncateCabinet)(HFDI, char FAR *);

static HMODULE FDI_Load(void)
{
	if (!g_tried) {
		g_tried = TRUE;
		UINT oldErrMode = SetErrorMode(SEM_NOOPENFILEERRORBOX);
		g_cabinet = LoadLibraryA("cabinet.dll");
		SetErrorMode(oldErrMode);
	}
	return g_cabinet;
}

#define FDI_PROC_UNUSED 0

HFDI FAR DIAMONDAPI FDICreate(PFNALLOC pfnalloc,
                              PFNFREE  pfnfree,
                              PFNOPEN  pfnopen,
                              PFNREAD  pfnread,
                              PFNWRITE pfnwrite,
                              PFNCLOSE pfnclose,
                              PFNSEEK  pfnseek,
                              int      cpuType,
                              PERF     perf)
{
	typedef PFN_FDICreate t; static HMODULE m = FDI_Load();
	t p;
	if (!m) return NULL;
	p = (t)GetProcAddress(m, "FDICreate");
	if (!p) return NULL;
	return p(pfnalloc, pfnfree, pfnopen, pfnread, pfnwrite, pfnclose,
	         pfnseek, cpuType, perf);
}

BOOL FAR DIAMONDAPI FDICopy(HFDI          hfdi,
                            char FAR     *pszCabinet,
                            char FAR     *pszCabPath,
                            int           flags,
                            PFNFDINOTIFY  pfnfdin,
                            PFNFDIDECRYPT pfnfdid,
                            void FAR     *pvUser)
{
	typedef PFN_FDICopy t; static HMODULE m = FDI_Load();
	t p;
	if (!m) return FALSE;
	p = (t)GetProcAddress(m, "FDICopy");
	if (!p) return FALSE;
	return p(hfdi, pszCabinet, pszCabPath, flags, pfnfdin, pfnfdid, pvUser);
}

BOOL FAR DIAMONDAPI FDIIsCabinet(HFDI            hfdi,
                                 INT_PTR         hf,
                                 PFDICABINETINFO pfdici)
{
	typedef PFN_FDIIsCabinet t; static HMODULE m = FDI_Load();
	t p;
	if (!m) return FALSE;
	p = (t)GetProcAddress(m, "FDIIsCabinet");
	if (!p) return FALSE;
	return p(hfdi, hf, pfdici);
}

BOOL FAR DIAMONDAPI FDIDestroy(HFDI hfdi)
{
	typedef PFN_FDIDestroy t; static HMODULE m = FDI_Load();
	t p;
	if (!m) return FALSE;
	p = (t)GetProcAddress(m, "FDIDestroy");
	if (!p) return FALSE;
	return p(hfdi);
}

BOOL FAR DIAMONDAPI FDITruncateCabinet(HFDI        hfdi,
                                        char FAR   *pszCabinet)
{
	typedef PFN_FDITruncateCabinet t; static HMODULE m = FDI_Load();
	t p;
	if (!m) return FALSE;
	p = (t)GetProcAddress(m, "FDITruncateCabinet");
	if (!p) return FALSE;
	return p(hfdi, pszCabinet);
}