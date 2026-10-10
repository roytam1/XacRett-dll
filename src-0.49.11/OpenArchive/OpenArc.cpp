#include "stdafx.h"
#include "OpenArc.h"

bool COpenArc::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	*path=0;
	pass=NULL;
	unsigned char* buf = COpenArc::common_buf;

	int hpos=0,length=fread( buf,1,XACR_BUFSIZE,COpenArc::fp);
	if( length==0 )
		return false;
	if( buf[0]!=0x1a )
	{
		if( -1==(hpos=SkipSfxStub( buf+1,length-1,1 )) )
			return false;
	}
	fseek( COpenArc::fp,hpos,SEEK_SET );
	return FindSub(lpSubInfo);
}

bool COpenArc::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}
bool COpenArc::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	if(!ReadHeader())
		return false;

	unsigned long curpos = ftell(COpenArc::fp);
	unsigned long nextpos = curpos+hdr.csize;

	lpSubInfo->dwCompressedSize=hdr.csize;
	lpSubInfo->dwCRC=hdr.crc;
	lpSubInfo->dwOriginalSize=hdr.usize;
	lpSubInfo->wDate=hdr.date;
	lpSubInfo->wTime=hdr.time;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	lstrcpy(lpSubInfo->szFileName,hdr.name);
	//lstrcpy(lpSubInfo->szMode,"");
	fseek(COpenArc::fp,nextpos,SEEK_SET);
	return true;
}
int COpenArc::SkipSfxStub( const BYTE* dat,unsigned long siz,int curpos )
{
	for( int i=0; i<(signed)siz-29; i++ )
	{
		const BYTE* hdrbuf=dat+i;

		// Magic
		if( hdrbuf[0]!=0x1a )
			continue;

		// Method
		if( hdrbuf[1]==0 || (hdrbuf[1]>0x0b && hdrbuf[1]!=0x1e) )
			continue;

		// read filename
		int j;
		for( j=2; j!=15; j++ )
			if( hdrbuf[j]==0 )
				break;
		if( j==15 || j==2 )
			continue;

		unsigned long csize =
			(hdrbuf[15])|(hdrbuf[16]<<8)|
			(hdrbuf[17]<<16)|(hdrbuf[18]<<24);
		fseek(COpenArc::fp,curpos+i+29+csize,SEEK_SET);
		if( fgetc(COpenArc::fp)!=0x1a )
			continue;

		// OK,header found.
		return curpos+i;
	}
	return -1;
}

bool COpenArc::ReadHeader()
{
	BYTE hdrbuf[29];

// Magic & Method
	if( 2!=fread(hdrbuf,1,2,COpenArc::fp) )
		return false;
	if( hdrbuf[0]!=0x1a )
		return false;

	// end of arc
	if( hdrbuf[1]==0 )
		return false;
	// end of folder
	else if( hdrbuf[1]==0x1f )
	{
		char *p,*pp,*ppp;
		for( p=path,pp=path,ppp=path; *p!=0; p=CharNext(p) )
			if( *p=='\\' )
				{ppp=pp;pp=p+1;}
		*ppp=0;
		return ReadHeader();
	}
	// unknown method
	else if( hdrbuf[1]>0x0b && hdrbuf[1]!=0x1e )
		return false;

// Other Data
	if( hdrbuf[1]==1 )
	{
		if( 23!=fread(hdrbuf+2,1,23,COpenArc::fp) )
			return false;
		hdrbuf[25]=hdrbuf[15],hdrbuf[26]=hdrbuf[16],
		hdrbuf[27]=hdrbuf[17],hdrbuf[28]=hdrbuf[18];
	}
	else
	{
		if( 27!=fread(hdrbuf+2,1,27,COpenArc::fp) )
			return false;
	}

	// read filename
	int j;
	for( j=2; j!=15; j++ )
		if( (hdr.name[j-2]=(char)hdrbuf[j])==0 )
			break;
	if( j==15 )
		return false;

	// folder start
	if( hdrbuf[1]==0x1e )
	{
		strcat(path,hdr.name);
		strcat(path,"\\");
		return ReadHeader();
	}

	hdr.method= hdrbuf[1];
	hdr.csize = (hdrbuf[15]) | (hdrbuf[16]<<8) |
				(hdrbuf[17]<<16) | (hdrbuf[18]<<24);
	hdr.date  = (hdrbuf[19]) | (hdrbuf[20]<<8);
	hdr.time  = (hdrbuf[21]) | (hdrbuf[22]<<8);
	hdr.crc   = (hdrbuf[23]) | (hdrbuf[24]<<8);
	hdr.usize = (hdrbuf[25]) | (hdrbuf[26]<<8) |
				(hdrbuf[27]<<16) | (hdrbuf[28]<<24);

	return true;
}