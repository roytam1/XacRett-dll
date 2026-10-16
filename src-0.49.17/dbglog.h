// dbglog.h -- debug logging for Win32s investigation.
//
// Compiled out entirely unless XACRETT_RAR_DEBUG is defined at build
// time (add /D XACRETT_RAR_DEBUG to the flags). With it defined, logging
// is further gated at runtime on the XACRETT.DBG flag file in the run
// directory (empty file is enough; the name is 8.3-safe), appending
// "[#seq] ..." lines to XacRett.log in the current directory.
// Uses only Win32s-safe kernel32 calls plus CRT vsprintf.
#ifndef _XACRETT_DBGLOG_
#define _XACRETT_DBGLOG_

#ifdef XACRETT_RAR_DEBUG

#include <stdarg.h>
#include <stdio.h>

static int dbglog_on = -1;
static long dbglog_seq = 0;

static bool DbgOn()
{
  if (dbglog_on < 0)
  {
    /* Flag file, not an environment variable: setting those up is hard
       on Win32s. Drop an (empty) XACRETT.DBG next to the log to switch
       logging on. Name is 8.3-safe so it can be created anywhere. */
    DWORD a = GetFileAttributesA("XACRETT.DBG");
    dbglog_on = (a != 0xFFFFFFFF && !(a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
  }
  return dbglog_on != 0;
}

static void DbgLog(const char *fmt, ...)
{
  char buf[1024];
  char out[1100];
  va_list ap;
  HANDLE h;
  DWORD w, n;
  size_t len;
  long seq;

  if (!DbgOn())
    return;
  seq = ++dbglog_seq;
  va_start(ap, fmt);
  _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  va_end(ap);
  buf[sizeof(buf) - 1] = 0;
  wsprintfA(out, "[#%ld] %s", seq, buf);
  out[sizeof(out) - 1] = 0;
  len = strlen(out);
  if (len == 0 || out[len - 1] != '\n')
  {
    if (len + 1 < sizeof(out))
    {
      out[len] = '\n';
      out[len + 1] = 0;
      len++;
    }
  }
  h = CreateFileA("XacRett.log", GENERIC_WRITE,
                  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE)
    return;
  SetFilePointer(h, 0, NULL, FILE_END);
  n = (DWORD)len;
  WriteFile(h, out, n, &w, NULL);
  CloseHandle(h);
}

#else /* !XACRETT_RAR_DEBUG */

static bool DbgOn() { return false; }
static void DbgLog(const char *fmt, ...) { (void)fmt; }

#endif /* XACRETT_RAR_DEBUG */

#endif
