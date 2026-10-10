#ifndef AFX_PITTOOL_H__9B7C4008_8CB8_4956_87F5_B27CD08A3E2C__INCLUDED_
#define AFX_PITTOOL_H__9B7C4008_8CB8_4956_87F5_B27CD08A3E2C__INCLUDED_

#include "Tool.h"

struct pithuf_node
{
	int flag, byte;
	pithuf_node *one,*zero;
};

class CPitTool : public CTool
{
//
// original source is
//
// macunpack
//   http://ring.crl.go.jp/pub/text/CTAN/tools/macunpack/unpack/
//

public:
	CPitTool() : CTool( "Pit 展開" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"pit");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	FILE *pit,*out;

	int SyncHeader(); // 3:normal 4:huffman 0:error
	bool ReadHeader();

	void yomisute( unsigned long length );
	void kakikomi( unsigned long length );

	bool huffmaned;
	char name[64];
	unsigned long datalen,rsrclen;
	unsigned long ctime,mtime; // 作成,更新

	// de-ハフマン
	pithuf_node nodelist[515], *nodeptr;
	int bit;
	void read_tree();
	pithuf_node* read_sub_tree();
	int getbit();
	int gethuffbyte(pithuf_node* np);
	int getdecodebyte();
};

#endif
