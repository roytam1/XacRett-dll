
#include "stdafx.h"
#include "ZacTool.h"

//解析結果
//
// FF 00 26    | MN-Arc?
// 01/02       | Ver
// -------------------------------
// D1          | MN-File?
// 31 bytes    | FName
// DWORD-intel | FLen
// FLen bytes  | FContent
// (WORD-intel)| CheckSum of (D1...data) / Ver2 only!
// -------------------------------
// ED          | MN-EndOfData?

bool CZacTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<43 || hdr[0]!=0xff || hdr[1]!=0x00 || hdr[2]!=0x26 || !(hdr[3]==0x01 || hdr[3]==0x02) && hdr[4]!=0xd1 )
		return false;
	return true; // 手抜き(^^;
}

bool CZacTool::Extract( const char* fname, const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	FILE* zac = fopen( fname, "rb" );
	if( !zac )
		return false;
	if( 4!=fread( buf, 1, 4, zac ) )
	{
		fclose( zac );
		return false;
	}
	bool zVer2 = (buf[3]==2);

	while( 36==fread( buf, 1, 36, zac ) )
	{
		if( buf[0]!=0xd1 )
			break;

		unsigned long  zLen = buf[32] + (buf[33]<<8) + (buf[34]<<16) + (buf[35]<<24);

		buf[32] = '\0';
		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr((char*)buf+1))//追加 By Bonty
		{
			if( zVer2 )
				fseek( zac, 2, SEEK_CUR ); // チェックサム読捨
			fseek( zac, zLen, SEEK_CUR );
			continue;
		}
		FILE* out = fopen( kiutil::pathMake((char*)buf+1), "wb" );
		if( !out )
		{
			if( zVer2 )
				fseek( zac, 2, SEEK_CUR ); // チェックサム読捨
			fseek( zac, zLen, SEEK_CUR );
		}
		else
		{
			while( zLen )
			{
				unsigned long ln = (zLen > XACR_BUFSIZE ? XACR_BUFSIZE : zLen);
				unsigned long rd = fread( buf, 1, ln, zac );
				if( rd<=0 )
					break;
				fwrite( buf, 1, rd, out );
				zLen -= rd;
			}

			fclose( out );
		}

		if( zVer2 )
			fseek( zac, 2, SEEK_CUR ); // チェックサム読捨
	}
	return true;
}

//static unsigned short checksum( unsigned short val, unsigned char* dat, int len )
//{
//	for( int i=0; i<len; i++ )
//		val += dat[i];
//}
