#ifndef AFX_DMGTOOL_H__D699F542_9EF8_4A1A_970A_49806A1D6AC1__INCLUDED_
#define AFX_DMGTOOL_H__D699F542_9EF8_4A1A_970A_49806A1D6AC1__INCLUDED_

#include "Tool.h"

class CDmgTool : public CTool
{
//
// consulted
//
// dmg2iso
//   http://china.pearpc.net/download/
// hfs+read
//   http://www.wht.mmtr.or.jp/~hirotsu/myware.html
// TN1150 HFS Plus Volume Format
//   http://developer.apple.com/technotes/tn/tn1150.html
//
public:
	CDmgTool();

private:
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"dmg");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool   zlibDecode( const BYTE* beg, const BYTE* end, FILE* fp );
	bool    hfsExpand( const BYTE* beg, const BYTE* end, const char* destDir );
	void inflateBlock( const BYTE* buf, size_t siz, FILE* fp, size_t outs );
};

#endif // include guard
