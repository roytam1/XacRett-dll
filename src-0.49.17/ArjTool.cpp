
#include "stdafx.h"
#include "ArjTool.h"
#include "LzhDecoder2.h"

bool CArjTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return (-1!=FindHeader( hdr,siz ));
}

bool CArjTool::Extract( const char* fname,const char* ddir )
{
	// 書庫オープン
	if( !(arj = fopen( fname,"rb" )) )
		return false;
	// ヘッダ探し
	unsigned long siz = fread( CTool::common_buf, 1, 50000, arj );
	int skps = FindHeader( CTool::common_buf, siz );
	if( -1==skps )
	{
		fclose( arj );
		return false;
	}
	fseek( arj,skps,SEEK_SET );
	if( !ReadHeader() ) // 一個読み捨て
	{
		fclose( arj );
		return false;
	}

	// 各格納ファイルを処理
	while( ReadHeader() )
	{
		char* name;
		CLzhDecoder2 lzh;

		// 正常なヘッダかどうか検証 & 出力先ファイル作成
		if( !IsValidFlags()	)
		{
		    fseek( arj,h_Compsize,SEEK_CUR );
			continue;
		}
		
		if (m_Endflg) return true;//追加 By Bonty
		if (!CheckStr(h_Filename))//追加 By Bonty
		{
			fseek( arj,h_Compsize,SEEK_CUR );
			continue;
		}

		out = fopen( name=kiutil::pathMake(h_Filename),
								(h_FileType&1)?"w":"wb" );
		if( !out )
		{
		    fseek( arj,h_Compsize,SEEK_CUR );
			continue;
		}

		// 展開
		crc = 0xffffffff;
		switch( h_Method )
		{
		case 0:
			lzh.Decode( LH0,arj,h_Compsize,out,h_Origsize );
			break;
		case 1:
		case 2:
		case 3:
			lzh.Decode( ARJ,arj,h_Compsize,out,h_Origsize);
			break;
		case 4:
			Decode_f();
			break;
		}

		// 後処理
		fclose( out );

		kiutil::timeSet(name,(WORD)(h_TimeStamp>>16),(WORD)h_TimeStamp);
		SetFileAttributes( name,h_Attrib );
	}

    fclose( arj );
	return true;
}

/**********************************************************/

static BYTE fget_byte(FILE* f)
{
	return getc(f);//    error(M_CANTREAD, "");
}
static WORD fget_word(FILE* f)
{
	WORD b0=fget_byte(f);
	WORD b1=fget_byte(f);
	return (b1<<8)+b0;
}
#define fget_crc fget_longword
static DWORD fget_longword(FILE* f)
{
	DWORD b0=fget_byte(f);
	DWORD b1=fget_byte(f);
	DWORD b2=fget_byte(f);
	DWORD b3=fget_byte(f);
	return (b3<<24)+(b2<<16)+(b1<<8)+b0;
}

/**************************************************************/

////////////////////
#define ARJ_X_VERSION     3

#define BINARY_TYPE       0
#define TEXT_TYPE         1

#define COMMENT_MAX    2048
#define FIRST_HDR_SIZE   30
#define HEADERSIZE_MAX (FIRST_HDR_SIZE + 10 + ARJ_FNAME_MAX + COMMENT_MAX)

#define CRC_MASK 0xffffffff

#define HEADER_ID     0xEA60
#define HEADER_ID_HI    0xEA
#define HEADER_ID_LO    0x60
/////////////////////

size_t CArjTool::fread_crc(void* p,size_t n,FILE* f)
{
	n = fread(p,1,n,f);
	h_Origsize += n;
	UpdateCRC( (BYTE*)p,n );
	return n;
}

void CArjTool::fwrite_crc(BYTE* p,int n)
{
	UpdateCRC(p,n);
	if( h_FileType==TEXT_TYPE )
	{
		while( --n )
			putc(p[n],out);
	}
	else
		fwrite( p,1,n,out );
}

int CArjTool::FindHeader( const unsigned char* hdr, unsigned long siz )
{
	long last = ( siz-2-HEADERSIZE_MAX > 50000 ?
					50000 : siz-2-HEADERSIZE_MAX );

	for( long i=0; i<last; i++ )
	{
		// Flag
		if( hdr[i] != HEADER_ID_LO )
			continue;
		if( hdr[i+1] != HEADER_ID_HI )
			continue;
		// Header_size
		int hsiz = (hdr[i+2] + (hdr[i+3]<<8));
		if( hsiz > HEADERSIZE_MAX )
			continue;
		// Crc
		crc = CRC_MASK;
		UpdateCRC( hdr+(i+4),hsiz );
		DWORD hcrc = (hdr[i+4+hsiz+0]
				   + (hdr[i+4+hsiz+1]<<8)
				   + (hdr[i+4+hsiz+2]<<16)
				   + (hdr[i+4+hsiz+3]<<24));
		if( (crc^CRC_MASK) == hcrc )
			return i;
    }
    return -1;
}

bool CArjTool::ReadHeader()
{
	WORD header_id = fget_word(arj);
	if( header_id!=HEADER_ID )
		return false;

	h_Size = fget_word(arj);
    if( h_Size==0 )
		return false;
    if( h_Size>HEADERSIZE_MAX )
		return false;

	BYTE header[HEADERSIZE_MAX],*ph=header;
    crc = CRC_MASK;
    fread_crc( header,h_Size,arj );
	h_CRC = fget_crc(arj);
	if( (crc^CRC_MASK)!=h_CRC )
		return false;

	h_1stSize = *(ph++);
	h_ArjVer = *(ph++);
	h_ArjXVer = *(ph++);
	h_OS = *(ph++);
	h_Flags = *(ph++);
	h_Method = *(ph++);
	h_FileType = *(ph++);
	(ph++);
	h_TimeStamp = (ph[3]<<24)+(ph[2]<<16)+(ph[1]<<8)+ph[0];	ph+=4;
	h_Compsize = (ph[3]<<24)+(ph[2]<<16)+(ph[1]<<8)+ph[0];	ph+=4;
	h_Origsize = (ph[3]<<24)+(ph[2]<<16)+(ph[1]<<8)+ph[0];	ph+=4;
	h_FileCRC = (ph[3]<<24)+(ph[2]<<16)+(ph[1]<<8)+ph[0];	ph+=4;
	h_EntryPos = (ph[1]<<8)+ph[0];	ph+=2;
	h_Attrib = (ph[1]<<8)+ph[0];	ph+=2;;
	h_HostData = (ph[1]<<8)+ph[0];	ph+=2;;
	
	lstrcpyn( h_Filename,(char*)&header[h_1stSize],ARJ_FNAME_MAX );
	if( h_OS != 0 && h_OS < 10 )//MSDOS/WIN95じゃなければASCII以外通さない。
	{
		char* pp=h_Filename;
		while(*pp)
			*(pp++)&=0x7f;
	}

	// 拡張ヘッダ読み捨て
	WORD extheadersize;
	while( extheadersize=fget_word(arj) )
		fseek( arj,extheadersize+4,SEEK_CUR );

	return true;
}

bool CArjTool::IsValidFlags()
{
	if( h_ArjXVer>ARJ_X_VERSION )
		return false;
	if( (h_Flags&0x01)!=0 )
		return false;
	if( h_Method<0 || h_Method>4 || (h_Method==4 && h_ArjVer==1) )
		return false;
	if( h_FileType!=BINARY_TYPE && h_FileType!=TEXT_TYPE )
		return false;
	return true;
}

/************************************************************************/

void CArjTool::init_getbits()
{
	bitbuf = 0;
	subbitbuf = 0;
	bitcount = 0;
	fillbuf(16);
}

void CArjTool::fillbuf(int n)
{
	bitbuf = (bitbuf<<n)&0xFFFF;
	while( n>bitcount )
	{
		bitbuf |= subbitbuf << (n -= bitcount);
		if( h_Compsize!=0 )
		{
			h_Compsize--;
			subbitbuf = (BYTE)getc(arj);
		}
		else
			subbitbuf = 0;
		bitcount = 8;
	}
	bitbuf |= subbitbuf >> (bitcount -= n);
}

#define CODE_BIT 16
#define ARJ_DICSIZE 26624
#define THRESHOLD    3
#define STRTP          9
#define STOPP         13
#define STRTL          0
#define STOPL          7

#define BFIL {getbuf|=bitbuf>>getlen;fillbuf(CODE_BIT-getlen);getlen=CODE_BIT;}
#define GETBIT(c) {if(getlen<=0)BFIL c=(getbuf&0x8000)!=0;getbuf<<=1;getlen--;}
#define BPUL(l) {getbuf<<=l;getlen-=l;}
#define GETBITS(c,l) {if(getlen<l)BFIL c=(WORD)getbuf>>(CODE_BIT-l);BPUL(l)}

short CArjTool::decode_ptr()
{
	short c,width,plus,pwr;

	plus = 0;
	pwr = 1 << (STRTP);
	for( width=(STRTP); width<(STOPP); width++ )
	{
		GETBIT(c);
		if( c==0 )
			break;
		plus += pwr;
		pwr <<= 1;
	}
	if( width!=0 )
		GETBITS(c, width);
	c += plus;
	return c;
}

short CArjTool::decode_len()
{
	short c,width,plus,pwr;

	plus = 0;
	pwr = 1 << (STRTL);
	for( width=(STRTL); width<(STOPL); width++ )
	{
		GETBIT(c);
		if( c==0 )
			break;
		plus += pwr;
		pwr <<= 1;
	}
	if( width!=0 )
		GETBITS(c, width);
	c += plus;
	return c;
}

void CArjTool::Decode_f()
{
	BYTE* text = new BYTE[ARJ_DICSIZE];

	init_getbits();
	getlen = getbuf = 0;

	short pos,i,j,c,r=0;
	DWORD count=0;
	while( count<h_Origsize )
	{
		c = decode_len();
		if( c==0 )
		{
			GETBITS(c, 8);
			text[r] = (BYTE)c;
			count++;
			if( ++r>=ARJ_DICSIZE )
			{
				r=0;
				fwrite_crc(text,ARJ_DICSIZE);
			}
		}
		else
		{
			j = c - 1 + THRESHOLD;
			count += j;
			pos = decode_ptr();
			if( (i=r-pos-1)<0 )
				i += ARJ_DICSIZE;
            while( j-->0 )
			{
				text[r] = text[i];
				if( ++r>=ARJ_DICSIZE )
				{
					r = 0;
					fwrite_crc(text,ARJ_DICSIZE);
				}
				if( ++i>=ARJ_DICSIZE )
					i = 0;
			}
		}
	}
	if( r!=0 )
		fwrite_crc(text,r);

	delete [] text;
}
