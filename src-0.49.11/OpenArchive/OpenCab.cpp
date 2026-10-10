#include "stdafx.h"
#include "OpenCab.h"

bool COpenCab::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	fseek(COpenCab::fp,0,SEEK_SET);
	CAB_ENTRY hdr;

	if (!doHeader(COpenArchive::common_buf ))
		return false;

	if (!read_header(hdr,COpenArchive::common_buf))
		return false;

	lstrcpy(lpSubInfo->szFileName,hdr.fnm);
	lpSubInfo->dwCRC = 0;
	lpSubInfo->dwCompressedSize = 0;
	lpSubInfo->dwOriginalSize = hdr.cbFile;

	lstrcpy(lpSubInfo->szAttribute ,"----");
	if (hdr.attribs & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[2] = 'R';
	if (hdr.attribs & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[1] = 'H';
	if (hdr.attribs & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[0] = 'S';
	if (hdr.attribs & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[3] = 'D';
	if (hdr.attribs & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[3] = 'A';

	//lstrcpy(lpSubInfo->szMode,"");

	lpSubInfo->wDate = hdr.date;
	lpSubInfo->wRatio = 0;
	lpSubInfo->wTime = hdr.time;
	
	return true;
}

bool COpenCab::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	CAB_ENTRY hdr;

	if (!read_header(hdr,COpenArchive::common_buf))
		return false;

	lstrcpy(lpSubInfo->szFileName,hdr.fnm);
	lpSubInfo->dwCRC = 0;
	lpSubInfo->dwCompressedSize = 0;
	lpSubInfo->dwOriginalSize = hdr.cbFile;

	lstrcpy(lpSubInfo->szAttribute ,"----");
	if (hdr.attribs & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[2] = 'R';
	if (hdr.attribs & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[1] = 'H';
	if (hdr.attribs & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[0] = 'S';
	if (hdr.attribs & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[3] = 'D';
	if (hdr.attribs & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[3] = 'A';

	lpSubInfo->wDate = hdr.date;
	lpSubInfo->wRatio = 0;
	lpSubInfo->wTime = hdr.time;
	
	return true;
}

bool COpenCab::read_header( CAB_ENTRY &hdr,unsigned char* buf )
{
	static int readedfilenum = 0;
	
	long n = ftell(COpenCab::fp);//元々いたところ
	fread(&hdr,sizeof(CAB_ENTRY),1,COpenCab::fp);
	for (int i = 0;hdr.fnm[i]!='\0';i++);//ファイル名の長さを得る

	if (readedfilenum++ == COpenCab::iFiles)
	{
		readedfilenum = 0;
		return false;
	}
	fseek(COpenCab::fp,n+i+17,SEEK_SET);
	return true;
	
}
bool COpenCab::doHeader(unsigned char* buf )
{ 
	CAB_HEADER hdr;
	int c, stage=0;
	while( EOF != (c=fgetc(COpenCab::fp)) )
	{
			 if( c=='M'             ) stage++;
		else if( c=='S' && stage==1 ) stage++;
		else if( c=='C' && stage==2 ) stage++;
		else if( c=='F' && stage==3 )
		{
			stage++;
			int x=ftell(COpenCab::fp);
			fseek(COpenCab::fp,x-4,SEEK_SET);
			fread(&hdr,sizeof(CAB_HEADER),1,COpenCab::fp);
			int t =filelength(fileno(COpenCab::fp))-x+4;
			if (hdr.cbCabinet == (DWORD) t )
			{
				COpenCab::iFiles = hdr.cFiles;
				fseek(COpenCab::fp,hdr.coffFiles+x-4,SEEK_SET);
				return true;
			}
		}
		else stage=0;
	}

	return false;
}