
#include "stdafx.h"
#include "GzTool.h"
#include "zlib/zlib.h"

struct GzHeader
{
	BYTE Magic[2];	// 0x1f, 0x8b
	BYTE Method;	// deflate:0x08 , 他は予約
	BYTE Flag;		// bit0:Text
					// bit1:No2 or 3 or... of MultiPartGzip
					// bit2:特別な領域が存在
					// bit3:ファイル名が存在
					// bit4:コメントが存在
					// bit5:ファイルは暗号化されている
	DWORD Update;	// 最終更新日時。Unix形式。
	BYTE ExFlag;	// 拡張フラグ
	BYTE OSType;	// ファイルを作成したOSの種類(詳細略)

//	7:bit1がセットされているとき、パート番号(0,1,2,...)	2byte又はなし
//	8:bit2がセットされているとき、特別な領域の長さ		2byte又はなし
//	9:特別な領域									?byte又はなし
//	10:bit3がセットされているとき、元のファイル名	?byte又はなし('\0'で終わる)
//	11:bit4がセットされているとき、コメント			?byte又はなし('\0'で終わる)
//	12:bit5がセットされているとき、暗号化ヘッダ		12byte又はなし
};
//	13:圧縮データ						?byte
//	14:32ビットCRC						4byte
//	15:ファイルサイズ&0xffffffff		4byte


bool CGzTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return ( siz>=10 && hdr[0]==0x1f
		&& ( hdr[1]==0x8b || hdr[1]==0x9e ) );
}

bool CGzTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	// ファイル名がもしあれば読み込み
	char outname[MAX_PATH];

	FILE* fp = fopen( fname,"rb" );
	if( !fp )
		return false;

	GzHeader head;
	if( 10 != fread( &head,1,10,fp ) )
	{
		fclose(fp);
		return false;
	}
	if( head.Flag&(1<<3) )
	{
		if( head.Flag&(1<<1) )fgetc(fp),fgetc(fp);
		if( head.Flag&(1<<2) )
		{WORD splen=0;fread(&splen,2,1,fp);fread(buf,1,splen,fp);}
		for( int i=0; i!=65536; i++ )
		{
			int c=fgetc(fp);
			if( c==-1 || c==0 ){buf[i]=0;break;}
			buf[i]=c;
		}
		strncpy( outname,(char*)buf,MAX_PATH );
		kiutil::pathMake(outname);
	}
	else
	{
		kiutil::getOriginalName( outname,fname,
			0==stricmp(kiutil::pathExt(fname),"gz") ? NULL : "ungzip" );
	}
	fclose( fp );
	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(outname))return true;//追加 By Bonty

	gzFile gfp=gzopen( fname,"rb" );
	if( !gfp )return false;

	kiutil::pathMake83(outname);
	FILE* fwp=fopen( outname,"wb" );
	if( fwp )
	{
		size_t s;
		while( s=gzread(gfp,buf,XACR_BUFSIZE) )
		{
			if( s==-1 )
				break;
			fwrite( buf,1,s,fwp );
		}
		fclose(fwp);
	}
	gzclose( gfp );
	return true;
}
