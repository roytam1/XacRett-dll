#include "stdafx.h"
#include "OpenZip.h"

bool COpenZip::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	cdhead.clear();
	long offset=get_cdstructure_offset();
	if(offset)
		get_cdhead(offset);
	cur_cdhead_it=cdhead.begin();

	return Find_sub(lpSubInfo);
}

bool COpenZip::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}
bool COpenZip::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	ZipLocalHeader hdr;

	if (!doHeader( &hdr,COpenArchive::common_buf ))
		return false;

	lstrcpy(lpSubInfo->szFileName,hdr.fnm);
	lpSubInfo->dwCRC = hdr.crc;
	lpSubInfo->dwCompressedSize = hdr.csz;
	lpSubInfo->dwOriginalSize = hdr.usz;

	if(cur_cdhead_it==cdhead.end())
		lstrcpy(lpSubInfo->szAttribute ,"A---W");
	else
	{
		ZipCDHeader h=*cur_cdhead_it;
		cur_cdhead_it++;
		lstrcpy(lpSubInfo->szAttribute ,"----");
		if ((h.attributes_e) & 0x01)
			lpSubInfo->szAttribute[3] = 'R';
		else
			lpSubInfo->szAttribute[3] = 'W';
		if ((h.attributes_e) & 0x02)
			lpSubInfo->szAttribute[2] = 'H';
		if ((h.attributes_e) & 0x04)
			lpSubInfo->szAttribute[1] = 'S';
		/*if (HIWORD(h.attributes_e) & FILE_ATTRIBUTE_DIRECTORY)
			lpSubInfo->szAttribute[3] = 'D';*/
		if ((h.attributes_e) & 0x20)
			lpSubInfo->szAttribute[0] = 'A';
	}
	//next
	fseek( COpenZip::fp,hdr.csz,SEEK_CUR );

	lpSubInfo->wDate = hdr.dat;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	lpSubInfo->wTime = hdr.tim;
	return true;
}
bool COpenZip::read_header( ZipLocalHeader* hdr,unsigned char* buf )
{
	bool del = (buf==NULL);
	if( buf==NULL ) buf = new unsigned char[65536];

	if( 26 != fread(buf,1,26,COpenZip::fp) )
		{ if(del)delete [] buf; return false; }

	hdr->ver = ((buf[ 0])|(buf[ 1]<<8));
	hdr->flg = ((buf[ 2])|(buf[ 3]<<8));
	hdr->mhd = ((buf[ 4])|(buf[ 5]<<8));
	hdr->tim = ((buf[ 6])|(buf[ 7]<<8));
	hdr->dat = ((buf[ 8])|(buf[ 9]<<8));
	hdr->crc = ((buf[10])|(buf[11]<<8)|(buf[12]<<16)|(buf[13]<<24));
	hdr->csz = ((buf[14])|(buf[15]<<8)|(buf[16]<<16)|(buf[17]<<24));
	hdr->usz = ((buf[18])|(buf[19]<<8)|(buf[20]<<16)|(buf[21]<<24));
	hdr->fnl = ((buf[22])|(buf[23]<<8));
	hdr->exl = ((buf[24])|(buf[25]<<8));

	if( hdr->fnl!=fread(buf,1,hdr->fnl,COpenZip::fp) )
		{ if(del)delete [] buf; return false; }

	int l = (hdr->fnl>MAX_PATH-1)?(MAX_PATH-1):(hdr->fnl);
	memcpy( hdr->fnm, buf, l );
	hdr->fnm[l]='\0';
	char* pp;
	for( pp=hdr->fnm; *pp!=0; pp++ );

	if( hdr->mhd > Deflated || hdr->mhd==Tokenized )
		{ if(del)delete [] buf; return false; }
	if( pp-hdr->fnm != l )
		{ if(del)delete [] buf; return false; }
	if( hdr->exl != fread(buf,1,hdr->exl,COpenZip::fp) )
		{ if(del)delete [] buf; return false; }

	if(del)
		delete [] buf;
	return true;
}

bool COpenZip::doHeader( ZipLocalHeader* hdr,unsigned char* buf )
{
	int c, stage=0;
	while( EOF != (c=fgetc(COpenZip::fp)) )
	{
			 if( c=='P'              ) stage++;
		else if( c=='K' &&  stage==1 ) stage++;
		else if( c==0x03 && stage==2 ) stage++;
		else if( c==0x04 && stage==3 )
		{
			stage++;
			int x=ftell(COpenZip::fp);
			if( read_header( hdr,buf ) )
				return true;
			fseek( COpenZip::fp,x,SEEK_SET );
		}
		else stage=0;
	}

	return false;
}
long  COpenZip::get_cdstructure_offset()
{
	long offset=0;
	long now=ftell(COpenZip::fp);
	unsigned char* buf=COpenArchive::common_buf;

	long    size = filelength( fileno( fp ));
	size=(size>XACR_BUFSIZE)?XACR_BUFSIZE-1:size;
	fseek(fp,size,SEEK_END);
	fread(buf,1,size,fp);
	ZipEndCDHeader h;
	for(;size;size--)
	{
		if(buf[size]==0x06) size--;
		else continue;
		if(buf[size]==0x05) size--;
		else continue;
		if(buf[size]==0x4B) size--;
		else continue;
		if(buf[size]==0x50) size;//End of central dir recordÇæÇÊÅOÅO
		else continue;
		::memcpy(&h,&buf[size],sizeof(ZipEndCDHeader));
		offset=h.offset;
		break;
	}
	if(size==0)//ë∂ç›ÇµÇ»Ç¢
	{
		fseek( COpenZip::fp,now,SEEK_SET );
		return 0;
	}
	//èIóπèàóù
	fseek( COpenZip::fp,now,SEEK_SET );
	return offset;
}
void COpenZip::get_cdhead(long offset)
{
	long now=ftell(COpenZip::fp);
	ZipCDHeader h;
	while(fseek(fp,offset,SEEK_SET)==0)
	{
		fread(&h,1,sizeof(ZipCDHeader),fp);
		offset+=sizeof(ZipCDHeader)+h.fnl+h.exl+h.fcl-2;
		if(h.sig[0]==0x50 && h.sig[1]==0x4b && h.sig[2]==0x05 && h.sig[3]==0x06)
			break;
		cdhead.push_back(h);
	}
	//èIóπèàóù
	fseek( COpenZip::fp,now,SEEK_SET );
}