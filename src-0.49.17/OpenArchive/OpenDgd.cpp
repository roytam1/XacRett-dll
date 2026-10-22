#include "stdafx.h"
#include "OpenDgd.h"
#include "../DgdDecode.h"
#include "../DgcCrypt.h"
#include "../Tool.h"
#include "../kiutil.h"

// A .dgd file holds one file that is split into parts, so the listing
// reports a single entry: the name and the size are in the INFO chunk of
// the parts, so the parts never have to be put together for the listing.
bool COpenDgd::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	DgdFile f;
	char* pw;
	unsigned long fsize = 0;
	FILE* fp;

	pw = CTool::GetPasswordArg();
	if( !DgdInfo( COpenDgd::szArcFileName, pw, &f ) )
		return false;

	fp = fopen( COpenDgd::szArcFileName, "rb" );
	if( fp ) {
		fseek( fp, 0, SEEK_END );
		fsize = (unsigned long)ftell(fp);
		fclose(fp);
	}

	lstrcpyn(lpSubInfo->szFileName,f.name,(int)sizeof(lpSubInfo->szFileName));
	lpSubInfo->dwOriginalSize = f.total;
	lpSubInfo->dwCompressedSize = fsize;
	// the format has a SHA-512 of the whole file, not a CRC
	lpSubInfo->dwCRC = 0;
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	lstrcpy(lpSubInfo->szMode,"dgd");

	// the time stamp of the file (the DFDT chunk)
	{
		FILETIME ft;
		LARGE_INTEGER li;
		li.QuadPart = (LONGLONG)f.mtime;
		ft.dwLowDateTime = li.LowPart;
		ft.dwHighDateTime = li.HighPart;
		FileTimeToDosDateTime(&ft,&lpSubInfo->wDate,&lpSubInfo->wTime);
	}

	lpSubInfo->wRatio = 1000;
	DgdClose( &f );
	return true;
}

bool COpenDgd::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	(void)lpSubInfo;
	return false;
}
