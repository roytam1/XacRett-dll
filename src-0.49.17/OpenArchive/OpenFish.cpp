#include "stdafx.h"
#include "OpenFish.h"

bool COpenFish::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	readsize=0;
	return FindSub(lpSubInfo);
}

bool COpenFish::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}

bool COpenFish::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	char outname[MAX_PATH];
	char* str=(char*)COpenFish::common_buf;
	if (!kiutil::getline( str,XACR_BUFSIZE,COpenFish::fp ))
		return false;
	int size=0;

	if( !Header( str,outname ,size) )
		return FindSub(lpSubInfo);

	lpSubInfo->dwCompressedSize=(DWORD)(ftell(COpenFish::fp)+25-readsize);
	lpSubInfo->dwOriginalSize=size;
	lstrcpy(lpSubInfo->szFileName,outname);
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	readsize+=lpSubInfo->dwCompressedSize;
	//lstrcpy(lpSubInfo->szMode,"");

	return true;
}
bool COpenFish::Header( const char* str, char* outname ,int& size)
{
	if( 0!=strncmp( str,"begin ",6 ) )
		return false;

	const char* fnm = NULL;
	if( 0==strncmp( str+6,"86~ ",4 ) )
		fnm = str+10;
	else if( 0==strncmp( str+6,"24~ ",4 ) )
		fnm = str+10;
	else if( 0==strncmp( str+6,"4649 ",5 ) )
		fnm = str+11;
	if( !fnm || !(*fnm) || *fnm=='\n' )
		return false;

	while( *fnm!='\0' && *fnm!='\n' )
		*outname++ = *fnm++;
	*outname = '\0';

	//ƒf[ƒ^•”•ª”ò‚Î‚µ
	while( kiutil::getline( (char*)str,XACR_BUFSIZE,COpenFish::fp ) )
	{
		// end
		if( 0==strncmp( str,"size ",5 ) )
			break;
	}
	fnm = str+5;
	char buf[20];
	char* pbuf = buf;
	while( *fnm!='\0' && *fnm!='\n' )
		*pbuf++ = *fnm++;
	*pbuf = '\0';

	size = atoi(buf);
	return true;
}