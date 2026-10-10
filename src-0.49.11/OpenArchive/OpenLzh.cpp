#include "stdafx.h"
#include "OpenLzh.h"

bool COpenLzh::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	DWORD siz = fread( COpenLzh::common_buf, 1, XACR_BUFSIZE, COpenLzh::fp );
	int ps = FindHeader(COpenLzh::common_buf,siz );
	if( ps==-1 )
		return false;
	fseek( COpenLzh::fp,ps,SEEK_SET );

	return COpenLzh::FindSub(lpSubInfo);
}

bool COpenLzh::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenLzh::FindSub(lpSubInfo);
}
bool COpenLzh::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	if (!ReadHeader(COpenLzh::common_buf ))
		return false;
	
	long base = ftell(COpenLzh::fp);
	fseek(COpenLzh::fp, base+h_CompSize, SEEK_SET );

	lpSubInfo->dwCompressedSize = h_CompSize;
	lpSubInfo->dwCRC = h_filecrc;
	lpSubInfo->dwOriginalSize = h_OrigSize;

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	if (h_Attrib & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[4] = 'R';
	if (h_Attrib & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[2] = 'H';
	if (h_Attrib & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[3] = 'S';
	if (h_Attrib & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[1] = 'D';
	if (h_Attrib & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[0] = 'A';

	//lstrcpy(lpSubInfo->szMode,"");
	lstrcpy(lpSubInfo->szFileName,h_FileName);

	if (h_OS=='M') lpSubInfo->uOSType=0;
	else if (h_OS=='U') lpSubInfo->uOSType=2;
	else if (h_OS=='m') lpSubInfo->uOSType=4;
	else if (h_OS=='H') lpSubInfo->uOSType=14;
	else if (h_OS=='2') lpSubInfo->uOSType=5;
	else if (h_OS=='C') lpSubInfo->uOSType=15;
	else if (h_OS=='9') lpSubInfo->uOSType=11;
	else if (h_OS=='K') lpSubInfo->uOSType=12;
	else if (h_OS=='3') lpSubInfo->uOSType=12;
	else if (h_OS=='F') lpSubInfo->uOSType=16;
	else if (h_OS=='R') lpSubInfo->uOSType=17;
	else if (h_OS=='W') lpSubInfo->uOSType=18;
	else if (h_OS=='w') lpSubInfo->uOSType=19;
	else lpSubInfo->uOSType=-1;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	if (h_Level<2){
		lpSubInfo->wDate = (WORD)(h_Update>>16);
		lpSubInfo->wTime = (WORD)(h_Update);
	}else{

		struct tm* time=localtime((long*)&h_Update);
		if( time!=NULL )
		{
			FILETIME ft;
			SYSTEMTIME sys;

			sys.wYear =      time->tm_year+1900;
			sys.wMonth =     time->tm_mon+1;
			sys.wDayOfWeek = time->tm_wday;
			sys.wDay =       time->tm_mday;
			sys.wHour =      time->tm_hour;
			sys.wMinute =    time->tm_min;
			sys.wSecond =    time->tm_sec;
			sys.wMilliseconds = 0;
			SystemTimeToFileTime(&sys,&ft);
			FileTimeToDosDateTime(&ft,&lpSubInfo->wDate,&lpSubInfo->wTime);
		}
	}
	return true;
}

int COpenLzh::FindHeader(const BYTE* hdr, DWORD siz )
{

	int ans=-1;
	BYTE temp[65536];

	if (siz < 20)
		return -1;
	for( DWORD i=0; i<siz-20; i++ )
	{
		if( hdr[i+2]=='-' && hdr[i+3]=='l' &&
		   (hdr[i+4]=='h' || hdr[i+4]=='z') )
		{
			fseek(COpenLzh::fp,i,SEEK_SET );
			if( ReadHeader(temp) )
				{ans=(signed)i;break;}
		}
	}

	return ans;
}

bool COpenLzh::ReadHeader( unsigned char* buf )
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
	if (h_Level==2){
		h_filecrc = (buf[21])+(buf[22]<<8);
		h_OS = buf[23];
	}
	if( h_Level!=2 )
	{
		if( sum!=hdrsum || 21+1+buf[21]+2>bshdr )
			return false;
		memcpy( h_FileName,buf+22,buf[21] );
		h_FileName[buf[21]]=0;
		h_filecrc = (buf[22+buf[21]])+(buf[23+buf[21]]<<8);
		if( h_Level==1 )
			h_OS = buf[24+buf[21]];
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