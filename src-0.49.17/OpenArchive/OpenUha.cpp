#include "stdafx.h"
#include "OpenUha.h"
#include "../Tool.h"
#include "../kiutil.h"

bool COpenUha::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// The listing uses the same reader as the extraction, so the two can
	// never disagree about what is in the archive. The command-line
	// password is handed over so an encrypted archive lists its members
	// without ever opening the dialog.
	if( !Opened )
	{
		Opened = true;
		{
			const char* pw = CTool::GetPasswordArg();
			if( 0 != UhaOpen( COpenUha::szArcFileName, pw, &arc ) )
			{
				COpenUha::FileNum = 0;
				return false;
			}
		}
		COpenUha::FileNum = (int)arc.nfiles;
	}
	COpenUha::Index = -1;

	return COpenUha::set_Find( lpSubInfo );
}

bool COpenUha::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenUha::set_Find( lpSubInfo );
}

bool COpenUha::set_Find(INDIVIDUALINFO *lpSubInfo)
{
	UhaEntry* e;

	if( ++COpenUha::Index >= COpenUha::FileNum )
	{
		COpenUha::Index = -1;
		return false;
	}
	e = &arc.entries[COpenUha::Index];

	lstrcpyn(lpSubInfo->szFileName,e->path,(int)sizeof(lpSubInfo->szFileName));

	lpSubInfo->dwOriginalSize = e->size;
	// A solid member has no own compressed size; the house shows the
	// whole packed stream length (matches the tar/solid convention).
	lpSubInfo->dwCompressedSize = arc.packed_len;
	lpSubInfo->dwCRC = e->crc;

	lstrcpy(lpSubInfo->szAttribute ,"----W");

	lpSubInfo->wDate = (WORD)e->date;
	lpSubInfo->wTime = (WORD)e->time;

	{
		double tmp1 = (double)lpSubInfo->dwOriginalSize;
		double tmp2 = (double)lpSubInfo->dwCompressedSize;
		if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
			lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
		}
	}
	lstrcpy(lpSubInfo->szMode, "solid");
	return true;
}
