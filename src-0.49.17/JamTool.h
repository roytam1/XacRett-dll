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
	bool IsType( const char* ext )
		{return ( 0==stricmp(ext,"jam") || 0==stricmp(ext,"jam5") || 0==stricmp(ext,"jam8") );}

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
	// Aug/96 reflection layer (see jam/AUG96/UNJAM.lst)
	bool jam_aug96;			// enable the layer
	long jam_cost_a, jam_cost_b;	// the accumulated reflection costs
	long jam_alt_used, jam_alt_total;	// how often the layer reflected
	long jam_rep, jam_codes;		// "repeat the last match" codes
	int finish_code( int val );
	bool try_aug96( long start, const char* outname );

	// stored / encrypted (see jam/SOURCE/CRYPTION.DOC)
	bool jam_stored;
	bool jam_enc;
	int  jam_enc_n;
	int  jam_enc_i;
	unsigned char jam_veil;
	unsigned char jam_prev;
	unsigned char* jam_key;
	int  jam_getc();
	bool jam_decrypt( const char* pw, unsigned short stored_crc );
	void jam_enc_free();
};

#endif
