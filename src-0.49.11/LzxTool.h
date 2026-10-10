#ifndef AFX_LZXTOOL_H__F143A063_398D_11D4_8D96_87647745DB39__INCLUDED_
#define AFX_LZXTOOL_H__F143A063_398D_11D4_8D96_87647745DB39__INCLUDED_

#include "Tool.h"

//ÇÕÇ¡Ç´ÇËåæÇ¡ÇƒâΩÇ‡óùâÇπÇ∏Ç…é ÇµÇƒÇ‹Ç∑^^;
class CLzxTool : public CTool
{
/* LZX Extract in (supposedly) portable C.                                */

/* Compile with:                                                          */
/* gcc unlzx.c -ounlzx -O2                                                */

/* Thanks to Dan Fraser for decoding the coredumps and helping me track   */
/* down some HIDEOUSLY ANNOYING bugs.                                     */

/* Everything is accessed as unsigned char's to try and avoid problems    */
/* with byte order and alignment. Most of the decrunch functions          */
/* encourage overruns in the buffers to make things as fast as possible.  */
/* All the time is taken up in crc_calc() and decrunch() so they are      */
/* pretty damn optimized. Don't try to understand this program.           */

/* ---------------------------------------------------------------------- */


public:
	CLzxTool() : CTool( "Lzx ìWäJ" ){}
	
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"lzx");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
private:
	unsigned char info_header[10];
	unsigned char archive_header[31];
	unsigned char header_filename[256];
	unsigned char header_comment[256];

	unsigned int pack_size;
	unsigned int unpack_size;

	unsigned int crc;
	unsigned int year, month, day;
	unsigned int hour, minute, second;
	unsigned char attributes;
	unsigned char pack_mode;

	/* ---------------------------------------------------------------------- */

	struct filename_node
	{
	 struct filename_node *next;
	 unsigned int	length;
	 unsigned int	crc;
	 FILETIME		ftime;
	 char			filename[256];
	};

	struct filename_node *filename_list;

	/* ---------------------------------------------------------------------- */

	unsigned char read_buffer[16384]; /* have a reasonable sized read buffer */
	unsigned char decrunch_buffer[258+65536+258]; /* allow overrun for speed */

	unsigned char *source;
	unsigned char *destination;
	unsigned char *source_end;
	unsigned char *destination_end;

	unsigned int decrunch_method;
	unsigned int decrunch_length;
	unsigned int last_offset;
	unsigned int global_control;
	int global_shift;

	unsigned char offset_len[8];
	unsigned short offset_table[128];
	unsigned char huffman20_len[20];
	unsigned short huffman20_table[96];
	unsigned char literal_len[768];
	unsigned short literal_table[5120];

	unsigned int sum;
	static const unsigned int crc_table[256];
	static const unsigned char table_one[32];
	static const unsigned int table_two[32];
	static const unsigned int table_three[16];
	static const unsigned char table_four[34];
	void crc_calc(unsigned char *memory, unsigned int length);
	int make_decode_table(int number_symbols, int table_size,
                      unsigned char *length, unsigned short *table);
	int read_literal_table();
	void decrunch();
	FILE *open_output(char *filename);
	int extract_normal(FILE *in_file);
	int extract_store(FILE *in_file);
	int extract_unknown(FILE *in_file);
	int extract_archive(FILE *in_file);
};

#endif
