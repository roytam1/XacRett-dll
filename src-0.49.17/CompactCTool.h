#ifndef AFX_COMPACTCTOOL_H__32CB3E9E_470E_41B5_953A_38E78229AC9A__INCLUDED_
#define AFX_COMPACTCTOOL_H__32CB3E9E_470E_41B5_953A_38E78229AC9A__INCLUDED_

#include "Tool.h"

struct c_fpoint;
struct c_index;
struct c_node;

struct c_fpoint
{
	c_node *fp;
	int flags;
};

struct c_index
{
	c_node *pt;
	c_index *next;
};

union c_treep
{
	c_node *p;
	int ch;
};

struct c_node
{
	c_fpoint fath;
	c_treep sp[2];
	c_index *top[2];
	long count[2];
};

class CCompactCTool : public CTool
{
//
// original source is
//
// uncompact 4.4 ( 8/11/83 )
//   http://www.ifi.uio.no/~in383/src/compact/
// by Colin L. Mc Master (UCB)
//

public:
	CCompactCTool() : CTool( "Compact “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==strcmp(ext,"C");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	c_fpoint in[258];
	c_index  dir[514], *head, *flist, *dirp, *dirq;
	c_node   dict[258], *bottom;
	long oc;

	void insert(int ch);
	void uptree(int ch);
	void exch( c_node* v,int vs,c_treep x,c_node* w,int ws );
	void encode(int ch);
};

#endif
