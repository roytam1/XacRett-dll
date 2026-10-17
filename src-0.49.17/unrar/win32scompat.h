// win32scompat.h -- ANSI fallback for the file APIs unrar needs.
//
// unrar is built with UNICODE defined (os.hpp), so every CreateFile /
// CreateDirectory / GetFileAttributes / ... below is really the W entry
// point. On Win32s (Windows 3.1) those resolve to stubs that fail, which
// used to make every RAR open, listing and extraction fail there while
// the ANSI CRT paths (fopen, our own RarTool reader) kept working.
//
// Detect the affected systems once via GetVersion - high bit SET
// (non-NT) plus the major/minor version - and only there convert the
// path to ACP and call the A entry point:
//   major < 4            Win32s (3.1 / 3.11)
//   major 4, minor 0     Windows 95
// Win95 belongs here too: it also chokes on the Unicode file APIs, so
// an earlier belief that its W entry points were usable made every RAR
// open fail there with ERAR_EREAD (18). The minor check keeps Win98
// and later on the W path, where the W APIs do work. NT is untouched
// (high bit clear) and compiles to the plain W call.
//
// Conversion never touches an NLS API for pure-ASCII names (byte copy),
// which covers every shipped fixture; only non-ASCII names go through
// WideCharToMultiByte(CP_ACP), with '?' substitution if even that fails.

#ifndef _RAR_WIN32SCOMPAT_
#define _RAR_WIN32SCOMPAT_

#include "../dbglog.h"

static int win32s_no_unicode = -1;

static bool Win32NoUnicode()
{
  if (win32s_no_unicode < 0)
  {
    /* XACRETT_NOUNICODE=1 forces the ANSI path for testing on machines
       that do have Unicode (proves the fallback logic without Win32s). */
    char probe[2];
    if (GetEnvironmentVariableA("XACRETT_NOUNICODE", probe, sizeof(probe)) > 0)
    {
      win32s_no_unicode = 1;
      return true;
    }
    DWORD v = GetVersion();
    DbgLog("NOUNICODE gate: GetVersion=0x%08X", v);
    win32s_no_unicode = 0;
    /* High bit set means non-NT; major version 3 with it set is Win32s
       (Windows 3.1 - 0x80AB0A03 on the test machine: highbit=1, major=3,
       minor=10). Win32s has no usable Unicode file API, so take the ANSI
       path there. Win95 (highbit=1, major 4) and all NT (highbit=0) keep
       the W path: their W file APIs work. */
    if ((v & 0x80000000) && LOBYTE(LOWORD(v)) < 4)
      win32s_no_unicode = 1;
    /* Windows 95 chokes on Unicode APIs as well. */
    else if ((v & 0x80000000) && LOBYTE(LOWORD(v)) == 4 && (DWORD)(HIBYTE(LOWORD(v))) == 0)
      win32s_no_unicode = 1;
    else
    {
      /* Belt and braces: if the W entry point is not even exported,
         calling it cannot work on any system. (Presence alone proves
         nothing - Win32s exports failing stubs, T7 ERROR_CALL_NOT_IMPLEMENTED.) */
      HMODULE k = GetModuleHandleA("kernel32.dll");
      if (k != NULL && GetProcAddress(k, "CreateFileW") == NULL)
        win32s_no_unicode = 1;
    }
  }
  DbgLog("NOUNICODE gate: decision=%d", win32s_no_unicode);
  return win32s_no_unicode != 0;
}

// wstring -> ACP bytes. Never fails; non-convertible chars become '?'.
static void WinWideToAnsi(const std::wstring &W, std::string &A)
{
  size_t i;
  A.resize(0);
  for (i = 0; i < W.size(); i++)
    if ((unsigned)W[i] >= 0x80)
      break;
  if (i >= W.size())
  {
    A.reserve(W.size());
    for (i = 0; i < W.size(); i++)
      A += (char)W[i];
    return;
  }
  int n = WideCharToMultiByte(CP_ACP, 0, W.c_str(), (int)W.size(), NULL, 0, NULL, NULL);
  if (n > 0)
  {
    std::vector<char> buf((size_t)n);
    WideCharToMultiByte(CP_ACP, 0, W.c_str(), (int)W.size(), &buf[0], n, NULL, NULL);
    A.assign(&buf[0], (size_t)n);
  }
  else
    for (i = 0; i < W.size(); i++)
      A += (char)((unsigned)W[i] < 0x80 ? W[i] : '?');
}

// ACP bytes -> wstring. Never fails; non-convertible bytes become '?'.
static void WinAnsiToWide(const std::string &A, std::wstring &W)
{
  size_t i;
  W.resize(0);
  for (i = 0; i < A.size(); i++)
    if ((unsigned char)A[i] >= 0x80)
      break;
  if (i >= A.size())
  {
    W.reserve(A.size());
    for (i = 0; i < A.size(); i++)
      W += (wchar_t)(unsigned char)A[i];
    return;
  }
  int n = MultiByteToWideChar(CP_ACP, 0, A.c_str(), (int)A.size(), NULL, 0);
  if (n > 0)
  {
    std::vector<wchar_t> buf((size_t)n);
    MultiByteToWideChar(CP_ACP, 0, A.c_str(), (int)A.size(), &buf[0], n);
    W.assign(&buf[0], (size_t)n);
  }
  else
    for (i = 0; i < A.size(); i++)
      W += (wchar_t)(unsigned char)A[i];
}

static HANDLE WinCreateFile(const std::wstring &Name, DWORD Access, DWORD Share,
                            LPSECURITY_ATTRIBUTES SA, DWORD Disp, DWORD Flags)
{
  if (!Win32NoUnicode())
    return CreateFileW(Name.c_str(), Access, Share, SA, Disp, Flags, NULL);
  std::string A; WinWideToAnsi(Name, A);
  {
    HANDLE h = CreateFileA(A.c_str(), Access, Share, SA, Disp, Flags, NULL);
    DbgLog("WinCreateFileA [%s] -> %s", A.c_str(), h == INVALID_HANDLE_VALUE ? "FAIL" : "ok");
    return h;
  }
}

static BOOL WinCreateDirectory(const std::wstring &Name)
{
  if (!Win32NoUnicode())
    return CreateDirectoryW(Name.c_str(), NULL);
  std::string A; WinWideToAnsi(Name, A);
  return CreateDirectoryA(A.c_str(), NULL);
}

static DWORD WinGetFileAttributes(const std::wstring &Name)
{
  if (!Win32NoUnicode())
    return GetFileAttributesW(Name.c_str());
  std::string A; WinWideToAnsi(Name, A);
  return GetFileAttributesA(A.c_str());
}

static BOOL WinSetFileAttributes(const std::wstring &Name, DWORD Attr)
{
  if (!Win32NoUnicode())
    return SetFileAttributesW(Name.c_str(), Attr);
  std::string A; WinWideToAnsi(Name, A);
  return SetFileAttributesA(A.c_str(), Attr);
}

static BOOL WinDeleteFile(const std::wstring &Name)
{
  if (!Win32NoUnicode())
    return DeleteFileW(Name.c_str());
  std::string A; WinWideToAnsi(Name, A);
  return DeleteFileA(A.c_str());
}

static BOOL WinRemoveDirectory(const std::wstring &Name)
{
  if (!Win32NoUnicode())
    return RemoveDirectoryW(Name.c_str());
  std::string A; WinWideToAnsi(Name, A);
  return RemoveDirectoryA(A.c_str());
}

static BOOL WinMoveFile(const std::wstring &Src, const std::wstring &Dst)
{
  if (!Win32NoUnicode())
    return MoveFileW(Src.c_str(), Dst.c_str());
  std::string A, B; WinWideToAnsi(Src, A); WinWideToAnsi(Dst, B);
  return MoveFileA(A.c_str(), B.c_str());
}

static BOOL WinSetCurrentDirectory(const std::wstring &Dir)
{
  if (!Win32NoUnicode())
    return SetCurrentDirectoryW(Dir.c_str());
  std::string A; WinWideToAnsi(Dir, A);
  return SetCurrentDirectoryA(A.c_str());
}

static DWORD WinGetCurrentDirectory(DWORD Len, wchar_t *Buf)
{
  if (!Win32NoUnicode())
    return GetCurrentDirectoryW(Len, Buf);
  std::vector<char> dirbuf(MAX_PATH);
  DWORD n = GetCurrentDirectoryA((DWORD)dirbuf.size(), &dirbuf[0]);
  if (n == 0 || n >= dirbuf.size())
    return 0;
  std::string A(&dirbuf[0]);
  std::wstring W; WinAnsiToWide(A, W);
  DWORD need = (DWORD)W.size() + 1;
  if (Len == 0)
    return need;
  if (need > Len)
    return need;
  size_t i;
  for (i = 0; i < W.size(); i++)
    Buf[i] = W[i];
  Buf[W.size()] = 0;
  return (DWORD)W.size();
}

static DWORD WinGetFullPathName(const std::wstring &Src, DWORD Len, wchar_t *Buf)
{
  if (!Win32NoUnicode())
    return GetFullPathNameW(Src.c_str(), Len, Buf, NULL);
  std::string A; WinWideToAnsi(Src, A);
  std::vector<char> fullbuf(MAX_PATH * 2);
  DWORD n = GetFullPathNameA(A.c_str(), (DWORD)fullbuf.size(), &fullbuf[0], NULL);
  if (n == 0 || n >= fullbuf.size())
    return 0;
  std::string FA; FA.assign(&fullbuf[0], n);
  std::wstring W; WinAnsiToWide(FA, W);
  DWORD need = (DWORD)W.size() + 1;
  if (Len == 0)
    return need;
  if (need > Len)
    return need;
  size_t i;
  for (i = 0; i < W.size(); i++)
    Buf[i] = W[i];
  Buf[W.size()] = 0;
  return (DWORD)W.size();
}


#endif
