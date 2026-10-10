
#include "stdafx.h"
#include "LzhTool.h"
#include "LzhDecoder2.h"

bool CLzhTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return (-1!=FindHeader(fname,hdr,siz>300*1024?300*1024:siz));
}

bool CLzhTool::Extract( const char* fname,const char* ddir )
{
	// 先頭の方を読み込み
	FILE* fp = fopen( fname,"rb" );
	if( !fp )
		return false;
	DWORD siz = fread( CTool::common_buf, 1, XACR_BUFSIZE, fp );
	fclose( fp );
	// ヘッダを探す
	int ps = FindHeader( fname,CTool::common_buf,siz );
	if( ps==-1 )
		return false;

	// 書庫を開く
	if( !(lzh = fopen( fname,"rb" )) )
		return false;
	fseek( lzh,ps,SEEK_SET );

	// 格納ファイル毎に処理
	while( ReadHeader( CTool::common_buf ) )
	{
		long base = ftell( lzh );
		char* name;
		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(h_FileName))//追加 By Bonty
		{
			fseek( lzh, base+h_CompSize, SEEK_SET );
			continue;
		}

		if( out=fopen( name=kiutil::pathMake(h_FileName),"wb") )
		{
			// デコード
			CLzhDecoder2 dec;
			lzh_method mhd = UNKNOWN;
				 if( 0==strcmp(h_Method,"-lh0-") )mhd=LH0;
			else if( 0==strcmp(h_Method,"-lh1-") )mhd=LH1;
			else if( 0==strcmp(h_Method,"-lh2-") )mhd=LH2;
			else if( 0==strcmp(h_Method,"-lh3-") )mhd=LH3;
			else if( 0==strcmp(h_Method,"-lh5-") )mhd=LH5;
			else if( 0==strcmp(h_Method,"-lh6-") )mhd=LH6;
			else if( 0==strcmp(h_Method,"-lh7-") )mhd=LH7;
			else if( 0==strcmp(h_Method,"-lzs-") )mhd=LZS;
			else if( 0==strcmp(h_Method,"-lz4-") )mhd=LZ4;
			else if( 0==strcmp(h_Method,"-lz5-") )mhd=LZ5;
			dec.Decode( mhd, lzh, h_CompSize, out, h_OrigSize );

			// 属性など設定
			fclose( out );

			SetFileAttributes( name,h_Attrib );
			if( h_Level<2 )
				kiutil::timeSet(name,(WORD)(h_Update>>16),(WORD)h_Update);
			else
				kiutil::timeSet( name, h_Update );
		}

		fseek( lzh, base+h_CompSize, SEEK_SET );
	}

    fclose( lzh );
	return true;
}

int CLzhTool::FindHeader( const char* fname, const BYTE* hdr, DWORD siz )
{
	bool bopen = false;
	BYTE* temp;
	int ans=-1;
	
	if (siz < 20)
		return -1;
	for( DWORD i=0; i<siz-20; i++ )
	{
		if( hdr[i+2]=='-' && hdr[i+3]=='l' &&
		   (hdr[i+4]=='h' || hdr[i+4]=='z') )
		{
			if( !bopen )
				if( !(lzh=fopen( fname,"rb" )) )
					return -1;
				else
					temp = new BYTE[65536];

			fseek( lzh,i,SEEK_SET );
			if( ReadHeader(temp) )
				{ans=(signed)i;break;}
		}
	}

	if( bopen )
		{delete [] temp;fclose( lzh );}
	return ans;
}

bool CLzhTool::ReadHeader( unsigned char* buf )
{
// 初期化
	*h_FileName=0;
	sum=0,crc=0;

// だいたい共通な部分
//
//  0- 1: [0]=header_size [1]=check_sum (h0)
//        [0]=bas_hdr_siz [1]=check_sum (h1)
//        [WORD] = all_header_size      (h2)
//
//  2- 6: method
//
//  7-10: [DWORD]=compressed_size (h0/h2)
//        [DWORD]=offset_to_next_hdr (h1)
//
// 11-14: [DWORD]=original_size
//
// 15-18: [ftime]=update_date_time
//
//    19: attribute (h0)
//        0x20      (h1/h2)
//
//    20: header_level
//
	if( 21!=fread_crc(buf,21) )
		return false;
	sum-=(buf[0]+buf[1]);

	if( (h_Level=buf[20])>2 ) // LV3以上未対応
		return false;

	BYTE bshdr = (h_Level==2)?26:buf[0]+2;
	if( bshdr<21 || buf[0]==0 )
		return false;
	BYTE hdrsum= buf[1];
	for( int i=0; i!=5; i++ )
		h_Method[i]=(char)buf[2+i];h_Method[5]=0;
	h_CompSize = (buf[ 7])+(buf[ 8]<<8)+(buf[ 9]<<16)+(buf[10]<<24);
	h_OrigSize = (buf[11])+(buf[12]<<8)+(buf[13]<<16)+(buf[14]<<24);
	h_Update   = (buf[15])+(buf[16]<<8)+(buf[17]<<16)+(buf[18]<<24);
	h_Attrib   =  buf[19];
	if( h_Method[0]!='-' || h_Method[1]!='l' )
		return false;

// レベル共通じゃない基本ヘッダ部分
// <h0>
//     21: file_name_length
//  22- n: file_name ( with path info )
//   WORD: crc16_of_file
//       : and some extension part
//
// <h1>
//     21: file_name_length
//  22- n: file_name ( with no path info )
//   WORD: crc16_of_file
//   BYTE: os_flag
//       : and some extension part
//
// <h2>
//   WORD: crc16_of_file
//   BYTE: os_flag
//
//
	if( (bshdr-21)!=fread_crc(buf+21,(bshdr-21)) )
		return false;
	if( h_Level!=2 )
	{
		if( sum!=hdrsum || 21+1+buf[21]+2>bshdr )
			return false;
		memcpy( h_FileName,buf+22,buf[21] );
		h_FileName[buf[21]]=0;
	}

// 拡張ヘッダ( h1/h2 )
//  repeating of such blocks.
//------------------------------------------------------
//  WORD: size_of_this_block ( if 0 then end_of_header )
//  BYTE: type_flag
//      : data ( (blocksize-3) bytes )
//------------------------------------------------------

	char PathName[MAX_PATH*2]={0};
	if( h_Level!=0 )
	{
		DWORD hdrcrc=0xffffffff;
		WORD tmpcrc;

		// ここでループって拡張ヘッダ読み込み
		WORD ehs;
		ehs = ((buf[bshdr-2])|(buf[bshdr-1]<<8));

		while( ehs>2 )
		{
			tmpcrc=crc;//CRC自身を読み込んだときに直すため
			if( ehs!=fread_crc(buf,ehs) )
				return false;
			if( h_Level==1 )
				h_CompSize-=ehs;

			switch( *buf )
			{
			case 0x00://共通
				if( ehs>=5 && hdrcrc==0xffffffff )
				{
					hdrcrc=((buf[1])|(buf[2]<<8));

					crc=tmpcrc;
					buf[1]=buf[2]=0;
					UpdateCRC(buf,ehs);
				}
				break;
			case 0x01://ファイル名
				memcpy(h_FileName,buf+1,ehs-3>MAX_PATH?MAX_PATH:ehs-3);
				h_FileName[ehs-3>MAX_PATH?MAX_PATH:ehs-3]=0;
				break;
			case 0x02://パス名
				memcpy(PathName,buf+1,ehs-3>MAX_PATH?MAX_PATH:ehs-3);
				PathName[ehs-3>MAX_PATH?MAX_PATH:ehs-3]=0;
				break;
			case 0xff://新属性
				break;
			case 0x40://属性(DOS依存)
				if( ehs>=5 )
					h_Attrib=((buf[1])|(buf[2]<<8));
				break;
			case 0x41://タイムスタンプ(UNLHA32.DLL)
				//は、なんだかうまくいかないので無視する。
				break;
			}

			ehs = ((buf[ehs-2])|(buf[ehs-1]<<8));
		}

		if( hdrcrc!=0xffffffff && crc!=hdrcrc )
			return false;
	}

	// ff -> \\ 変換
	char* x;
	for( x=h_FileName; *x!=0; x=CharNext(x) )
		if( (BYTE)*x==0xff )
			*x='\\';
	for( x=PathName; *x!=0; x=CharNext(x) )
		if( (BYTE)*x==0xff )
			*x='\\';
	strcat( PathName,h_FileName );
	strcpy( h_FileName,PathName );
	return true;

// lha header MEMO
// (h0/h1)
//   1 byte 目は、「1byte目自身を除いた基本ヘッダのサイズ」
//   チェックサムは、「1,2byte目自身を除いた基本ヘッダの合計」
// (h1)
//   offset_to_next_hdr は、基本ヘッダ末端から次のヘッダへの距離
// (h1/h2)
//   crcは自分自身は0000として他全部から算出
}
