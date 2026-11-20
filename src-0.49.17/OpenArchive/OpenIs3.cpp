#include "stdafx.h"
#include "OpenIs3.h"
#include "../Tool.h"
#include "../kiutil.h"

bool COpenIs3::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// Listing uses the same container reader as extraction.
	if( !Opened )
	{
		Opened = true;
		if( !Is3Open( COpenIs3::szArcFileName, &arc ) )
		{
			COpenIs3::FileNum = 0;
			return false;
		}
		COpenIs3::FileNum = arc.nfile;
	}
	COpenIs3::Index = -1;
	return COpenIs3::set_Find( lpSubInfo );
}

bool COpenIs3::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenIs3::set_Find( lpSubInfo );
}

bool COpenIs3::set_Find(INDIVIDUALINFO *lpSubInfo)
{
	Is3File* f;

	if( ++COpenIs3::Index >= COpenIs3::FileNum )
	{
		COpenIs3::Index = -1;
		return false;
	}
	f = &arc.files[COpenIs3::Index];

	lstrcpyn(lpSubInfo->szFileName,f->path,(int)sizeof(lpSubInfo->szFileName));

	lpSubInfo->dwOriginalSize = f->orig;
	lpSubInfo->dwCompressedSize = f->comp;
	lpSubInfo->dwCRC = 0;	// IS3 does not store a CRC

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	lpSubInfo->wDate = f->date;
	lpSubInfo->wTime = f->time;

	{
		double tmp1 = (double)lpSubInfo->dwOriginalSize;
		double tmp2 = (double)lpSubInfo->dwCompressedSize;
		if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0)
			lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	lstrcpy(lpSubInfo->szMode, (f->flags & 0x10) ? "stored" : "dcl");
	return true;
}
