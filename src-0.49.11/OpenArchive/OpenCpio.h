#ifndef AFX_OPENCPIO_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENCPIO_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include "../zlib/zlib.h"
#include "../libbzip2/bzlib.h"
#include "../complib.h"

class COpenCpio : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenCpio() : COpenArchive( )
	{COpenCpio::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);
	unsigned long SkipBlock();

	char outname[MAX_PATH];
	unsigned long orisiz;
	unsigned long mtime;
	bool isdir;
	int hdr_type;

protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
};

class COpenCpioGz : public COpenCpio
{
public:
	COpenCpioGz() : COpenCpio() {gfp=NULL;};
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	gzFile gfp;
};

class COpenCpioZ : public COpenCpio
{
public:
	COpenCpioZ() : COpenCpio() {zfp=NULL;};
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	ZFILE* zfp;
};

class COpenCpioBz : public COpenCpio
{
public:
	COpenCpioBz() : COpenCpio() {bfp=NULL;};
protected:
	virtual bool   CpioOpen(const char* fname);
	virtual size_t CpioRead(void* red,size_t size);
	virtual void   CpioClose();
private:
	BZFILE* bfp;
};

#endif