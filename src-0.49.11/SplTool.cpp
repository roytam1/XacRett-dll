
#include "stdafx.h"
#include "SplTool.h"

int CSplTool::input_bit(FILE* inp)
{
	int t;
	if( ibitstogo==0 )
	{
		ibuf = getc(inp);
		if( ibuf==EOF )
		{
			igarbits++;
			if( igarbits>16 )
			{
				flagEOF=true;
				return 0;
			}
		}
		ibitstogo = 8;
	}
	t = ibuf & 1;
	ibuf >>= 1;
	ibitstogo--;
	return t;
}

void CSplTool::spl_init()
{
	flagEOF=false;
	igarbits=ibitstogo=0;

	int s;
	for( s=0; s!=64; s++ )
	{
		for( int i=2; i!=516; i++ )
			stree[s].up[i] = (i>>1);
		for( int j=1; j!=258; j++ )
		{
			stree[s].left[j] = (j<<1);
			stree[s].right[j] = (j<<1)+1;
		}
	}

	if( pass!=NULL )
	{
		int k = strlen(pass)-1;
		for( s=0; s!=64; s++ )
		{
			for( int i=k; i>=0; i-- )
			{
				state = s;
				splay(pass[i]);
			}
		}
	}

	state = 0;
}

void CSplTool::splay(int sym)
{
	int a=sym+258,b,c,d;
	spl_tree* spp=&stree[state];

	do
	{
		c = spp->up[a];
		if( c!=1 )
		{
			d = spp->up[c];
			b = spp->left[d];
			if( c==b)
			{
				b = spp->right[d];
				spp->right[d] = a;
			}
			else
				spp->left[d] = a;
			if( a==spp->left[c] )
				spp->left[c] = b;
			else
				spp->right[c] = b;
			spp->up[a] = d;
			spp->up[b] = c;
			a = d;
		}
		else
			a = c;
	}while( a!=1 );

	state = (sym&63);
}

int CSplTool::spl_exp(FILE* inp)
{
	int a=1,sym;
	spl_tree* spp = &stree[state];
	a = 1;

	do
	{
		a = (input_bit(inp)==0) ? spp->left[a] : spp->right[a];
		if( flagEOF )return SPL_EOF_CODE;
	}while( a<=257 );
	sym = a - 258;
	splay(sym);
	return sym;
}

bool CSplTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<7 )
		return false;
	if( hdr[0]!=0x93 || hdr[1]!=0xb9 || hdr[2]!=0x06 )
		return false;

	int i, max = siz > 40 ? 40 : siz;
	for( i=7; i!=max; i++ )
		if( hdr[i]=='\0' )
			break;
	return (i!=max);
}

bool CSplTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	// open spl file
	FILE* spl=fopen(fname,"rb");
	if( spl==NULL )return false;

	pass=NULL;

retry:// なんでgotoかな～ ＞ (^^;

	// read header
	if( 7!=fread(buf,1,7,spl) )
		return false;
	if( buf[0]!=0x93 || buf[1]!=0xb9 || buf[2]!=0x06 )
		return false;
	// orig-size but not use
	DWORD usize = (buf[6]<<24)|(buf[5]<<16)|(buf[4]<<8)|(buf[3]);

	// get original filename
	int c,i;
	for( i=0; i!=33; i++ )
	{
		c = getc(spl);
		if( (buf[i]=c)=='\0' )
			 break;
	}
	if( i==33 ){fclose(spl);return false;}

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr((char*)buf))//追加 By Bonty
	{
		// close
		fclose(spl);
		return true;
	}

	// open new original file
	FILE* org = fopen(kiutil::pathMake((char*)buf),"wb");
	if( org== NULL ){fclose(spl);return false;}

	// decoding
	spl_init();
	int sym=spl_exp(spl);
	while( sym!=SPL_EOF_CODE )
	{
		putc(sym,org);
		sym=spl_exp(spl);
	}

	// close
	DWORD xsize=ftell(org);
	fclose(org);

	// password?
	if( usize!=xsize )
	{
		DeleteFile((char*)buf);
		if( pass==NULL )
		{
			pass=GetPassword();
			rewind(spl);
			goto retry;
		}
	}

	// close
	fclose(spl);
	return true;
}

