
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
	int fs1 = ftell( bel );
	fseek( bel,0,SEEK_SET );

	BYTE* bf1 = new BYTE[fs1+10];
	fread( bf1,1,fs1,bel );
	fclose(bel);

	// 解凍先バッファ(bf2)確保
	int fs2 = bf1[0]+(bf1[1]<<8)+(bf1[2]<<16);
	BYTE *p1 = bf1+8, *p2 = bf1+bf1[3]+(bf1[4]<<8);
	BYTE* bf2 = new BYTE[fs2];

	// バッファ(bf2)に解凍(ここのアルゴリズム解読しなきゃ...)
	BYTE *p=bf2, a;
	int i=0,j,l,lp;
	while( i<fs2-1 )
	{
		lp = *(p1++);
		l  = (i+(lp&31)>fs2-1) ? fs2-i-1 : lp&31 ;
		i += l;
		for( j=0; j<l; j++ )
			*(p++) = *(p2++);
		if( lp!=31 )
		{
			if( lp<32 )
				l=*(p1++);
			else
				l=lp/32;
			a =  *(p2-1);
			i += l-1;
			for( j=0; j<l-1; j++ )
				*(p++)=a;
		}
	}
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
