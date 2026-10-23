#include "stdafx.h"
#include "AlzDecode.h"

#include <string.h>
#include <stdlib.h>

static const unsigned long ALZ_MAGIC_ARC  = 0x015A4C41; // "ALZ\x01"
static const unsigned long ALZ_MAGIC_ITEM = 0x015A4C42; // "BLZ\x01"
static const unsigned long ALZ_MAGIC_END  = 0x015A4C43; // "CLZ\x01"

static long AlzVolSiz( FILE* f )
{
	long o = ftell( f );
	fseek( f, 0, SEEK_END );
	long s = ftell( f );
	fseek( f, o, SEEK_SET );
	return s;
}

bool AlzChain::Open( const char* main )
{
	Close();

	char base[MAX_PATH];
	lstrcpyn( base, main, MAX_PATH );
	size_t bl = strlen( base );
	if( bl > 4 && 0 == stricmp( base+bl-4, ".alz" ) )
		base[bl-4] = '\0';

	vol[0] = fopen( main, "rb" );
	if( vol[0] == NULL )
		{Close(); return false;}
	fsiz[0] = AlzVolSiz( vol[0] );

	bool ok = ( fsiz[0] >= ALZ_HDRSZ );
	unsigned char m[4];
	if( ok && 4 == fread( m,1,4,vol[0] ) )
	{
		unsigned long mag = (unsigned long)m[0] | ((unsigned long)m[1]<<8)
			| ((unsigned long)m[2]<<16) | ((unsigned long)m[3]<<24);
		ok = ( mag == ALZ_MAGIC_ARC );
	}
	if( !ok )
	{
		fclose( vol[0] );
		vol[0] = NULL;
		Close();
		return false;
	}
	fseek( vol[0], ALZ_HDRSZ, SEEK_SET );

	int i = 1;
	for(;;)
	{
		char vp[MAX_PATH+8];
		sprintf( vp, "%s.a%02d", base, i-1 );
		FILE* f = fopen( vp, "rb" );
		if( f == NULL )
			break;
		if( i >= ALZ_MAXVOLS )
			{fclose( f ); break;}
		vol[i] = f;
		fsiz[i] = AlzVolSiz( f );
		fseek( f, ALZ_HDRSZ, SEEK_SET );
		i++;
	}
	nv = i;
	v = 0;
	vpos = ALZ_HDRSZ;
	return true;
}

void AlzChain::Close()
{
	for( int i=0; i!=ALZ_MAXVOLS; i++ )
		if( vol[i] )
			{fclose( vol[i] ); vol[i]=NULL;}
	nv = 0;
	v = 0;
	vpos = 0;
}

bool AlzChain::nextvol()
{
	if( v+1 >= nv || vol[v+1] == NULL )
		return false;
	if( fsiz[v+1] < ALZ_HDRSZ + ALZ_ENDSZ )
		return false;               // a degenerate continuation
	v++;
	vpos = ALZ_HDRSZ;
	fseek( vol[v], vpos, SEEK_SET );
	return true;
}

bool AlzChain::setpos( int av, long p )
{
	if( av < 0 || av >= nv || vol[av] == NULL )
		return false;
	if( p < ALZ_HDRSZ || p > fsiz[av] )
		return false;
	v = av;
	vpos = p;
	fseek( vol[v], p, SEEK_SET );
	return true;
}

int AlzChain::read( void* buf, int len )
{
	if( len < 0 || buf == NULL )
		return 0;
	unsigned char* p = (unsigned char*)buf;
	int total = 0;
	while( total < len )
	{
		long dend = fsiz[v] - ALZ_ENDSZ;
		if( vpos >= dend )
		{
			if( !nextvol() )
				break;
			continue;
		}
		long take = dend - vpos;
		if( take > (long)(len-total) )
			take = len-total;
		size_t got = fread( p+total, 1, take, vol[v] );
		vpos += (long)got;
		total += (int)got;
		if( got == 0 )
			break;              // short read inside a volume
	}
	return total;
}

LONGLONG AlzChain::skip( LONGLONG len )
{
	LONGLONG total = 0;
	while( total < len )
	{
		long dend = fsiz[v] - ALZ_ENDSZ;
		if( vpos >= dend )
		{
			if( !nextvol() )
				break;
			continue;
		}
		long take = dend - vpos;
		LONGLONG want = len - total;
		if( want < (LONGLONG)take )
			take = (long)want;
		if( fseek( vol[v], take, SEEK_CUR ) != 0 )
			break;
		vpos += take;
		total += take;
	}
	return total;
}

bool AlzChain::magic4( unsigned long* out )
{
	unsigned char m[4];
	// the reference's seek_item bound: it walks the whole file and
	// reads the 4 magic bytes unguarded, so the END Record is found
	// where the walk lands on it
	while( vpos + 4 > fsiz[v] )
		if( !nextvol() )
			return false;
	fread( m,1,4,vol[v] );
	vpos += 4;
	*out = (unsigned long)m[0] | ((unsigned long)m[1]<<8)
		 | ((unsigned long)m[2]<<16) | ((unsigned long)m[3]<<24);
	return true;
}

bool AlzChain::readhead( void* buf, int len )
{
	if( len < 0 || buf == NULL )
		return false;
	// one volume, bounded like the reference cur_avail
	if( (long)len > fsiz[v] - vpos )
		return false;
	if( len == 0 )
		return true;
	size_t got = fread( buf, 1, len, vol[v] );
	if( got != (size_t)len )
		return false;
	vpos += len;
	return true;
}

int AlzNextMember( AlzChain* chain, AlzMember* m, char* name )
{
	unsigned long mag;
	m->Zero();
	name[0] = '\0';

	if( !chain->magic4( &mag ) )
		return -1;
	if( mag == ALZ_MAGIC_END )
		return 0;
	if( mag != ALZ_MAGIC_ITEM )
		return -1;

	// the fixed part of the Header after the magic
	unsigned char p[9];
	if( !chain->readhead( p, 9 ) )
		return -1;

	m->name_len = (unsigned short)( p[0] | (p[1]<<8) );
	m->attr = p[2];
	m->time = (unsigned short)( p[3] | (p[4]<<8) );
	m->date = (unsigned short)( p[5] | (p[6]<<8) );
	m->fd   = p[7];

	unsigned nb = m->fd >> 4;
	if( nb > 8 )
		return -1;

	if( m->fd & ALZ_FD_NB )
	{
		unsigned char o[6], s[16];
		if( !chain->readhead( o, 6 ) )
			return -1;
		m->method = o[0];
		m->crc = (unsigned long)o[2] | ((unsigned long)o[3]<<8)
			   | ((unsigned long)o[4]<<16) | ((unsigned long)o[5]<<24);
		if( !chain->readhead( s, nb*2 ) )
			return -1;
		for( unsigned k=0; k<nb; k++ )
			m->csize |= (LONGLONG)s[k+0] << (8*k);
		for( unsigned j=0; j<nb; j++ )
			m->usize |= (LONGLONG)s[nb+j] << (8*j);
	}

	// the name, capped so a corrupt Header cannot walk past the
	// caller's buffer (the caller skips name_len >= ALZ_NAME_MAX)
	{
		unsigned take = m->name_len;
		unsigned rest = 0;
		if( take > ALZ_NAME_MAX-1 )
			{rest = take - (ALZ_NAME_MAX-1); take = ALZ_NAME_MAX-1;}
		unsigned done = 0;
		while( done < take )
		{
			unsigned want = take - done;
			if( want > 256 )
				want = 256;
			if( !chain->readhead( name+done, want ) )
				return -1;
			done += want;
		}
		name[done] = '\0';
		if( rest != 0 )
		{
			// too long for any caller: still eat the stored name
			name[0] = '\0';
			unsigned char disc[64];
			while( rest )
			{
				unsigned want = rest > 64 ? 64 : rest;
				if( !chain->readhead( disc, want ) )
					return -1;
				rest -= want;
			}
		}
	}
	return 1;
}

void AlzSkipMember( AlzChain* chain, const AlzMember* m )
{
	if( !(m->fd & ALZ_FD_NB) )
		return;                     // no Data to skip
	LONGLONG n = m->csize;
	if( m->fd & ALZ_FD_ENC )
		n += ALZ_RANDHED;           // the verifier
	chain->skip( n );
}
