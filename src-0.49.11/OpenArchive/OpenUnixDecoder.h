#ifndef AFX_OPENUnixDecoder_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENUnixDecoder_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenUnixDecoder : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
protected:
	COpenUnixDecoder( char a,char b )
		: COpenArchive(), x(a), X(b) {COpenUnixDecoder::fp=NULL;}
private:
	char x,X;
};

class COpenUue : public COpenUnixDecoder
{
public:
	COpenUue() : COpenUnixDecoder('u','U' ) {}
};

class COpenXxe : public COpenUnixDecoder
{
public:
	COpenXxe() : COpenUnixDecoder('x','X' ) {}
};

#endif