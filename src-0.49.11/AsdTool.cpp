
#include "stdafx.h"
#include "AsdTool.h"

#define incr(x) (x=(x+1)&0xfff)

bool CAsdTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	// 魔法の数字は A S D 0 1 [1a] !
	return ( siz>6 && hdr[0]=='A' && hdr[1]=='S' && hdr[2]=='D'
		&& hdr[3]=='0' && hdr[4]=='1' && hdr[5]==0x1A );
}

bool CAsdTool::Extract( const char* fname,const char* ddir )
{
	unsigned char buf[300];
//--------------------------------------------------------------------//

	// 開く
	FILE* asd = fopen( fname, "rb" );
	if( !asd ) return false;

	// マジックナンバをとばす
	fseek( asd, 6, SEEK_CUR );

	// 格納されているファイル数
	if( 2!=fread( buf, 1, 2, asd ) )
	{
		fclose( asd );
		return false;
	}
	int num_of_files = (buf[0]) + (buf[1]<<8);

	// ヘッダの開始位置
	// （ヘッダをいったん別のメモリに格納した方が速いのは明らかだが、）
	// （面倒なので(^^;、SEEKしまくりで解決する。）
	long header_pos = 8;

	// 圧縮されたデータの開始位置
	for( int t,d=0; d!=num_of_files; d++ )
		if( EOF == (t=fgetc(asd))
		 || (t+14) != (signed)fread( buf, 1, (t+14), asd ) )
		{
			fclose( asd );
			return false;
		}
	init_expander( fgetc( asd ) );
	long data_pos = ftell( asd );

	// 展開開始
	for( int c,i=0; i!=num_of_files; i++ )
	{
		// 次のファイルの情報格納位置へ
		fseek( asd, header_pos, SEEK_SET );

		// ヘッダ解析
		c = fgetc(asd);
		fread( buf, 1, c+14, asd );

		char outname[256];		// ファイル名
		memcpy( outname, buf, c ), outname[c] = '\0';
		unsigned long orisize =	// 元サイズ
			(buf[c+ 0])+(buf[c+ 1]<<8)+(buf[c+ 2]<<16)+(buf[c+ 3]<<24);
		unsigned long cr =		// CRC
			(buf[c+ 4])+(buf[c+ 5]<<8)+(buf[c+ 6]<<16)+(buf[c+ 7]<<24);
		unsigned long uptime =	// 更新日時
			(buf[c+ 8])+(buf[c+ 9]<<8)+(buf[c+10]<<16)+(buf[c+11]<<24);
		unsigned short attr =	// 属性
			(buf[c+12])+(buf[c+13]<<8);

		// ヘッダ位置ポインタを進めておく
		header_pos += (1+c+14);
		
		if (m_Endflg) return true;//追加 By Bonty
		if (!CheckStr(outname))//追加 By Bonty
		{
			unsigned char* buf = CTool::common_buf;
			fseek( asd, data_pos, SEEK_SET );
			while( orisize )//データ部分の飛ばし
			{
				while( orisize && wptr != xptr )
				{
					incr( wptr ), orisize--;
				}
				if( orisize==0 )
					break;
				if( feof(asd) )
					goto finish;

				// 展開。辞書サイズ4096bytes の LZSS。
				unsigned char flag = fgetc(asd);
				for( int sft=8; sft; sft-- )
				{
					if( flag & (1<<(sft-1)) )
					{
						// bit-ON＝符号化されたデータ
						unsigned short code = (fgetc(asd)<<8)+fgetc(asd);
						int fptr = (xptr-1-(code&0xfff))&0xfff;
						int ln = (code>>12)+3;
						if( ln==18 )
							ln += verylong;

						while( ln-- )
						{
							buf[xptr] = buf[fptr];
							incr(xptr), incr(fptr);
						}
					}
					else
					{
						// bit-OFF＝素のまま
						buf[xptr] = fgetc(asd);
						incr(xptr);
					}
				}
			}
			data_pos=ftell( asd );
			continue;
		}

		// 出力先ファイルを開く
		char* cn = kiutil::pathMake( outname );
		FILE* out = fopen( cn, "wb" );

		// 展開処理
		fseek( asd, data_pos, SEEK_SET );
		bool eof = expand_data( asd, out, orisize );
		data_pos = ftell( asd );

		// 閉じて属性設定（面倒なのでCRCチェック無し）
		if( out )
		{
			fclose( out );
			kiutil::timeSet( cn, (unsigned short)(uptime>>16),
								 (unsigned short)(uptime&0xffff) );
			SetFileAttributes( cn, attr );
		}

		if( eof )
			break;
	}
finish:
	fclose( asd );
	return true;
}

void CAsdTool::init_expander( int x )
{
	// LZSSルーチン初期化
	wptr = xptr = 0;
	memset( CTool::common_buf, 0, 4096 );
	verylong = (x&0xff);
}


bool CAsdTool::expand_data( FILE* in, FILE* out, unsigned long len )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	while( len )
	{
		// バッファに残ってる物を出力
		while( len && wptr != xptr )
		{
			if( out )
				fputc( buf[wptr], out );
			incr( wptr ), len--;
		}
		if( len==0 )
			return false;
		if( feof(in) )
			return true;

		// 展開。辞書サイズ4096bytes の LZSS。
		unsigned char flag = fgetc(in);
		for( int sft=8; sft; sft-- )
		{
			if( flag & (1<<(sft-1)) )
			{
				// bit-ON＝符号化されたデータ
				unsigned short code = (fgetc(in)<<8)+fgetc(in);
				int fptr = (xptr-1-(code&0xfff))&0xfff;
				int ln = (code>>12)+3;
				if( ln==18 )
					ln += verylong;

				while( ln-- )
				{
					buf[xptr] = buf[fptr];
					incr(xptr), incr(fptr);
				}
			}
			else
			{
				// bit-OFF＝素のまま
				buf[xptr] = fgetc(in);
				incr(xptr);
			}
		}
	}

	return false;
}
#undef incr
