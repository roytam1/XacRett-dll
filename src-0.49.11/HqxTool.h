#ifndef AFX_HQXTOOL_H__A9F61040_4094_11D4_8D96_DA8E9F468D35__INCLUDED_
#define AFX_HQXTOOL_H__A9F61040_4094_11D4_8D96_DA8E9F468D35__INCLUDED_

#include "Tool.h"

class CHqxTool : public CTool
{
public:
	CHqxTool() : CTool( "BinHex Decode" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"hqx");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	FILE *fp,*op;
	char outname[MAX_PATH];
	DWORD fsize;

	bool FindBeginning(int limit);
	bool ProcessHeader();
	void DoDataFork();

	int GetByte();
	int GetByteRaw();

	bool bStarted;
	bool bEOF;
	int prevc;
	int repeating;
	DWORD outbuf,i;
};

#endif
