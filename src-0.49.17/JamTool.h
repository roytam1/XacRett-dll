#ifndef AFX_JAMTOOL_H__992A92A6_9367_473F_AA37_962165D104FA__INCLUDED_
#define AFX_JAMTOOL_H__992A92A6_9367_473F_AA37_962165D104FA__INCLUDED_

#include "Tool.h"

struct jam_dic_item
{
	unsigned char name;
	int son;
	int dad;
};

class CJamTool : public CTool
{
//
// original source is
//
// unjam May/96
//   ftp://ftp.externet.hu/pub/mirror/sac/pack/jam.zip
// by W. Jiang.
//

public:
	CJamTool() : CTool( "Jam May/96 “WŠJ" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:

	// main
	FILE *jam, *out;
	bool unjam();

	// lz78
	int read_code();
	unsigned char write_string( int code );
	void shrink();
	jam_dic_item* dic;
	int code_len, code, available;
	bool mapped, tagged, pure_char;

	// bit-level-io
	int read_bits(int len);
	long bitbuf, bits;
};

#endif
