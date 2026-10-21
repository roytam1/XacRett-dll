#include "stdafx.h"
#include "OpenCab.h"

// The listing uses the same parser as the extraction, so the two can
// never disagree about what is in the archive.  The old code had a
// parser of its own, and it only accepted an archive whose cbCabinet
// field was exactly the size of the file, so most real cabinets
// listed as empty.
bool COpenCab::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	long base;
	long len;

	fseek(COpenCab::fp,0,SEEK_END);
	len = ftell(COpenCab::fp);
	fseek(COpenCab::fp,0,SEEK_SET);
	base = CabFindHeader( COpenCab::fp, (unsigned long)len );
	if( base<0 )
		return false;
	if( !CabReadHeader( COpenCab::fp, base, &m_cab ) )
		return false;

	m_index = 0;
	return FindNext( lpSubInfo );
}

bool COpenCab::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	const CabFileEntry* f;

	if( m_index >= m_cab.nFiles )
	{
		CabFreeHeader( &m_cab );
		return false;
	}
	f = &m_cab.files[m_index++];

	lstrcpy( lpSubInfo->szFileName, f->name );
	lpSubInfo->dwOriginalSize = f->size;
	lpSubInfo->dwCompressedSize = 0;
	lpSubInfo->dwCRC = 0;
	lpSubInfo->wDate = f->date;
	lpSubInfo->wTime = f->time;
	lpSubInfo->wRatio = 0;

	lstrcpy( lpSubInfo->szAttribute, "----" );
	if( f->attribs & CAB_ATTRIB_READONLY ) lpSubInfo->szAttribute[2] = 'R';
	if( f->attribs & CAB_ATTRIB_HIDDEN )   lpSubInfo->szAttribute[1] = 'H';
	if( f->attribs & CAB_ATTRIB_SYSTEM )   lpSubInfo->szAttribute[0] = 'S';
	if( f->attribs & CAB_ATTRIB_DIRECTORY ) lpSubInfo->szAttribute[3] = 'D';
	else if( f->attribs & CAB_ATTRIB_ARCHIVE ) lpSubInfo->szAttribute[3] = 'A';

	// the compression method of the folder the member is in
	{
		WORD tc = 0xFFFF;
		if( (int)f->folder < m_cab.nFolders )
			tc = m_cab.folders[f->folder].typeCompress;
		lstrcpy( lpSubInfo->szMode, CabCompName( tc ) );
	}
	return true;
}