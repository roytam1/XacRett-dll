#ifndef AFX_ARGTOOL_H__9C6F9F2C_E85A_4E09_9922_43BF1CAC6027__INCLUDED_
#define AFX_ARGTOOL_H__9C6F9F2C_E85A_4E09_9922_43BF1CAC6027__INCLUDED_

#include "Tool.h"

class CArgTool : public CTool
{
//
// original source is
//
// ARG 1.00.001 BETA. ( Jun 5 1994 )
//   ftp://ftp.externet.hu/pub/mirror/sac/pack/arg.zip
// Copyright (c) Igor Pavlov  5-Jun-1994
//

public:
	CArgTool() : CTool( "Arg “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"arg");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	// Bit–ˆRead—p
	int m_Mask,m_Char;
	void bits_begin()
		{m_Mask=0;}
	char getbit(FILE* fp)
		{if(m_Mask&=7) m_Char>>=1;
		 else m_Char=fgetc(fp);
		 m_Mask++; return (m_Char&1);}
	char getbits(int iNumBits,FILE* fp)
		{int iChar=0,iMask=1;
		 for(;iNumBits>0;iNumBits--)
		 {if(getbit(fp))iChar|=iMask;iMask<<=1;}
		 return iChar;}
};

#endif
