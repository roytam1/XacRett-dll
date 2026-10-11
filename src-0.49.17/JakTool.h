#ifndef AFX_JAKTOOL_H__03ED6241_3976_11D4_8D96_A6394BD8C334__INCLUDED_
#define AFX_JAKTOOL_H__03ED6241_3976_11D4_8D96_A6394BD8C334__INCLUDED_

#include "Tool.h"

class CJakTool : public CTool
{
//
// ヘッダ構造の情報ありがとうございました！ ＞ 木村利靖さん
//

public:
	CJakTool() : CTool( "Jack 結合" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"jak");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	int FindHeader( const BYTE* hdr,DWORD size );
};

#endif
