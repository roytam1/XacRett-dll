#include "stdafx.h"
#include "OpenQuantum.h"
#include "../Tool.h"
#include "../kiutil.h"

bool COpenQuantum::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// The listing uses the same reader as the extraction, so the two can
	// never disagree about what is in the archive.
	if( !Opened )
	{
		Opened = true;
		if( !QuantumOpen( COpenQuantum::szArcFileName, &arc ) )
		{
			COpenQuantum::FileNum = 0;
			return false;
		}
		COpenQuantum::FileNum = arc.nfile;
	}
	COpenQuantum::Index = -1;

	return COpenQuantum::set_Find( lpSubInfo );
}

bool COpenQuantum::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenQuantum::set_Find( lpSubInfo );
}

bool COpenQuantum::set_Find(INDIVIDUALINFO *lpSubInfo)
{
	QuantumFile* f;

	for(;;)
	{
		if( ++COpenQuantum::Index >= COpenQuantum::FileNum )
		{
			COpenQuantum::Index = -1;
			return false;
		}
		f = &arc.files[COpenQuantum::Index];
		// a member whose stored path is not usable is not shown and not
		// extracted either
		if( f->badname ) continue;
		break;
	}

	lstrcpyn(lpSubInfo->szFileName,f->name,(int)sizeof(lpSubInfo->szFileName));

	lpSubInfo->dwOriginalSize = f->size;
	// the format does not store the size of a member in the archive
	lpSubInfo->dwCompressedSize = 0;
	// the CRC of a member is in the compressed stream behind its data, so
	// it is not known before the member is decoded
	lpSubInfo->dwCRC = 0;

	lstrcpy(lpSubInfo->szAttribute ,"----W");

	lpSubInfo->wDate = f->date;
	lpSubInfo->wTime = f->time;

	lpSubInfo->wRatio = 0;

	lstrcpy(lpSubInfo->szMode,"quantum");
	return true;
}
