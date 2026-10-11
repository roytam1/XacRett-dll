#include "stdafx.h"
#include "OpenArj.h"


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

bool COpenArj::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	unsigned long siz = fread( COpenArj::common_buf, 1, 50000, COpenArj::fp );
	int skps = FindHeader( COpenArj::common_buf, siz );
	if( -1==skps )
	{
		return false;
	}
	fseek(COpenArj::fp,skps,SEEK_SET );
	if( !ReadHeader() ) // ˆêŒÂ“Ç‚ÝŽÌ‚Ä
		return false;

	return FindSub(lpSubInfo);
}

bool COpenArj::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}

bool COpenArj::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	if (!ReadHeader())
		return false;

	if( !IsValidFlags()	)
	{
		fseek(COpenArj::fp,ahdr.h_Compsize,SEEK_CUR );
		return FindSub(lpSubInfo);
	}
	crc = 0xffffffff;

	lpSubInfo->dwCompressedSize = ahdr.h_Compsize;
	lpSubInfo->dwCRC = ahdr.h_FileCRC;
	lpSubInfo->dwOriginalSize = ahdr.h_Origsize;
	wsprintf(lpSubInfo->szMode,"m%u",ahdr.h_Method);

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	if (ahdr.h_Attrib & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[4] = 'R';
	if (ahdr.h_Attrib & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[2] = 'H';
	if (ahdr.h_Attrib & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[3] = 'S';
	if (ahdr.h_Attrib & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[1] = 'D';
	if (ahdr.h_Attrib & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[0] = 'A';

	lstrcpy(lpSubInfo->szFileName,ahdr.h_Filename);

	lpSubInfo->uOSType = ahdr.h_OS;

	lpSubInfo->wDate = (ahdr.h_TimeStamp & 0xFFFF0000) >> 16;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0)
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));

	lpSubInfo->wTime = (ahdr.h_TimeStamp & 0xFFFF) > 0x7FFF ? (ahdr.h_TimeStamp & 0xFFFF) - 0x10000:ahdr.h_TimeStamp & 0xFFFF;;
	fseek( COpenArj::fp,ahdr.h_Compsize,SEEK_CUR );
	return true;
}

int COpenArj::FindHeader( const unsigned char* hdr, unsigned long siz )
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

bool COpenArj::ReadHeader()
{
	WORD header_id = fget_word(COpenArj::fp);

	if( header_id!=HEADER_ID )
		return false;

	ahdr.h_Size = fget_word(COpenArj::fp);
    if( ahdr.h_Size==0 )
		return false;
    if( ahdr.h_Size>HEADERSIZE_MAX )
		return false;

	BYTE header[HEADERSIZE_MAX],*ph=header;
    crc = CRC_MASK;
    fread_crc( header,ahdr.h_Size,COpenArj::fp );
	ahdr.h_CRC = fget_crc(COpenArj::fp);
	if( (crc^CRC_MASK)!=ahdr.h_CRC )
		return false;

	ahdr.h_1stSize = *(ph++);
	ahdr.h_ArjVer = *(ph++);
	ahdr.h_ArjXVer = *(ph++);
	ahdr.h_OS = *(ph++);
	ahdr.h_Flags = *(ph++);
	ahdr.h_Method = *(ph++);
	ahdr.h_FileType = *(ph++);
	(ph++);
	ahdr.h_TimeStamp = (ph[3]<<24)+(ph[2]<<16)+(ph[1]<<8)+ph[0];	ph+=4;
	ahdr.h_Compsize = (ph[3]<<24)+(ph[2]<<16)+(ph[1]<<8)+ph[0];	ph+=4;
	ahdr.h_Origsize = (ph[3]<<24)+(ph[2]<<16)+(ph[1]<<8)+ph[0];	ph+=4;
	ahdr.h_FileCRC = (ph[3]<<24)+(ph[2]<<16)+(ph[1]<<8)+ph[0];	ph+=4;
	ahdr.h_EntryPos = (ph[1]<<8)+ph[0];	ph+=2;
	ahdr.h_Attrib = (ph[1]<<8)+ph[0];	ph+=2;;
	ahdr.h_HostData = (ph[1]<<8)+ph[0];	ph+=2;;
	
	lstrcpyn( ahdr.h_Filename,(char*)&header[ahdr.h_1stSize],ARJ_FNAME_MAX );
	/*if( ahdr.h_OS != 0 && ahdr.h_OS != 10 )//MSDOS/WIN95‚¶‚á‚È‚¯‚ê‚ÎASCIIˆÈŠO’Ê‚³‚È‚¢B
	{
		char* pp=ahdr.h_Filename;
		while(*pp)
			*(pp++)&=0x7f;
	}*/

	// Šg’£ƒwƒbƒ_“Ç‚ÝŽÌ‚Ä
	WORD extheadersize;
	while( extheadersize=fget_word(COpenArj::fp) )
		fseek( COpenArj::fp,extheadersize+4,SEEK_CUR );

	return true;
}

size_t COpenArj::fread_crc(void* p,size_t n,FILE* f)
{
	n = fread(p,1,n,f);
	ahdr.h_Origsize += n;
	UpdateCRC( (BYTE*)p,n );
	return n;
}

bool COpenArj::IsValidFlags()
{
	if( ahdr.h_ArjXVer>ARJ_X_VERSION )
		return false;
	if( (ahdr.h_Flags&0x01)!=0 )
		return false;
	if( ahdr.h_Method<0 || ahdr.h_Method>4 || (ahdr.h_Method==4 && ahdr.h_ArjVer==1) )
		return false;
	if( ahdr.h_FileType!=BINARY_TYPE && ahdr.h_FileType!=TEXT_TYPE )
		return false;
	return true;
}
