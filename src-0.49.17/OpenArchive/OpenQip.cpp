#include "stdafx.h"
#include "OpenQip.h"
#include "../Tool.h"
#include "../kiutil.h"

bool COpenQip::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// The listing uses the same reader as the extraction, so the two can
	// never disagree about what is in the archive.
	if( !Opened )
	{
		Opened = true;
		if( !QipOpen( COpenQip::szArcFileName, &arc ) )
		{
			COpenQip::FileNum = 0;
			return false;
		}
		COpenQip::FileNum = arc.nfile;
	}
	COpenQip::Index = -1;

	return COpenQip::set_Find( lpSubInfo );
}

bool COpenQip::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenQip::set_Find( lpSubInfo );
}

bool COpenQip::set_Find(INDIVIDUALINFO *lpSubInfo)
{
	QipFile* f;

	if( ++COpenQip::Index >= COpenQip::FileNum )
	{
		COpenQip::Index = -1;
		return false;
	}
	f = &arc.files[COpenQip::Index];

	lstrcpyn(lpSubInfo->szFileName,f->path,(int)sizeof(lpSubInfo->szFileName));

	lpSubInfo->dwOriginalSize = f->orig;
	lpSubInfo->dwCompressedSize = f->csize;
	lpSubInfo->dwCRC = f->crc;

	lstrcpy(lpSubInfo->szAttribute ,"----W");

	// The five metadata bytes of the record look like an archive wide
	// timestamp, but the unpacker library does not use them, so no time
	// stamp is shown.
	lpSubInfo->wDate = 0;
	lpSubInfo->wTime = 0;

	{
		double tmp1 = (double)lpSubInfo->dwOriginalSize;
		double tmp2 = (double)lpSubInfo->dwCompressedSize;
		if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
			lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
		}
	}

	lstrcpy(lpSubInfo->szMode,"qip");
	return true;
}
