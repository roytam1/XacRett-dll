// -*- Mode: C++ -*-

// Rar3Tool.h

#ifndef Rar3Tool_h__
#define Rar3Tool_h__

#include "Tool.h"

class CRar3Tool : public CTool
{
  friend int CALLBACK unrar_callback(UINT, LONG, LONG, LONG);

  int check_archive(const unsigned char *buf, size_t buf_size);
  int check_sfx(const unsigned char *buf, size_t buf_size, const char *fname);

public:
  CRar3Tool(void);
  ~CRar3Tool();

  bool IsType(const char *ext);
  bool Check(const char *fname, unsigned long fsize);
  bool Extract(const char *fname, const char *ddir);
};


#endif  // Rar3Tool_h__
