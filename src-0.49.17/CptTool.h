#ifndef AFX_CPTTOOL_H__273A41C4_4F59_11D4_8D96_E1F5C93B6A32__INCLUDED_
#define AFX_CPTTOOL_H__273A41C4_4F59_11D4_8D96_E1F5C93B6A32__INCLUDED_

#include "Tool.h"

#define CIRCSIZE 8192
struct cpt_sf_entry
{
	int Value;
	int BitLength;
};
struct cpt_node
{
	int flag, byte;
	struct cpt_node *one, *zero;
};

class CCptTool : public CTool
{
//
// original source is
//
// macunpack
//   http://ring.crl.go.jp/pub/text/CTAN/tools/macunpack/unpack/
//

public:
	CCptTool() : CTool( "Cpt “WŠJ" )
		{cptindex=NULL;make_crc_table();}
	~CCptTool()
		{delete [] cptindex;}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"cpt");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	FILE *cpt,*out;
	bool isMacBin;

	char FileName[MAX_PATH*3];
	char path[MAX_PATH*3];
	//char DirPath[MAX_PATH];//’Ç‰ÁByBonty

	BYTE folder;
	WORD foldersize;

	bool ReadCptHdr();
	void ReadFileHdr(BYTE* hdr);

	void cpt_folder(BYTE* cptptr);
	void cpt_uncompact();
	void cpt_outch(unsigned char ch);

	WORD entries;
	BYTE* cptindex;
	BYTE cpt_outstat,cpt_savechar;

	WORD cptFlag;
	DWORD rsrcULen,dataULen,rsrcCLen,dataCLen,filepos;

	// lz-huf
	void cpt_rle_lzh();
	int cpt_get6bits();
	int cpt_getbit();
	void cpt_readHuff(int size,cpt_node* Hufftree);
	int gethuffbyte(cpt_node* l_nodelist);
#define SLACK 6
	cpt_node cpt_Hufftree[512 + SLACK],cpt_LZlength[128 + SLACK]
			,cpt_LZoffs[256 + SLACK];
	unsigned char cpt_LZbuff[CIRCSIZE];
	unsigned int cpt_LZptr;
	unsigned long cpt_newbits;
	int cpt_bitsavail;
	int cpt_blocksize;

	// crc
	DWORD crc_table[256];
	void make_crc_table()
		{
			for( DWORD c,n=0; n!=256; n++ )
			{
				c = n;
				for( DWORD k=8; k; k-- )
					c = (c&1) ? ((0xedb88320L)^(c>>1)) : (c>>1);
				crc_table[n] = c;
			}
		}
	DWORD updcrc( DWORD crc,BYTE* cp,int cnt )
		{
			while( cnt-- )
				crc = (crc>>8)^crc_table[(crc&0xff)^*cp++];
			return crc;
		}
};

#endif
