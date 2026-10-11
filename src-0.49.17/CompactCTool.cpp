
#include "stdafx.h"
#include "CompactCTool.h"

#define LLEAF 010
#define RLEAF 04
#define SEEN 02
#define FBIT 01
#define EF 0400
#define C_NC 0401

bool CCompactCTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return ( siz>2 && hdr[0]==0xff && hdr[1]==0x1f ); 
}

bool CCompactCTool::Extract( const char* fname,const char* ddir )
{
	FILE *fp,*op;
	if( !(fp=fopen(fname,"rb")) )return false;

	// ヘッダチェックは無し
	int c=fgetc(fp),d; c=fgetc(fp);
	if( c==EOF ){fclose(fp);return false;}

	
	char outname[MAX_PATH];
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"C") ? NULL : "uncompact" );

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(outname)){fclose(fp);return true;}//追加 By Bonty

	// 出力先を開く
	if( !(op=fopen(outname,"wb")) ){fclose(fp);return false;}

	// 展開開始
	c_node *p;
	int m,j,ic;
	char b;

	dir[513].next = NULL;
	for( head=dir+(j=513); j--; )
	{
		dirp = head--;
		head->next = dirp;
	}
	bottom = dirp->pt = dict;
	dict[0].top[0] = dict[0].top[1] = dirp;
	dirq = dirp->next;
	in[EF].flags = FBIT | SEEN;

	ic = oc = 0;
	(bottom->top[1])->next = flist;
	bottom->top[1] = dirp;
	flist = dirq;

	if( EOF!=(c=fgetc(fp)) )
	{
		fputc( c,op );

		ic = 3;

		in[C_NC].fp = in[EF].fp
					= dict[0].sp[0].p
					= bottom			= dict+1;

		bottom->count[0] = bottom->count[1]
						 = dict[0].count[1] = 1;

		dirp->next = dict[0].top[1]
				   = bottom->top[0]
				   = bottom->top[1]
				   = dirq				= flist;	flist=flist->next;

		dirq->pt   = bottom->fath.fp
				   = in[c].fp			= dict;

		dirq->next       = NULL;
		dict[0].fath.fp  = NULL;
		in[c].flags      = (FBIT | SEEN);
		in[C_NC].flags     = SEEN;
		dict[0].fath.flags = RLEAF;
		bottom->fath.flags = (LLEAF | RLEAF);
		dict[0].count [0]  = 2;

		dict[0].sp[1].ch = c;
		bottom->sp[0].ch = C_NC;
		bottom->sp[1].ch = EF;

		p = dict;
		while( EOF!=(c=fgetc(fp)) )
		{
			ic++;
			for( m=0200; m; )
			{
				b = (m&c ? 1 : 0);
				m >>= 1;
				if( p->fath.flags & (b ? RLEAF : LLEAF) )
				{
					d = p->sp[b].ch;
					if( d==EF )
						break;
					if( d==C_NC )
					{
						uptree(C_NC);
						d = 0;
						for( j=8; j--; m>>=1 )
						{
							if( m==0 )
							{
								c = fgetc(fp);
								ic++;
								m = 0200;
							}
							d <<= 1;
							if( m & c )
								d++;
						}
						insert( d );
					}
					uptree( d );
					fputc( d,op );
					oc++;
					p = dict;
				}
				else
					p = p->sp[b].p;
			}
		}
	}

	fclose(fp),fclose(op);
	return true;
}

void CCompactCTool::insert(int ch)
{
	c_node *pp;
	int c,d;
	c_index *qt, *wt;

	c = ch;
	wt = flist; flist = flist->next;
	pp = bottom++;
	bottom->fath.fp = pp;
	in[c].flags = (SEEN | FBIT);
	d = bottom->sp[0].ch = pp->sp[1].ch;
	in[c].fp = in[d].fp = pp->sp[1].p = wt->pt = bottom;
	bottom->fath.flags = (LLEAF | RLEAF | FBIT);
	pp->fath.flags &= ~RLEAF;
	in[d].flags = SEEN;

	bottom->count[0] = pp->count[1];
	qt = pp->top[1];
	bottom->top[0] = qt;
	bottom->sp[1].ch = c;
	bottom->count[1] = 0;
	bottom->top[1] = qt->next = wt;
	wt->next = NULL;
}

void CCompactCTool::uptree(int ch)
{
	c_node *r;
	c_treep q, s;
	int rs, qs, ss, ts;
	long rc, qc, sc;
	c_node *t;
	c_index *rt, *qt, *st;

	r = in [ch] . fp;
	rs = in [ch] . flags & FBIT;

	do {
		(r -> count [rs])++;
		rc = r -> count [rs];
		rt = r -> top [rs];
	
		for ( ; ; ) {
			qs = ss = 1 - rs;
			s . p = r + rs;
			sc = (s . p) -> count [ss];
			st = (s . p) -> top [ss];
	
			if (rs)
				if (r == bottom) {
					sc = rc - 2;
					st = NULL;
				}
				else;
			else if (r == dict) {
				qc = rc + 1;
				qt = head;
				break;
			}

			q . p = r - qs;
			qc = (q . p) -> count [qs];
			qt = (q . p) -> top [qs];
			if (rc <= qc) break;

			t = qt -> pt;
			ts = (rc <= t -> count [0] ? 1 : 0);

			/* exchange pointers of (t, ts) and (r, rs) */

			q . ch = t -> sp [ts] . ch;
			s . ch = r -> sp [rs] . ch;
			t -> sp [ts] . ch = s . ch;
			r -> sp [rs] . ch = q . ch;
			exch (t, ts, q, r, rs);
			exch (r, rs, s, t, ts);

			qs = (rs ? RLEAF : LLEAF);
			ss = (ts ? RLEAF : LLEAF);
			if( ((r->fath.flags&qs)<<rs)^((t->fath.flags&ss)<<ts) )
			{
				r -> fath . flags ^= qs;
				t -> fath . flags ^= ss;
			}

			(t -> count [ts])++;
			(r -> count [rs])--;
			(qt -> pt) += ts;
			r = t;
			rs = ts;
		}

		if (rc == qc) {
			r -> top [rs] = qt;
			if (rc > sc + 1) {
				qt -> next = st;

				/* dispose of rt */

				rt -> next = flist;
				flist = rt;
			}
			else st -> pt = s . p;
		}

		else if (rc == sc + 1) {

			/* create new index at rt */

			rt = flist; flist = flist->next;
			rt -> next = st;
			rt -> pt = r;
			qt -> next = rt;
			if (st) st -> pt = s . p;
			r -> top [rs] = rt;
		}

		rs = r -> fath . flags & FBIT;
		r = r -> fath . fp;

	} while (r);
	dirp = head -> next;
	dirq = dirp -> next;
}

void CCompactCTool::exch( c_node* v,int vs,c_treep x,c_node*  w,int ws )
{
	if( v->fath.flags & (vs ? RLEAF : LLEAF) )
	{
		in[ x.ch ].fp = w;
		in[ x.ch ].flags &= ~01;
		if(ws) in[ x.ch ].flags |= ws;
	}
	else
	{
		(x.p)->fath.fp = w;
		(x.p)->fath.flags &= ~01;
		if(ws) (x.p)->fath.flags |= ws;
	}
}
