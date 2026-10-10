#ifndef AFX_ZOOTOOL_H__2AF16500_3F00_11D4_8D96_F49A4FA73435__INCLUDED_
#define AFX_ZOOTOOL_H__2AF16500_3F00_11D4_8D96_F49A4FA73435__INCLUDED_

#include "Tool.h"

struct zoo_tabentry
{
	unsigned next;
	char z_ch;
};

class CZooTool : public CTool
{
//
// original source is
//
// Zoo 2.10 ( 88/01/27 )
//   http://www.vector.co.jp/soft/dos/util/se010838.html
// by Rahul Dhesi
//

public:
	CZooTool() : CTool( "Zoo “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"zoo");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool read_zoo_header( struct header* h,FILE* fp );
	bool read_zoo_dir( struct direntry *direntry,FILE* fp );

	void lzd( FILE* arc,FILE* out );
	void ad_dcode();
	void wr_dchar(int ch);
	unsigned rd_dcode();

	FILE *in_f,*out_f; 
	char *out_buf_adr,*in_buf_adr;

	unsigned stack_pointer;
	unsigned *stack;

	zoo_tabentry* table;

	unsigned cur_code;
	unsigned old_code;
	unsigned in_code;

	unsigned free_code;
	int nbits;
	unsigned max_code;

	char fin_char;
	char k;
	unsigned bit_offset;
	unsigned output_offset;
};

#endif
