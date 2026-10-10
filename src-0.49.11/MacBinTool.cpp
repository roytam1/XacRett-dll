
#include "stdafx.h"
#include "MacBinTool.h"

/*==== いろんな資料からのまとめ by K.INABA ====

   byte: 000  0x00
   byte: 001  Filename_len( 1-63 )
    63B: 002  Filename
  dword: 065  Filetype
  dword: 069  Creator
   byte: 073  FinderFlag
   byte: 074  0x00
   word: 075  vpos in window
   word: 077  hpos in window
   word: 079  Id of window/folder
   byte: 081  ProtectedFlag( least-bit 1/0 )
   byte: 082  0x00
  dword: 083  DataFork_len
  dword: 087  RsrcFork_len( 0-0x7FFFFF ) 
  dword: 091  CreateTime( sec ftom 1904/1/1 0:00 AM )
  dword: 095  ModifiedTime( sec ftom 1904/1/1 0:00 AM )

  -----------------MacBinary 1---------------------
     099-125  0x00
   word: 126  OSID & ComputerType
  -------------------------------------------------

  -----------------MacBinary 2&3-------------------
   word: 099  GetInfo_Fork
   byte: 101  FinderFlag2
   ----3only(0x00 for 2)----
  dword: 102  "mBIN"
   byte: 106  script of file name ? 
   byte: 107  FinderFlagEx 
   -------------------------
     108-115  0x00
  dword: 116  total files length ?
   word: 120  2nd Header_len (currently 0x00)
   byte: 122  verMacBin( II:129 III:130 )
   byte: 123  minverMacBin( 129 )
   word: 124  crc of 124byte
   word: 126  undefined
  -------------------------------------------------

   2nd hdr: rounded up to the next multiple of 128
  datafork:
  rsrcfork:
   getinfo:

  ===================================================

  ---- MacBinary 2+ ----

    Almost as same as MacBin2.
	But Offset[ 000 ] is 0x01 for 'Start' and 'End' Block.
     Offset[065-]=='fold' Offset[069-]==0xffffffff(Start) or 0xfffffffe(End)
*/

#define x68Long(_p) (((_p)[0]<<24)+((_p)[1]<<16)+((_p)[2]<<8)+((_p)[3]))
#define x68Word(_p) (((_p)[0]<<8)+((_p)[1]))

static inline bool isAllZero( const unsigned char* st, const unsigned char* ed )
{
	for( ; st!=ed; st++ )
		if( *st )
			return false;
	return true;
}

static unsigned short getCRC( const unsigned char *p, long len, unsigned short crc )
{
	static const unsigned short crctable[] = {
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
	};
	for( int i=0; i<len; i++,p++ )
	{
		crc ^= (*p << 8);
		crc  = (crc << 8) ^ crctable[(unsigned char)(crc>>8)];
	}
	return crc;
}

bool CMacBinTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	// 共通チェック
	if( !easyCheck(hdr) )
		return false;

	// ファイルサイズチェック
	DWORD dtLen = x68Long(hdr+83);
	DWORD rcLen = x68Long(hdr+87);
	if( rcLen>0x800000 || 128+dtLen+rcLen>fsize )
		return false;

	// CRC
	if( hdr[122]!=0 && getCRC( hdr, 124, 0 ) != x68Word(hdr+124) )
		return false;

	return true;
}

bool CMacBinTool::Extract( const char* fname, const char* ddir )
{
	FILE* bin = fopen( fname, "rb" );
	if( !bin ) return false;

	char name[MAX_PATH],path[MAX_PATH]="";
	int offset, dirflag;
	unsigned long time, siz;

	while( doHeader( bin, name, &siz, &offset, &dirflag, &time ) )
	{
		int next = ftell(bin) + offset;

		if( dirflag==1 ) // fold-START
		{
			strcat( path, name );
			strcat( path, "\\" );
			//kiutil::pathMake( path );
		}
		else if( dirflag==2 ) // fold-END
		{
			char *p,*pp,*ppp;
			for( p=path,pp=path,ppp=path; *p; p=CharNext(p) )
				if( *p=='\\' )
					{ppp=pp;pp=p+1;}
			*ppp=0;
		}
		else
		{
			char outname[MAX_PATH], *nm;
			strcpy( outname, path );
			strcat( outname, name );

			if (m_Endflg) return true;//追加 By Bonty
			//ワイルドカード処理
			if (!CheckStr(outname))//追加 By Bonty
			{
				fseek( bin, next, SEEK_SET );
				continue;
			}
			FILE* out = fopen( nm=kiutil::pathMake(outname), "wb" );
			if( out )
			{
				doContent( bin, out, siz );
				fclose(out);
				// セット時間 to nm
			}
		}

		fseek( bin, next, SEEK_SET ); // 次のヘッダへ
	}

	fclose( bin );
	return true;
}

bool CMacBinTool::easyCheck( const unsigned char* hdr )
{
	// 共通チェック
	if( !( 1<=hdr[1] && hdr[1]<=63 ) || (hdr[74]!=0x00) || (hdr[82]!=0x00) )
		return false;
	if( hdr[0] == 0x01 )
	{
		if( 0!=memcmp( hdr+65, "fold", 4 ) || x68Long(hdr+69)<0xfffffffe )
			return false;
	}
	else if( hdr[0] != 0x00 )
		return false;

	// バージョン分岐
	int ver = 1;
		 if( hdr[122]==0   ) ver=1;
	else if( hdr[122]==129 ) ver=2;
	else if( hdr[122]==130 ) ver=3;
	else              return false;
		 if( hdr[0]==1 )     ver=20;
	switch( ver )
	{
	case 1:	if( !isAllZero( hdr+99 ,hdr+126 ) ) return false; break;
	case 2:
	case 20:if( !isAllZero( hdr+102,hdr+116 ) ) return false; break;
	case 3:	if( !isAllZero( hdr+108,hdr+116 ) || 0!=memcmp( hdr+102, "mBIN", 4 ) ) return false; break;
	}

	return true;
}

void CMacBinTool::doContent( FILE* bin, FILE* out, DWORD len )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	while( len )
	{
		long rd = fread( buf, 1, XACR_BUFSIZE>len ? len : XACR_BUFSIZE, bin );
		if( rd<=0 )
			break;
		fwrite( buf, 1, rd, out );
		len -= rd;
	}
}

bool CMacBinTool::doHeader( FILE* bin, char* name, DWORD* siz, int* offset, int* dirflag, DWORD* time )
{
	unsigned char* hdr = CTool::common_buf;
//--------------------------------------------------------------------//

	if( 128 != fread( hdr, 1, 128, bin ) )
		return false;

	// 簡易チェック
	if( !easyCheck( hdr ) )
		return false;

	memcpy( name, hdr+2, 63 ), name[hdr[1]]='\0'; // 名前
	*siz = x68Long(hdr+83);		// データサイズ
	*time = x68Long(hdr+95);	// 更新日時

	// オフセット
	*offset = x68Long(hdr+83) + x68Long(hdr+87) + x68Word(hdr+99);
	int _2ndH = x68Word(hdr+120);
	_2ndH = (_2ndH&0x4f) ? (((_2ndH>>7)+1)<<7) : _2ndH;
	fseek( bin, _2ndH, SEEK_CUR );

	// ディレクトリフラグ
	*dirflag = 0;
	if( hdr[0]==1 )
	{
		if( x68Long(hdr+69)==0xffffffff )
			*dirflag = 1;
		else
			*dirflag = 2;
	}
	return true;
}
