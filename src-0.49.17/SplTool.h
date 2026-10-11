#ifndef AFX_SPLTOOL_H__B0A61D62_4943_11D4_8D96_83B5D33B0032__INCLUDED_
#define AFX_SPLTOOL_H__B0A61D62_4943_11D4_8D96_83B5D33B0032__INCLUDED_

#include "Tool.h"

#define SPL_EOF_CODE 256

struct spl_tree
{
	int	up[516];
	int	left[258];
	int	right[258];
};

class CSplTool : public CTool
{
//
// original source is
//
// Splint 2.1 ( 89/04/12 )
//   ftp://ftp.externet.hu/pub/mirror/sac/pack/splint.zip
// Copyright (C)1988, 1989 by Kenji Rikitake and Naoshi Wakatabe. All rights reserved.
//
public:
	CSplTool() : CTool( "Splint “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"sp") || 0==stricmp(ext,"spl");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	spl_tree stree[64];
	int state;
	bool flagEOF;

	void spl_init();
	void splay(int sym);
	int spl_exp(FILE* inp);

	int input_bit(FILE* inp);
	int ibuf;
	int ibitstogo;
	int igarbits;

	char* pass;
};

#endif
