#include "stdafx.h"
#include "OpenBga.h"

bool COpenBga::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	int hpos = COpenBga::FindHeader();
	if( hpos == -1 )
		return false;

	fseek( COpenBga::fp , hpos, SEEK_SET );
	return COpenBga::FindSub(lpSubInfo);
}

bool COpenBga::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenBga::FindSub(lpSubInfo);
}
bool COpenBga::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	BgaHeader hdr;

	if (!ReadHeader(&hdr ))
		return false;
	
	fseek( COpenBga::fp, ftell( COpenBga::fp ) + hdr.compressed_size, SEEK_SET );

	lpSubInfo->dwCompressedSize = hdr.compressed_size;
	lpSubInfo->dwOriginalSize = hdr.original_size;

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	if (hdr.attrib & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[4] = 'R';
	if (hdr.attrib & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[2] = 'H';
	if (hdr.attrib & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[3] = 'S';
	if (hdr.attrib & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[1] = 'D';
	if (hdr.attrib & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[0] = 'A';

	//lstrcpy(lpSubInfo->szMode,"");

	lstrcpy(lpSubInfo->szFileName , hdr.name);
	lpSubInfo->wDate = hdr.date;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	lpSubInfo->wTime = hdr.time;

	return true;
}
int COpenBga::FindHeader()
{
	DWORD checksum, fnlen;
	int sum;
	unsigned long siz;

	(filelength(fileno(COpenBga::fp))<=XACR_BUFSIZE) ? siz = filelength(fileno(COpenBga::fp)): siz = XACR_BUFSIZE;

	for( int i=0; i<(signed)siz-28; i++ )
	{
		if( COpenBga::common_buf[i+4]!='G' && COpenBga::common_buf[i+4]!='B' ) continue;
		if( COpenBga::common_buf[i+5]!='Z'                  ) continue;
		if( COpenBga::common_buf[i+6]!='I' && COpenBga::common_buf[i+6]!='2' ) continue;
		if( COpenBga::common_buf[i+7]!='P' && COpenBga::common_buf[i+7]!='\0') continue;

		checksum = (COpenBga::common_buf[i+0])+(COpenBga::common_buf[i+1]<<8)+(COpenBga::common_buf[i+2]<<16)+(COpenBga::common_buf[i+3]<<24);
		fnlen    = (COpenBga::common_buf[i+24])+(COpenBga::common_buf[i+25]<<8)+(COpenBga::common_buf[i+26])+(COpenBga::common_buf[i+27]<<8);

		if( i+28+fnlen > (signed)siz )		 continue;

		sum = 0;
		for( int j=i+4; j!=i+28+(signed)fnlen; j++ )
			sum += (char)COpenBga::common_buf[j];
		if( checksum == (unsigned)sum )
			return i;
	}

	return -1;
}

bool COpenBga::ReadHeader(BgaHeader* hdr )
{
	unsigned char* buf = COpenBga::common_buf;
//--------------------------------------------------------------------//

	if( 28 != fread( buf,1,28,COpenBga::fp ) )
		return false;

	// 一応 Byte-Order-Independent に…する意味あるのかなぁ？(^^;
	hdr->checksum		 = (buf[0])+(buf[1]<<8)+
							(buf[2]<<16)+(buf[3]<<24);
	memcpy( hdr->type, buf+4, 4 );
	hdr->compressed_size = (buf[8])+(buf[9]<<8)+
							(buf[10]<<16)+(buf[11]<<24);
	hdr->original_size	 = (buf[12])+(buf[13]<<8)+
							(buf[14]<<16)+(buf[15]<<24);
	hdr->date			 = (buf[16])+(buf[17]<<8);
	hdr->time			 = (buf[18])+(buf[19]<<8);
	hdr->attrib			 = (buf[20]);
	hdr->header_type	 = (buf[21]);
	hdr->arc_type		 = (buf[22])+(buf[23]<<8);
	hdr->dir_name_len	 = (buf[24])+(buf[25]<<8);
	hdr->file_name_len	 = (buf[26])+(buf[27]<<8);

	// ファイル名( MAX_PATHまでで切ってしまう )
	DWORD fnlen = hdr->dir_name_len + hdr->file_name_len;
	if( fnlen != (unsigned)fread( buf+28, 1, fnlen, COpenBga::fp ) )
		return false;

	int len = fnlen > MAX_PATH-1 ? MAX_PATH-1 : fnlen;
	memcpy( hdr->name, buf+28, len );
	hdr->name[ len ] = '\0';

	// チェックサム
	int sum=0;
	for( unsigned i=4; i!=28+fnlen; i++ )
		sum += (char)buf[i];
	return ( hdr->checksum == (unsigned)sum );
}
