#ifndef AFX_FRZTOOL_H__F16CF540_5F27_11D4_8D96_00909909C163__INCLUDED_
#define AFX_FRZTOOL_H__F16CF540_5F27_11D4_8D96_00909909C163__INCLUDED_

#include "Tool.h"

class CFrzTool : public CTool
{
//
// original source is
//
// freeze 2.5
//   ftp://ftp.externet.hu/pub/mirror/sac/pack/freeze25.zip
//

public:
	CFrzTool() : CTool( "Freeze “WŠJ" )
	{
		Table1[0]=Table1[1]=Table1[2]=0;
		Table1[3]=1;Table1[4]=3;Table1[5]=8;
		Table1[6]=12;Table1[7]=24;Table1[8]=16;
		Table2[0]=Table2[1]=Table2[2]=0;
		Table2[3]=1;Table2[4]=2;Table2[5]=6;
		Table2[6]=19;Table2[7]=34;Table2[8]=0;
	}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"F") || 0==stricmp(ext,"xF");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	FILE *frz,*out;

	unsigned char window[8192];
	void melt1();
	void melt2();

	void init(unsigned char* table);
	int DecodeChar();
	int DecodePosition();
	short DecodePOld();

	bool read_header();
	void StartHuff( int n_char );
	void reconst();
	void update(int c);

	int tt,rr,chars;
	unsigned char p_len[64];
	unsigned char d_len[256];
	unsigned char code[256];
	unsigned freq[1022];
	int son[1021];
	int prnt[1532];
	unsigned char Table1[9];
	unsigned char Table2[9];

	unsigned long bitbuf;
	long bitlen;
	long overrun;
};

#endif
