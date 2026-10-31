#include "stdafx.h"
#include "OpenBsa.h"
#include "../Tool.h"
#include "../kiutil.h"

bool COpenBsa::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// The listing uses the same reader as the extraction, so the two can
	// never disagree about what is in the archive.
	if( !Opened )
	{
		Opened = true;
		if( !BsaOpen( COpenBsa::szArcFileName, &arc ) )
		{
			COpenBsa::FileNum = 0;
			return false;
		}
		COpenBsa::FileNum = arc.nentry;
	}
	COpenBsa::Index = -1;

	return COpenBsa::set_Find( lpSubInfo );
}

bool COpenBsa::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenBsa::set_Find( lpSubInfo );
}

bool COpenBsa::set_Find(INDIVIDUALINFO *lpSubInfo)
{
	BsaEntry* e;

	if( ++COpenBsa::Index >= COpenBsa::FileNum )
	{
		COpenBsa::Index = -1;
		return false;
	}
	e = &arc.entries[COpenBsa::Index];

	lstrcpyn(lpSubInfo->szFileName,e->name,(int)sizeof(lpSubInfo->szFileName));

	lpSubInfo->dwOriginalSize = e->orig;
	lpSubInfo->dwCompressedSize = e->packed;
	lpSubInfo->dwCRC = e->crc;

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	if( e->isdir )
		lpSubInfo->szAttribute[1] = 'D';

	// the time stamp of the entry is a DOS date and time
	lpSubInfo->wDate = (WORD)(e->ts >> 16);
	lpSubInfo->wTime = (WORD)(e->ts & 0xFFFF);

	{
		double tmp1 = (double)lpSubInfo->dwOriginalSize;
		double tmp2 = (double)lpSubInfo->dwCompressedSize;
		if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
			lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
		}
	}
	if( e->method == BSA_METHOD_UNKNOWN && !arc.isbsn )
	{
		// a BSA flag that is not a known method: show it as it is
		wsprintf(lpSubInfo->szMode, "%02X?", e->flag);
	}
	else
		lstrcpy(lpSubInfo->szMode, BsaMethodName( e->method ));
	return true;
}
