#include "stdafx.h"
#include "OpenAlz.h"

bool COpenAlz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}

bool COpenAlz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}
bool COpenAlz::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	if (!doHeader())
		return false;

	::lstrcpyn(lpSubInfo->szFileName,(LPCSTR)hdr.fname,hdr.blz.fn_len+1);
	lpSubInfo->dwCRC = hdr.blz.crc;
	lpSubInfo->dwCompressedSize = hdr.cmp_size;
	lpSubInfo->dwOriginalSize = hdr.ext_size;

	lstrcpy(lpSubInfo->szAttribute ,"----");
	if ((hdr.blz.atter) & 0x0100)
		lpSubInfo->szAttribute[3] = 'R';
	else
		lpSubInfo->szAttribute[3] = 'W';
	if ((hdr.blz.atter) & 0x0200)
		lpSubInfo->szAttribute[2] = 'H';
	if ((hdr.blz.atter) & 0x0400)
		lpSubInfo->szAttribute[1] = 'S';
	/*if (HIWORD(h.attributes_e) & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[3] = 'D';*/
	if ((hdr.blz.atter) & 0x2000)
		lpSubInfo->szAttribute[0] = 'A';

	lpSubInfo->wDate = hdr.blz.date;
	lpSubInfo->wTime = hdr.blz.time;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	return true;
}
bool COpenAlz::read_header()
{
	unsigned char* buf = COpenAlz::common_buf;

	int x=ftell(COpenAlz::fp);
	if( sizeof(BLZ)+1 != fread(buf,1,sizeof(BLZ)+1,COpenAlz::fp) )
		return false;

	fseek( COpenAlz::fp,x+9,SEEK_SET );

	hdr.blz.fn_len=buf[0];
	hdr.blz.atter=((buf[1])|(buf[2]<<8));
	hdr.blz.time=((buf[3])|(buf[4]<<8));
	hdr.blz.date=((buf[5])|(buf[6]<<8));
	switch ((buf[7]&0x01)?(buf[7]^0x01):buf[7])
	{
	case 0x00:
		hdr.blz.method=0;
		hdr.blz.crc=0;
		hdr.ext_size=0;
		hdr.cmp_size=0;
		fread(&buf[9],1,hdr.blz.fn_len+1,COpenAlz::fp);
		hdr.fname=&buf[9];
		break;
	case 0x10:
		fread(&buf[9],1,8,COpenAlz::fp);
		hdr.cmp_size=buf[15];
		hdr.ext_size=buf[16];
		fread(&buf[17],1,hdr.blz.fn_len+1,COpenAlz::fp);
		hdr.fname=&buf[17];
		break;
	case 0x20:
		fread(&buf[9],1,10,COpenAlz::fp);
		hdr.cmp_size=((buf[15])|(buf[16]<<8));
		hdr.ext_size=((buf[17])|(buf[18]<<8));
		fread(&buf[19],1,hdr.blz.fn_len+1,COpenAlz::fp);
		hdr.fname=&buf[19];
		break;
	case 0x40:
		fread(&buf[9],1,14,COpenAlz::fp);
		hdr.cmp_size=((buf[15])|(buf[16]<<8)|(buf[17]<<16)|(buf[18]<<24));
		hdr.ext_size=((buf[19])|(buf[20]<<8)|(buf[21]<<16)|(buf[22]<<24));
		fread(&buf[23],1,hdr.blz.fn_len+1,COpenAlz::fp);
		hdr.fname=&buf[23];
		break;
	case 0x80:
		fread(&buf[9],1,22,COpenAlz::fp);
		hdr.cmp_size=((buf[15])|(buf[16]<<8)|(buf[17]<<16)|(buf[18]<<24)|(buf[19]<<32)|(buf[20]<<40)|(buf[21]<<48)|(buf[22]<<56));
		hdr.ext_size=((buf[23])|(buf[24]<<8)|(buf[25]<<16)|(buf[26]<<24)|(buf[27]<<32)|(buf[28]<<40)|(buf[29]<<48)|(buf[30]<<56));
		
		fread(&buf[31],1,hdr.blz.fn_len+1,COpenAlz::fp);
		hdr.fname=&buf[31];
		break;
	};
	if((buf[7]&0x01)?(buf[7]^0x01):buf[7]!=0)
	{
		hdr.blz.method=((buf[9])|(buf[10]<<8));
		hdr.blz.crc=((buf[11])|(buf[12]<<8)|(buf[13]<<16)|(buf[14]<<24));
	}
	fseek( COpenAlz::fp,-1,SEEK_CUR );

	return true;
}
bool COpenAlz::doHeader()
{
	int c, stage=0;
	while( EOF != (c=fgetc(COpenAlz::fp)) )
	{
			 if( c=='B'              ) stage++;
		else if( c=='L' &&  stage==1 ) stage++;
		else if( c=='Z' && stage==2 ) stage++;
		else if( c==0x01 && stage==3 )
		{
			stage++;
			int x=ftell(COpenAlz::fp);
			if( read_header() )
				return true;
			fseek( COpenAlz::fp,x,SEEK_SET );
		}
		else stage=0;
	}
	return false;
}