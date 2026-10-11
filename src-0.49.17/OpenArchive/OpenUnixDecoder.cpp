#include "stdafx.h"
#include "OpenUnixDecoder.h"

bool COpenUnixDecoder::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	char* str=(char*)COpenUnixDecoder::common_buf;

	char outname[5000]="";
	while( !feof(fp) )
	{
		kiutil::getline( str,XACR_BUFSIZE,fp );
		if( str==strstr(str,"begin") )
		{
			int dummy;
			sscanf(str,"begin %o %s",&dummy,outname);
			break;
		}
	}
	if( *outname==0 )
	{
		char newext[] = {x,x,'d','e','c','o','d','e'};

		const char* ext = kiutil::pathExt( szArcFileName );
		if( (ext[0]!=x && ext[0]!=X)
		 || (ext[1]!=x && ext[1]!=X) )
			kiutil::getOriginalName( outname,szArcFileName,newext );
		else
			kiutil::getOriginalName( outname,szArcFileName,NULL );
	}
	
	lstrcpy(lpSubInfo->szFileName,outname);
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	lpSubInfo->dwCompressedSize=(DWORD)filelength(fileno(fp));
	lstrcpy(lpSubInfo->szMode,"stored");
	return true;
}

bool COpenUnixDecoder::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}