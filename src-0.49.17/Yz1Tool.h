#ifndef AFX_YZ1TOOL_H__4D73EA81_3956_11D4_8D96_D938F6BC5F33__INCLUDED_
#define AFX_YZ1TOOL_H__4D73EA81_3956_11D4_8D96_D938F6BC5F33__INCLUDED_

#include "Tool.h"

class CYz1Tool : public CTool
{
//
// using
//
// yz32*.lib in DeepFreezer 1.05
//   http://member.nifty.ne.jp/yamazaki/
//

public:
	CYz1Tool() : CTool( "Yz1 “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"yz1");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	friend class CKiYzEv;
};

#endif
