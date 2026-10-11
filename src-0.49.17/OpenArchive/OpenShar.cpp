#include "stdafx.h"
#include "OpenShar.h"

bool COpenShar::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}

bool COpenShar::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}

bool COpenShar::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	char *line = (char*)COpenShar::common_buf,
		lead[256], file[256], end[256];
	static int readed=0;

	int llen, elen;

	if( !getline(COpenShar::fp,true,line ) )
		{readed=0;return false;}
	/*if( 0==strcmp(line,"# This shar contains:"))
	{
		if( !getline(COpenShar::fp,false,line ) )
				{readed=0;return false;}
		if( 0==strcmp(line,"# length mode       name"))
		{
			return true;

		}
		return false;
	}else*/ if( 0==strncmp(line,"sed",3)
		 || 0==strncmp(line,"gres",4)
		 || 0==strncmp(line,"cat",3) )
	{
		// ↓こんな感じになっている。
		// sed "s/^XX//" >'gzip-1.2.4/README' <<'END_OF_FILE'

		getparameters( line,lead,file,end );
		llen = strlen( lead );
		elen = strlen( end );
		lstrcpy(lpSubInfo->szFileName,file);
		lstrcpy(lpSubInfo->szAttribute,"A---W");
		lstrcpy(lpSubInfo->szMode,"stored");
		//lstrcpy(lpSubInfo->szMode,"");
		// データ部飛ばし
		while( true )
		{
			if( !getline(COpenShar::fp,false,line ) )
				break;
			if( 0==strncmp( line,end,elen ) )
				break;	
		}
		lpSubInfo->dwCompressedSize=ftell(COpenShar::fp)-readed;
		readed=ftell(COpenShar::fp);
		return true;
	}else{
		return Find_sub(lpSubInfo);
	}
}
