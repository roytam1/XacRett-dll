
#include "stdafx.h"
#include "BelTool.h"

bool CBelTool::Check( const char* fname, unsigned long fsize )
{
	// 拡張子チェック
	if( 0!=stricmp( kiutil::pathExt(fname),"bel" ) )
		return false;

	// 16MBまでしか圧縮できないbelにしては大きすぎたら却下
	return ( fsize<=16777216*2 );
}

bool CBelTool::Extract( const char* fname, const char* ddir )
{
	// 書庫を一挙にバッファ(bf1)にロード
	FILE* bel = fopen( fname,"rb" );
	if( bel==NULL )	return false;

	fseek( bel,0,SEEK_END );
	long fl = ftell( bel );
	fseek( bel,0,SEEK_SET );
	// The header is 8 bytes; anything shorter cannot be decoded and
	// every later field is read from it.
	if( fl<8 || fl>16777216L*2 )
	{
		fclose( bel );
		return false;
	}
	int fs1 = (int)fl;

	BYTE* bf1 = new BYTE[fs1+10];
	if( (int)fread( bf1,1,fs1,bel )!=fs1 )
	{
		delete [] bf1;
		fclose(bel);
		return false;
	}
	fclose(bel);

	// 解凍先バッファ(bf2)確保
	int fs2 = bf1[0]+(bf1[1]<<8)+(bf1[2]<<16);		// LE24
	int doff = bf1[3]+(bf1[4]<<8);				// LE16
	// This is the check the decoder was missing.  doff indexes
	// straight into bf1, and fs2 is a bare 24-bit number, so a
	// corrupt header used to walk p2 arbitrarily far past the
	// allocation (a 14-byte file declaring doff=18814 crashed with
	// 0xC0000005).  fs2==0 also made new BYTE[0] and then wrote
	// bf2[0] unconditionally.  16 MB is the documented limit.
	if( fs2<=0 || fs2>16777216 || doff<8 || doff>=fs1 )
	{
		delete [] bf1;
		return false;
	}
	BYTE* bf1e = bf1+fs1;					// end of input
	BYTE *p1 = bf1+8, *p2 = bf1+doff;
	BYTE* bf2 = new BYTE[fs2];

	// バッファ(bf2)に解凍(ここのアルゴリズム解読しなきゃ...)
	BYTE *p=bf2, a;
	int i=0,j,l,lp;
	// p-bf2 tracks i exactly (both advance by the same amounts),
	// so bounding i bounds the write pointer too.  p1/p2 get their
	// own end-of-input guard.
	while( i<fs2-1 && p1<bf1e && p2<bf1e )
	{
		lp = *(p1++);
		l  = (i+(lp&31)>fs2-1) ? fs2-i-1 : lp&31 ;
		i += l;
		if( l>bf1e-p2 )
			break;					// input ran out mid-copy
		for( j=0; j<l; j++ )
			*(p++) = *(p2++);
		if( lp!=31 )
		{
			if( lp<32 )
			{
				if( p1>=bf1e )
					break;
				l=*(p1++);
			}
			else
				l=lp/32;
			if( l<1 )
				l=1;
			a =  *(p2-1);
			// Clamp the repeat so it can never run past bf2[fs2-1].
			if( l-1>fs2-1-i )
				l=fs2-i;
			i += l-1;
			for( j=0; j<l-1; j++ )
				*(p++)=a;
		}
	}
	if( p>bf2+fs2-1 )
		p=bf2+fs2-1;
	if( p2>=bf1e )
		p2=bf1e-1;
	*p = *p2;

	// 保存ファイルネーム作成
	char outname[MAX_PATH];
	int y,d;
	kiutil::pathSplit( fname,&y,&d );
	strcpy( outname,fname+y+1 );

	if( d==-1 )
		//もう解凍終わってるのでバッファに干渉しちゃう
		bf1[4]='.', bf1[8]='\0', strcat( outname,(char*)bf1+4 );
	else
		d-=(y+1),
		outname[d+1]=bf1[5], outname[d+2]=bf1[6],
		outname[d+3]=bf1[7], outname[d+4]='\0';

	// bf1は用無し
	delete [] bf1;

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(kiutil::pathName(outname)))//追加 By Bonty
	{
		delete [] bf2;
		return true;
	}

	// バッファ(bf2)を保存
	kiutil::pathMake83(outname);
	FILE* out = fopen( outname,"wb" );
	if( !out )
	{
		delete [] bf2;
		return false;
	}
	fwrite( bf2,1,fs2,out );
	fclose( out );

	delete [] bf2;
	return true;
}
