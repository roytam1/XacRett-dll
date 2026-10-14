#include "stdafx.h"
#include "SevenZTool.h"
#include "SevenZDecode.h"
#include "kiutil.h"
#include "XacRett.h"		// EXTRACTINGINFO / arcproc messages

bool C7zTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	CSevenZ az;

	(void)fsize;
	if( siz < 32 )
		return false;
	if( hdr[0]!=0x37 || hdr[1]!=0x7A || hdr[2]!=0xBC ||
	    hdr[3]!=0xAF || hdr[4]!=0x27 || hdr[5]!=0x1C )
		return false;
	if( hdr[6] != 0 )
		return false;

	// Full header parse (this may decode a compressed header) so we never
	// claim a file we cannot actually list.
	return az.Open( fname, NULL );
}

bool C7zTool::Extract( const char* fname, const char* ddir )
{
	CSevenZ az;
	DWORD i, n;
	(void)ddir;

	if( !az.Open( fname, NULL ) )
		return false;

	// Only prompt when the archive actually carries an encrypted folder.
	if( az.NeedsPassword() )
		{
		char* pw = GetPassword();
		if( !pw || !*pw )
			return false;
		az.SetPassword( pw );
		}

	n = az.Count();
	for( i = 0; i < n; i++ )
		{
		const CSevenZEntry* e = az.Get(i);
		char namebuf[FNAME_MAX32 + 1];
		char* name;

		if( m_Endflg )
			return true;
		if( !e )
			break;

		if( e->bDir )
			{
			// still create the directory tree
			lstrcpyn( namebuf, e->szName, sizeof(namebuf) );
			kiutil::pathMake( namebuf );
			continue;
			}

		if( !CheckStr( e->szName ) )
			continue;

		if( arcproc )
			{
			EXTRACTINGINFO info;
			::memset(&info,0,sizeof(info));
			lstrcpy(info.szSourceFileName,e->szName);
			if( arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info) )
				continue;
			}

		lstrcpyn( namebuf, e->szName, sizeof(namebuf) );
		name = kiutil::pathMake( namebuf );
		if( !az.Extract( i, name ) )
			continue;		// bad member: skip it, keep going

		if( e->bHasTime )
			{
			FILETIME ft;
			ft.dwLowDateTime  = (DWORD)(e->ftTime & 0xFFFFFFFFUL);
			ft.dwHighDateTime = (DWORD)(e->ftTime >> 32);
			if( ft.dwHighDateTime )
				kiutil::timeSet( name, &ft );
			}
		if( e->dwAttrib )
			SetFileAttributes( name,
				e->dwAttrib & (_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_ARCH) );
		}
	return true;
}