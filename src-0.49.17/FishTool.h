#ifndef AFX_FISHTOOL_H__4EBFD054_6F0E_49CE_A2EC_24FA59007EC9__INCLUDED_
#define AFX_FISHTOOL_H__4EBFD054_6F0E_49CE_A2EC_24FA59007EC9__INCLUDED_

#include "Tool.h"

class CFishTool : public CTool
{
//
// original source is
//
// defish
//   http://www.vector.co.jp/soft/dos/util/se003310.html
// Copyright(C)1992. by fuzzy"鮪麻呂"_No.3. All Rights Reserved.
//
// 作者さんのメールアドレスがASCIInet…。
// なので、無断でソース利用させていただいてます。
// 問題がある場合、この CFishTool は即削除します。
//

public:
	CFishTool() : CTool( "Fish decode" ){}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool Header( const char* str, int* mhd, char* outname );
	void decode86( unsigned char* str, FILE* out );
	void decode464924( unsigned char* str, FILE* out, const char* xish );
};

#endif
