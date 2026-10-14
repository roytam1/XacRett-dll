#ifndef AFX_TARTOOL_H__5015A722_3A09_11D4_8D96_9C430C9F5A32__INCLUDED_
#define AFX_TARTOOL_H__5015A722_3A09_11D4_8D96_9C430C9F5A32__INCLUDED_

#include "Tool.h"
#include "zlib/zlib.h"
#include "libbzip2/bzlib.h"
#include "complib.h"

#ifndef FA_RDONLY
/* Attribute */
#define FA_RDONLY       0x01            /* Read only attribute */
#define FA_HIDDEN       0x02            /* Hidden file */
#define FA_SYSTEM       0x04            /* System file */
#define FA_LABEL        0x08            /* Volume label */
#define FA_DIREC        0x10            /* Directory */
#define FA_ARCH         0x20            /* Archive */
#endif

// TAR+BZIP2 Ç∆Ç© TAR+GZIP Ç∆Ç© TAR+COMPRESS Ç∆Ç© àÍäáÉTÉ|Å[Ég
class CTarTool : public CTool
{
public:
	CTarTool() : CTool( "Tar ìWäJ" ){fp=NULL;}
	CTarTool(const char* r)	: CTool(r) {fp=NULL;}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"tar");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

protected:
	virtual bool   TarOpen(const char* fname);
	virtual size_t TarRead(void* red,size_t size);
	virtual void   TarClose();
private:
	void SkipBlock(int num);
	FILE* fp;
};

class CTbzTool : public CTarTool
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
	CTbzTool() : CTarTool("Tar+Bzip2 ìWäJ") {bfp=NULL;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"bz2") || 0==stricmp(ext,"tbz");}
protected:
	bool   TarOpen(const char* fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	BZFILE* bfp;
};

class CTgzTool : public CTarTool
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

public:
	CTgzTool() : CTarTool("Tar+Gzip ìWäJ") {gfp=NULL;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"gz") || 0==stricmp(ext,"tgz");}
protected:
	bool   TarOpen(const char* fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	gzFile gfp;
};

class CTaZTool : public CTarTool
{
// see complib.h

public:
	CTaZTool() : CTarTool("Tar+Compress ìWäJ") {zfp=NULL;};
	bool IsType( const char* ext )
		{return 0==strcmp(ext,"Z") || 0==strcmp(ext,"taZ");}
protected:
	bool   TarOpen(const char* fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	ZFILE* zfp;
};

class CTlzmaTarTool : public CTarTool
{
// .tar.lzma via whole-file LZMA-Alone decode to a temp file,
// then plain tar over it (inherited Check/Extract just work).
// Temp uses a fixed per-type name and is self-healing (removed
// on open/close); a cancel between open and close can leave one
// stale file behind at most, deleted on the next open.
public:
	CTlzmaTarTool() : CTarTool("Tar+LZMA ìWäJ") {tmpPath[0]=0; tmpLive=false;};
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"lzma") || 0==stricmp(ext,"tlz");}
protected:
	bool   TarOpen(const char* fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	char tmpPath[MAX_PATH];
	bool tmpLive;
};

class CTazTool : public CTarTool
{
// see complib.h

public:
	CTazTool() : CTarTool("Tar+Pack ìWäJ") {pfp=NULL;};
	bool IsType( const char* ext )
		{return 0==strcmp(ext,"z") || 0==strcmp(ext,"taz");}
protected:
	bool   TarOpen(const char* fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	PFILE* pfp;
};

#endif
