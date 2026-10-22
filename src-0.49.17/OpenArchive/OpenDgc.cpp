#include "stdafx.h"
#include "OpenDgc.h"
#include "../Tool.h"
#include "../kiutil.h"

bool COpenDgc::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// The listing uses the same reader as the extraction, so the two
	// can never disagree about what is in the archive.
	if( !Opened )
	{
		Opened = true;
		if( !DgcOpen( COpenDgc::szArcFileName, &arc ) )
		{
			COpenDgc::FileNum = 0;
			return false;
		}
		COpenDgc::FileNum = arc.nentry;
	}
	COpenDgc::Index = -1;

	return COpenDgc::set_Find( lpSubInfo );
}

bool COpenDgc::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenDgc::set_Find( lpSubInfo );
}

bool COpenDgc::set_Find(INDIVIDUALINFO *lpSubInfo)
{
	DgcEntry* e;
	char tmp[MAX_PATH];

	if( ++COpenDgc::Index >= COpenDgc::FileNum )
	{
		COpenDgc::Index = -1;
		return false;
	}
	e = &arc.entries[COpenDgc::Index];

	// the names are UTF-8
	if( !kiutil::utf8toAnsi(tmp,(int)sizeof(tmp),e->name) )
		lstrcpyn(tmp,e->name,(int)sizeof(tmp));
	lstrcpyn(lpSubInfo->szFileName,tmp,(int)sizeof(lpSubInfo->szFileName));

	lpSubInfo->dwOriginalSize = e->size;
	lpSubInfo->dwCompressedSize = e->csize;
	lpSubInfo->dwCRC = e->crc;

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	if (e->attr & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[4] = 'R';
	if (e->attr & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[2] = 'H';
	if (e->attr & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[3] = 'S';
	if (e->attr & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[1] = 'D';
	if (e->attr & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[0] = 'A';

	// the time stamp of the entry
	{
		FILETIME ft;
		LARGE_INTEGER li;
		li.QuadPart = (LONGLONG)e->mtime;
		ft.dwLowDateTime = li.LowPart;
		ft.dwHighDateTime = li.HighPart;
		FileTimeToDosDateTime(&ft,&lpSubInfo->wDate,&lpSubInfo->wTime);
	}

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	lstrcpy(lpSubInfo->szMode,"DGC");
	return true;
}
