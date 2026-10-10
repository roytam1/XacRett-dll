#ifndef AFX_ARCTOOL_H__BBC59B00_49CD_11D4_8D96_9CA5AA99D535__INCLUDED_
#define AFX_ARCTOOL_H__BBC59B00_49CD_11D4_8D96_9CA5AA99D535__INCLUDED_

#include "Tool.h"

#define ARC_TABSIZE 4096

struct ArcHead
{
	unsigned char method;
	char name[13];
	unsigned long csize;
	unsigned short date;
	unsigned short time;
	unsigned short crc;
	unsigned long usize;
};

struct arc_entry
{
	char used;
	unsigned char follower;
	unsigned short next;
	unsigned short predecessor;
};

class CArcTool : public CTool
{
//
// original source is
//
// Arc 5.21 for Unix ( June 6, 1988 )
//   ftp://garbo.uwasa.fi/unix/arcers/arc.tar.Z
// by Howard Chu
//
// (C)COPYRIGHT 1985,86 by System Enhancement Associates, Inc.
//     ALL RIGHTS RESERVED
//

public:
	CArcTool() : CTool( "Arc “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"arc");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	FILE *arc,*out;
	ArcHead hdr;
	char path[MAX_PATH];
	char *pass,*ppos;

	int SkipSfxStub( const BYTE* dat,unsigned long siz,int curpos );
	bool ReadHeader();

	void Unstore();
	void UnSqueeze();
	void UnLz();
	void UnLzw();
	static int tolzw(void* buf,int size,unsigned long param);

	int myfgetc();
	int myfread(BYTE* buf,int len);
	void codebuf(char* buf,int len);

	void myfputc(int n);
	void myfputc_r(int n);
	void myfwrite(BYTE* buf,int len);
	void myfwrite_r(BYTE* buf,int len);
	int prevc;
	bool repeating;
	unsigned short crc;

	// lzw
	unsigned long lzwsize;

	// lz
	unsigned short eolist(unsigned short index);
	unsigned short hash(unsigned short pred,unsigned char foll);
	void upd_tab(unsigned short pred,unsigned char foll);

	arc_entry string_tab[ARC_TABSIZE];
	unsigned short(*h)(unsigned,unsigned char);
};

#endif
