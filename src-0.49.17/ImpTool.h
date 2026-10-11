#ifndef AFX_IMPTOOL_H__8B2EB420_64E1_11D4_8D97_00909909C163__INCLUDED_
#define AFX_IMPTOOL_H__8B2EB420_64E1_11D4_8D97_00909909C163__INCLUDED_

#include "Tool.h"

#define IMP_HEADER_SIZE 42
struct IMP_HEADER
{
	unsigned long sig;
	unsigned long diroffset;
	unsigned long direntries;
	unsigned long ecoffset;
	unsigned long cmoffset;
	unsigned long length;
	unsigned long csize;
	unsigned short mvsig;
	unsigned short disk;
	unsigned short dirdisk;
	unsigned short cmdisk;
	unsigned short ecdisk;
	unsigned short flags;
	unsigned short crc;
};

#define IMP_DIR_ENTRY_SIZE 38
struct IMP_DIR_ENTRY
{
	unsigned short ver;
	unsigned short disk;
	unsigned long fpos;
	unsigned short attrib;
	unsigned char commlen;
	unsigned char flags;
	unsigned long unitpos;
	unsigned long esize;
	unsigned long crc;
	unsigned short extra;
	unsigned short namelen;
	unsigned short ctime;
	unsigned short cdate;
	unsigned short mtime;
	unsigned short mdate;
	unsigned short entcrc;
};

#define LOCAL_HEADER_SIZE 11
struct IMP_LOCAL_HEADER
{
	unsigned long esize;
	unsigned short namelen;
	unsigned short ver;
	unsigned short entcrc;
	unsigned char flags;
};

class CImpTool : public CTool
{
//
// original source is
//
// unimp 1.13 ( June 26, 2001 )
//   http://www.winimp.com/
// Copyright (c) 1999-2001 Technelysium Pty Ltd
//

public:
	CImpTool() : CTool( "Imp “WŠJ" )
	{
		for( unsigned long c,crc,i=0; i!=256; i++ )
		{
			crc = i;
			for( c=8;c;c-- )crc = crc&1 ? 0xEDB88320L^(crc>>1) : (crc>>1);
			crctbl[i]=crc;
		}
	}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"imp");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

public:
	FILE* imp;
	FILE* out;

	IMP_HEADER ih;
	bool ReadImpHead();
	bool HeaderCheck(const unsigned char* p);
	bool get_dir_entry( IMP_DIR_ENTRY *pdirent,char *name );
	bool extract_dir();

	void init_getbits();
	unsigned long getbits(int nbits);

	int unstore(unsigned char *output,int size);
	int expand_lz(unsigned char *output, int start, int exsize, int mm);
	int expand_bwt(unsigned char *output, int exsize);

	void reverse_mm(unsigned char *block,int bsize);
	void redo_mm(unsigned long overlap);
	int load_hcodes(unsigned char *hclengths, int ncodes, int ntrees, int exsize);
	void load_fixed_codes(unsigned char *hclens,int exsize);
	int make_decode_tables(int *ttable, int hdchain[][2], unsigned long *huffcodes,unsigned char *hclengths, int ttbits, int ncodes);
	void make_codes(unsigned long *huffcodes,unsigned char *hclengths,int ncodes);
	int decomp( FILE *outfile,unsigned long fpos,unsigned long unitpos,unsigned long togo,unsigned char flags);
	int e8ut_crc_write(FILE *outfile,unsigned char *block,int size,unsigned long e8offset,unsigned long srcsize,int last);
	int e8ut_crc_wr16(FILE *outfile,unsigned char *block,int size,unsigned long e8offset);
	int e8ut_crc_wr32(FILE *outfile,unsigned char *block,int size,unsigned long e8offset,unsigned long srcsize);

	int bitpos;
	unsigned char dcbuffer[256],*dcsrc,*dcend;
	unsigned long bitbuf,dirfpos;
	unsigned char dirbuf[8192],*dirbufsrc,*dirbufend;

	int g_mmptr;
	unsigned long g_mmlist[1024],g_prevfpos,g_blockstart;
	unsigned char *g_blockbuffer;
	unsigned long g_e8extra,g_unitoffset,g_blocksize,g_exmemory,g_crc,g_exesize;
	unsigned char g_exlast,g_exprev,g_exerror,g_e8buf[8];
	int g_e8t;

	unsigned long crctbl[256];
	unsigned long calc_crc(const unsigned char *p,unsigned long n,unsigned long crc)
	{for( unsigned long i=0; i!=n; i++ )crc=crctbl[(crc^p[i])&0xff]^(crc>>8);return crc;}
};

#endif
