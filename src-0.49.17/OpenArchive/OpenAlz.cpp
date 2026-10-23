#include "stdafx.h"
#include "OpenAlz.h"

bool COpenAlz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	if( !chain.Open( szArcFileName ) )
		return false;
	return Find_sub(lpSubInfo);
}

bool COpenAlz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}
bool COpenAlz::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	AlzMember m;
	char name[ALZ_NAME_MAX];

	if( !chain.Valid() )  // the walk was closed; FindFirst re-opens
		return false;

	for(;;)
	{
		int r = AlzNextMember( &chain, &m, name );
		if( r <= 0 )          // END Record or a structural error
		{
			chain.Close();
			return false;
		}
		if( m.name_len < ALZ_NAME_MAX
			&& ( !(m.fd & ALZ_FD_NB)
				|| m.method == ALZ_M_STORE
				|| m.method == ALZ_M_DEFLATE ) )
			break;
		AlzSkipMember( &chain, &m );  // skip unsupported or unsafe members
	}

	// the member's data follows its header; step over it so the next
	// AlzNextMember() lands on the following header
	if( m.fd & ALZ_FD_NB )
		AlzSkipMember( &chain, &m );

	::lstrcpyn(lpSubInfo->szFileName,name,FNAME_MAX32+1);
	lpSubInfo->dwCRC = m.crc;
	lpSubInfo->dwCompressedSize = m.csize;
	lpSubInfo->dwOriginalSize = m.usize;

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	if (m.attr & 0x01)
		lpSubInfo->szAttribute[4] = 'R';
	if (m.attr & 0x02)
		lpSubInfo->szAttribute[2] = 'H';
	if (m.attr & 0x04)
		lpSubInfo->szAttribute[3] = 'S';
	if (m.attr & 0x10)
		lpSubInfo->szAttribute[1] = 'D';
	if (m.attr & 0x20)
		lpSubInfo->szAttribute[0] = 'A';

	lpSubInfo->wDate = m.date;
	lpSubInfo->wTime = m.time;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	//0 store, 2 deflate
	lstrcpy(lpSubInfo->szMode,(m.method&0x02)?"deflate":"stored");

	return true;
}
