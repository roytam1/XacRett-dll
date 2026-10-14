#include "stdafx.h"
#include "OpenSevenZ.h"

bool COpenSevenZ::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	m_open = m_az->Open( COpenSevenZ::szArcFileName, NULL );
	if( !m_open )
		return false;
	m_index = 0;
	return FindNext( lpSubInfo );
}

bool COpenSevenZ::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	const CSevenZEntry* e;

	if( !m_open )
		return false;
	e = m_az->Get( m_index );
	if( !e )
		return false;
	m_index++;

	// the stored name already includes directories and the extension
	lstrcpyn( lpSubInfo->szFileName, e->szName, FNAME_MAX32 + 1 );
	lpSubInfo->dwOriginalSize  = e->bDir ? 0 : e->dwSize;
	lpSubInfo->dwCompressedSize = e->bDir ? 0 : e->dwPackSize;
	lpSubInfo->dwCRC          = e->bCRCKnown ? e->dwCRC : 0;
	lpSubInfo->wDate          = 0;
	lpSubInfo->wTime          = 0;
	lpSubInfo->uOSType        = 0;
	lpSubInfo->uFlag          = 0;

	lstrcpy( lpSubInfo->szMode, e->szMode );
	if( e->bDir )
		lstrcpy( lpSubInfo->szAttribute, "D---W" );
	else if( e->dwAttrib & 0x10 )
		lstrcpy( lpSubInfo->szAttribute, "D---W" );
	else
		lstrcpy( lpSubInfo->szAttribute, "A---W" );
	return true;
}