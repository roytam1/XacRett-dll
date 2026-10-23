#include "stdafx.h"
#include "OpenEgg.h"
#include "..\\kiutil.h"

bool COpenEgg::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	arc.Close();
	if( !arc.Open( szArcFileName ) )
		return false;
	m_index = 0;
	return FindNext( lpSubInfo );
}

bool COpenEgg::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	while( m_index < arc.Count() )
	{
		EggMember m;
		char ansi[EGG_NAME_MAX * 2];
		unsigned __int64 csize = 0;
		WORD date = 0, time = 0;

		arc.Member( m_index, &m );
		m_index++;

		if( !kiutil::utf8toAnsi( ansi, sizeof(ansi), m.name ) || !ansi[0] )
			continue;

		csize = arc.MemberCsize( m_index - 1 );

		lstrcpyn( lpSubInfo->szFileName, ansi, FNAME_MAX32 + 1 );
		lpSubInfo->dwCRC = 0;     // EGG carries per-block CRCs only
		lpSubInfo->dwCompressedSize = (DWORD)csize;
		lpSubInfo->dwOriginalSize = (DWORD)m.usize;

		lstrcpy( lpSubInfo->szAttribute, "----W" );
		if( m.attrs & EGG_ATTR_RO )
			lpSubInfo->szAttribute[4] = 'R';
		if( m.attrs & EGG_ATTR_H )
			lpSubInfo->szAttribute[2] = 'H';
		if( m.attrs & EGG_ATTR_S )
			lpSubInfo->szAttribute[3] = 'S';
		if( m.IsDir() )
			lpSubInfo->szAttribute[1] = 'D';

		if( m.hasWinInfo )
		{
			FILETIME ft, lt;

			ft.dwLowDateTime = (DWORD)(m.filetime & 0xFFFFFFFF);
			ft.dwHighDateTime = (DWORD)(m.filetime >> 32);
			if( FileTimeToLocalFileTime( &ft, &lt ) )
				FileTimeToDosDateTime( &lt, &date, &time );
		}
		lpSubInfo->wDate = date;
		lpSubInfo->wTime = time;

		switch( arc.MemberMethod( m_index - 1 ) )
		{
		case EGG_M_DEFLATE: lstrcpy( lpSubInfo->szMode, "deflate" ); break;
		case EGG_M_BZIP2:   lstrcpy( lpSubInfo->szMode, "bzip2" );   break;
		case EGG_M_AZO:     lstrcpy( lpSubInfo->szMode, "azo" );     break;
		case EGG_M_LZMA:    lstrcpy( lpSubInfo->szMode, "lzma" );    break;
		default:            lstrcpy( lpSubInfo->szMode, "stored" );  break;
		}

		double tmp1 = (double)lpSubInfo->dwOriginalSize;
		double tmp2 = (double)lpSubInfo->dwCompressedSize;
		if( lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0 )
			lpSubInfo->wRatio = (WORD)(tmp2 * 1000 / tmp1);

		return true;
	}
	return false;
}
