#include "stdafx.h"
#include "EggTool.h"
#include "kiutil.h"
#include <stdlib.h>

// The output state of one extraction run.  The engine calls the four
// callbacks below; they are file-scope functions because a local class
// cannot have members under VC4 (see AGENTS.md).
struct EggOut
{
	CTool*      owner;
	EggArchive* arc;
	char        path[MAX_PATH];
	FILE*       fp;

	void Reset()
	{
		path[0] = '\0';
		fp = NULL;
	}
};

static int EggOutOpen( void* ctx, int index, const EggMember* m )
{
	EggOut* o = (EggOut*)ctx;
	char    ansi[EGG_NAME_MAX * 2];
	char*   name2;

	(void)index;
	if( o->fp )
	{
		fclose( o->fp );
		o->fp = NULL;
	}
	o->path[0] = '\0';

	if( !kiutil::utf8toAnsi( ansi, sizeof(ansi), m->name ) || !ansi[0] )
		return 0;                 // no usable name: skip the member
	if( !o->owner->CheckStr( ansi ) )
		return 0;                 // the progress dialog asked to stop

	if( m->IsDir() )
	{
		name2 = kiutil::pathMake( ansi );
		::CreateDirectory( name2, NULL );
		return 0;                 // nothing to write
	}

	name2 = kiutil::pathMake( ansi );
	o->fp = fopen( name2, "wb" );
	if( !o->fp )
		return -1;
	lstrcpyn( o->path, name2, MAX_PATH );
	return 1;
}

static int EggOutWrite( void* ctx, const BYTE* d, DWORD n )
{
	EggOut* o = (EggOut*)ctx;

	if( !o->fp )
		return -1;
	if( n && fwrite( d, 1, n, o->fp ) != n )
		return -1;
	return 1;
}

static void EggOutClose( void* ctx, int index )
{
	EggOut*   o = (EggOut*)ctx;
	EggMember m;

	if( !o->fp )
		return;
	fclose( o->fp );
	o->fp = NULL;
	if( o->arc && o->path[0] && o->arc->Member( index, &m ) && m.hasWinInfo )
	{
		FILETIME ft;

		ft.dwLowDateTime = (DWORD)(m.filetime & 0xFFFFFFFF);
		ft.dwHighDateTime = (DWORD)(m.filetime >> 32);
		kiutil::timeSet( o->path, &ft );
	}
	o->path[0] = '\0';
}

static void EggOutFail( void* ctx, int index, const char* msg )
{
	EggOut* o = (EggOut*)ctx;

	(void)index; (void)msg;
	if( o->fp )
	{
		fclose( o->fp );
		o->fp = NULL;
	}
	if( o->path[0] )
	{
		DeleteFile( o->path );
		o->path[0] = '\0';
	}
}

// Debug hook: with XACRETT_EGG_DBG set, the reason of a failed extraction
// is appended to XacRett.egg.log in the current directory (the output dir,
// since MeltArchive chdirs before the provider runs).
static void EggDbg( const char* msg )
{
	const char* env = getenv( "XACRETT_EGG_DBG" );
	FILE*       f;

	if( !env || !*env )
		return;
	f = fopen( "XacRett.egg.log", "a" );
	if( !f )
		return;
	fprintf( f, "%s\n", msg );
	fclose( f );
}

//-- CEggTool -----------------------

bool CEggTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	unsigned long i;

	(void)fname;
	if( siz < EGG_HDRSZ )
		return false;

	// the EGGA header; a short stub before it is tolerated (no EGG SFX
	// writer exists, so the header is normally at offset 0)
	for( i = 0; i + EGG_HDRSZ <= siz && i < 0x1000; i++ )
	{
		if( hdr[i+0] == 'E' && hdr[i+1] == 'G' &&
		    hdr[i+2] == 'G' && hdr[i+3] == 'A' )
			return true;
	}
	return false;
}

bool CEggTool::Extract( const char* fname, const char* ddir )
{
	EggArchive arc;
	EggOut     out;
	EggSink    sink;
	bool       ok;

	(void)ddir;                   // MeltArchive already chdir()ed

	out.owner = this;
	out.arc = NULL;
	out.Reset();

	if( !arc.Open( fname ) )
	{
		EggDbg( arc.Error() );
		return false;
	}
	out.arc = &arc;

	sink.ctx = &out;
	sink.open = EggOutOpen;
	sink.write = EggOutWrite;
	sink.close = EggOutClose;
	sink.fail = EggOutFail;
	ok = arc.Extract( &sink );
	if( !ok )
		EggDbg( arc.Error() );
	if( out.fp )
	{
		fclose( out.fp );
		out.fp = NULL;
		if( out.path[0] )
			DeleteFile( out.path );
	}
	arc.Close();
	return ok;
}
