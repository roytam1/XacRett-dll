#ifndef AFX_ASKTOOL_H_INCLUDED_
#define AFX_ASKTOOL_H_INCLUDED_

#include "Tool.h"

class CAskTool : public CTool
{
//
// original source is
//
// Neo_ASK
//   http://hp.vector.co.jp/authors/VA022180/
// (c) 2001/1/14 è¨çg
//

public:
	CAskTool() : CTool( "Neo_ASK ìWäJ" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	unsigned char* UnPack_ask(const char *file,unsigned long int &length);
	unsigned char* UnPack(unsigned char *root,unsigned long int file_length,unsigned long int &length);
	bool Cut(const char *file);
};

#endif
