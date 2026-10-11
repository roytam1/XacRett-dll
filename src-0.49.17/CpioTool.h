#ifndef AFX_CPIOTOOL_H__D5CD1898_2852_467C_9CA9_9BC1AB451094__INCLUDED_
#define AFX_CPIOTOOL_H__D5CD1898_2852_467C_9CA9_9BC1AB451094__INCLUDED_

#include "Tool.h"
#include "zlib/zlib.h"
#include "libbzip2/bzlib.h"
#include "complib.h"

// cpio, cpio.gz, cpio.z, cpio.bz2, rpm 一括サポート
class CCpioTool : public CTool
{
public:
	CCpioTool() : CTool( "Cpio 展開" ){fp=NULL;}
	CCpioTool(const char* r) : CTool(r) {}

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
	CCpioGzTool() : CCpioTool("Cpio.gz/Rpm 展開") {gfp=NULL;};
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
	CCpioZTool() : CCpioTool("Cpio.Z 展開") {zfp=NULL;};
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
	CCpioBzTool() : CCpioTool("Cpio.bz2 展開") {bfp=NULL;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"bz2");}
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	BZFILE* bfp;
};

#endif
