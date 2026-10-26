#ifndef AFX_CPIOTOOL_H__D5CD1898_2852_467C_9CA9_9BC1AB451094__INCLUDED_
#define AFX_CPIOTOOL_H__D5CD1898_2852_467C_9CA9_9BC1AB451094__INCLUDED_

#include "Tool.h"
#include "zlib/zlib.h"
#include "libbzip2/bzlib.h"
#include "complib.h"

// cpio, cpio.gz, cpio.z, cpio.bz2, rpm àÍäáÉTÉ|Å[Ég
class CCpioTool : public CTool
{
public:
	CCpioTool() : CTool( "Cpio ìWäJ" ){fp=NULL;}
	CCpioTool(const char* r) : CTool(r) {fp=NULL;}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"rpm") || 0==stricmp(ext,"cpio");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	FILE* fp;

	bool DoCpioHeader();
	void ProcessData();

	char outname[MAX_PATH];
	unsigned long orisiz;
	unsigned long mtime;
	bool isdir;
	int hdr_type;
};

// cpio.gz || rpm
class CCpioGzTool : public CCpioTool
{
//
// using
//
// zlib 1.1.3
//   http://www.info-zip.org/pub/infozip/zlib/
// Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler
//
// see "zlib/zlib.h"
//

//
// thanks to
//
// Format of the RPM File
//   http://www.rpmdp.org/rpmbook/node117.html
//

public:
	CCpioGzTool() : CCpioTool("Cpio.gz/Rpm ìWäJ") {gfp=NULL;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"gz") || 0==stricmp(ext,"rpm");}
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	gzFile gfp;
};

// cpio.Z
class CCpioZTool : public CCpioTool
{
// see complib.h

public:
	CCpioZTool() : CCpioTool("Cpio.Z ìWäJ") {zfp=NULL;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"Z");}
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	ZFILE* zfp;
};

// cpio.bz2
class CCpioBzTool : public CCpioTool
{
//
// using
//
// libbzip2 1.0.1
//   http://sources.redhat.com/bzip2/
// Copyright (C) 1996-2000 Julian R Seward.  All rights reserved.
//
// see "libbzip2/LICENSE"
//

public:
	CCpioBzTool() : CCpioTool("Cpio.bz2 ìWäJ") {bfp=NULL;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"bz2");}
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	BZFILE* bfp;
};

// .cpio.lzma via whole-file LZMA decode to a temp file,
// then plain cpio over it (inherited Check/Extract just work).
// Temp uses a fixed per-type name and is self-healing (removed
// on open/close); a cancel between open and close can leave one
// stale file behind at most, deleted on the next open.
class CCpioLzmaTool : public CCpioTool
{
public:
	CCpioLzmaTool() : CCpioTool("Cpio+LZMA ìWäJ") {tmpPath[0]=0; tmpLive=false;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"lzma");}
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	char tmpPath[MAX_PATH];
	bool tmpLive;
};

// .cpio.xz via whole-file XZ decode to a temp file,
// then plain cpio over it (inherited Check/Extract just work).
// Temp uses a fixed per-type name and is self-healing (removed
// on open/close); a cancel between open and close can leave one
// stale file behind at most, deleted on the next open.
class CCpioXzTool : public CCpioTool
{
public:
	CCpioXzTool() : CCpioTool("Cpio+XZ ìWäJ") {tmpPath[0]=0; tmpLive=false;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"xz");}
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	char tmpPath[MAX_PATH];
	bool tmpLive;
};

// .cpio.zst via whole-file ZSTD decode to a temp file,
// then plain cpio over it (inherited Check/Extract just work).
// Temp uses a fixed per-type name and is self-healing (removed
// on open/close); a cancel between open and close can leave one
// stale file behind at most, deleted on the next open.
class CCpioZstTool : public CCpioTool
{
public:
	CCpioZstTool() : CCpioTool("Cpio+ZSTD ìWäJ") {tmpPath[0]=0; tmpLive=false;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"zst");}
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	char tmpPath[MAX_PATH];
	bool tmpLive;
};

#endif
