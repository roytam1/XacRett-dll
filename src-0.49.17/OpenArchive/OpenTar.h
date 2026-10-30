#ifndef AFX_OPENTAR_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENTAR_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include "../zlib/zlib.h"
#include "../libbzip2/bzlib.h"
#include "../complib.h"

#ifndef FA_RDONLY
/* Attribute */
#define FA_RDONLY       0x01            /* Read only attribute */
#define FA_HIDDEN       0x02            /* Hidden file */
#define FA_SYSTEM       0x04            /* System file */
#define FA_LABEL        0x08            /* Volume label */
#define FA_DIREC        0x10            /* Directory */
#define FA_ARCH         0x20            /* Archive */
#endif
#ifndef TAR_SPARSE_PAIR_DEFINED
#define TAR_SPARSE_PAIR_DEFINED
// One sparse data run: file offset + length (both 64-bit; the
// apparent size of a sparse member can exceed 4GB).
struct TarSparsePair
{
	unsigned __int64 off;
	unsigned __int64 len;
};
#endif

class COpenTar : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------
	struct TarHeader
	{
		char filename[100];
		char attrib[8];
		char userID[8];	// 無視(^^;
		char groupID[8];// 無視(^^;
		char filesize[12];
		char update[12];//更新日時
		char checksum[8];
		char type;
		char linkedfilename[100];
		char magic_and_ver[8];
		char username[32];	// 無視(^^;
		char groupname[32];	// 無視(^^;
		char deviceNoM[8];
		char deviceNom[8];
	//		以下GNU Tar形式
		char lastaccessdate[12];
		char creationdate[12];
		char offset[12];
	};

	COpenTar() : COpenArchive( )
	{COpenTar::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
protected:
	virtual bool   TarOpen(LPCSTR fname);
	virtual size_t TarRead(void* red,size_t size);
	virtual void   TarClose();
private:
	void SkipBlock(int num);
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);
	// GNU sparse support (old-GNU type S + GNU 1.0 via PAX type x).
	// ReadXHeader consumes an x/g payload; sparse keys update the
	// pending name/realsize, everything else is ignored.
	bool ReadXHeader(DWORD xsize, char* nameOut, unsigned __int64* realOut);
	// ReadSparseOld parses the 4 inline pairs plus continuation
	// blocks of an old-GNU S header; apparent covers realsize too.
	bool ReadSparseOld(BYTE* hdr, TarSparsePair** pmap, DWORD* npairs,
		unsigned __int64* apparent, DWORD* mapBlocks);
};

class COpenTbz : public COpenTar
{
public:
	COpenTbz() : COpenTar() {bfp=NULL;};
protected:
	bool   TarOpen(LPCSTR fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	BZFILE* bfp;
};

class COpenTgz : public COpenTar
{
public:
	COpenTgz() : COpenTar() {gfp=NULL;};
protected:
	bool   TarOpen(LPCSTR fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	gzFile gfp;
};

class COpenTaZ : public COpenTar
{
// see complib.h

public:
	COpenTaZ() : COpenTar() {zfp=NULL;};
protected:
	bool   TarOpen(LPCSTR fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	ZFILE* zfp;
};

class COpenTlzma : public COpenTar
{
// .tar.lzma via whole-file decode to temp (see CTlzmaTarTool).
public:
	COpenTlzma() : COpenTar() {tmpPath[0]=0; tmpLive=false;};
protected:
	bool   TarOpen(LPCSTR fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	char tmpPath[MAX_PATH];
	bool tmpLive;
};

class COpenTlz : public COpenTar
{
// .tar.lz via whole-file lzip decode to temp (see CLzTarTool).
public:
	COpenTlz() : COpenTar() {tmpPath[0]=0; tmpLive=false;};
protected:
	bool   TarOpen(LPCSTR fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	char tmpPath[MAX_PATH];
	bool tmpLive;
};

class COpenTxz : public COpenTar
{
public:
	COpenTxz() : COpenTar() {tmpPath[0]=0; tmpLive=false;};
protected:
	bool   TarOpen(LPCSTR fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	char tmpPath[MAX_PATH];
	bool tmpLive;
};

class COpenTzst : public COpenTar
{
public:
	COpenTzst() : COpenTar() {tmpPath[0]=0; tmpLive=false;};
protected:
	bool   TarOpen(LPCSTR fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	char tmpPath[MAX_PATH];
	bool tmpLive;
};

class COpenTaz : public COpenTar
{
// see complib.h

public:
	COpenTaz() : COpenTar() {pfp=NULL;};
	
protected:
	bool   TarOpen(LPCSTR fname);
	size_t TarRead(void* red,size_t size);
	void   TarClose();
private:
	PFILE* pfp;
};

#endif