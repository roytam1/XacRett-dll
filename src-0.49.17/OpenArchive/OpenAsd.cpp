#include "stdafx.h"
#include "OpenAsd.h"

bool COpenAsd::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char buf[300];

	// the version byte follows the "ASD" magic: '1' = 0.1.x,
	// '2' = 0.2.x
	unsigned char hd[6];
	if( 6!=fread( hd, 1, 6, COpenAsd::fp ) )
	{
		return false;
	}
	m_ver = hd[4];

	// 格納されているファイル数
	if( '2'==m_ver )
	{
		if( 3!=fread( buf, 1, 3, COpenAsd::fp ) )
		{
			return false;
		}
		num_of_files = (buf[0]) + (buf[1]<<8) + (buf[2]<<16);
		m_hdrpos = 9;
	}
	else
	{
		if( 2!=fread( buf, 1, 2, COpenAsd::fp ) )
		{
			return false;
		}
		num_of_files = (buf[0]) + (buf[1]<<8);
		m_hdrpos = 8;
	}
	m_idx = 0;

	return COpenAsd::FindSub(lpSubInfo);
}

bool COpenAsd::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenAsd::FindSub(lpSubInfo);
}
bool COpenAsd::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char buf[300];

	if (m_idx++==num_of_files)
	{
		m_idx=0;
		m_hdrpos = ( '2'==m_ver ) ? 9 : 8;
		return false;
	}
	// 次のファイルの情報格納位置へ
	fseek( COpenAsd::fp, m_hdrpos, SEEK_SET );
	// ヘッダ解析
	int c = fgetc(COpenAsd::fp);
	fread( buf, 1, c+14, COpenAsd::fp );
	lstrcpy(lpSubInfo->szMode,"ASD");

	memcpy(lpSubInfo->szFileName, buf, c ), lpSubInfo->szFileName[c] = '\0';
	lpSubInfo->dwOriginalSize =	// 元サイズ
		(buf[c+ 0])+(buf[c+ 1]<<8)+(buf[c+ 2]<<16)+(buf[c+ 3]<<24);
	lpSubInfo->dwCRC =		// CRC
		(buf[c+ 4])+(buf[c+ 5]<<8)+(buf[c+ 6]<<16)+(buf[c+ 7]<<24);
	unsigned long uptime =	// 更新日時
		(buf[c+ 8])+(buf[c+ 9]<<8)+(buf[c+10]<<16)+(buf[c+11]<<24);
	unsigned short attr =	// 属性
		(buf[c+12])+(buf[c+13]<<8);

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	if (attr & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[4] = 'R';
	if (attr & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[2] = 'H';
	if (attr & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[3] = 'S';
	if (attr & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[1] = 'D';
	if (attr & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[0] = 'A';
	
	lpSubInfo->wDate=(WORD)(uptime>>16);
	lpSubInfo->wTime=(WORD)uptime;

	// ヘッダ位置ポインタを進めておく
	m_hdrpos += (1+c+14);

	return true;
}