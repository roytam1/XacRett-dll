
#include "stdafx.h"
#include "DgdDecode.h"
#include "DgcCrypt.h"
#include "DgcDecode.h"

// The chunk header of the container.
#define DGD_CHUNK_HDR	32

static unsigned long DgdRd32( const unsigned char* p )
{
	return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
		   ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}

static unsigned long DgdAlign16( unsigned long x )
{
	return (x + 15ul) & ~15ul;
}

// The SHA-512 of the data, in the byte order the binary stores it (the
// state of the hash, every 64 bit word with its low dword first).
static void DgdHash( const unsigned char* data, unsigned long len,
					 unsigned char out[64] )
{
	DgcSha512 h;
	h.Init();
	h.Update( data, len );
	h.Final();
	h.State( out );
}

// Find the INFO, the DFDT and the DATA chunk of the part.  They are the
// children of the DGCD container.
static int DgdFindChunks( const unsigned char* img, unsigned long size,
						  const unsigned char** info, unsigned long* infolen,
						  const unsigned char** dfdt, unsigned long* dfdtlen,
						  const unsigned char** data, unsigned long* datalen )
{
	unsigned long p, end;

	*info = 0; *infolen = 0;
	*dfdt = 0; *dfdtlen = 0;
	*data = 0; *datalen = 0;
	if( size < DGD_CHUNK_HDR ) return -1;
	if( memcmp( img, "DGCD", 4 ) ) return -1;
	p = DGD_CHUNK_HDR;
	{
		unsigned long psroot = DgdRd32( img + 8 );
		// subtraction form: a huge value must not wrap to a small end
		end = ( psroot > size - DGD_CHUNK_HDR ) ? size
												: DGD_CHUNK_HDR + psroot;
	}
	while( p + DGD_CHUNK_HDR <= end ) {
		unsigned long ps = DgdRd32( img + p + 8 );
		// the same here: a wrapped p would never advance
		if( ps > end - p - DGD_CHUNK_HDR ) break;
		if( !memcmp( img + p, "INFO", 4 ) ) {
			*info = img + p + DGD_CHUNK_HDR; *infolen = ps;
		} else if( !memcmp( img + p, "DFDT", 4 ) ) {
			*dfdt = img + p + DGD_CHUNK_HDR; *dfdtlen = ps;
		} else if( !memcmp( img + p, "DATA", 4 ) ) {
			*data = img + p + DGD_CHUNK_HDR; *datalen = ps;
		}
		p = DgdAlign16( p + DGD_CHUNK_HDR + ps );
	}
	if( !*info || !*data ) return -1;
	return 0;
}

// Read one part and decrypt it when it is encrypted.
static int DgdReadPart( const char* path, const char* pw, DgdPart* p )
{
	FILE* fp;
	unsigned char* img = NULL;
	unsigned long fsize;
	const unsigned char *info, *dfdt, *data;
	unsigned long infolen, dfdtlen, datalen;
	unsigned char* mat = NULL;
	unsigned long pwlen;

	memset( p, 0, sizeof(*p) );

	fp = fopen( path, "rb" );
	if( !fp ) return -1;
	fseek( fp, 0, SEEK_END );
	fsize = (unsigned long)ftell(fp);
	fseek( fp, 0, SEEK_SET );
	if( fsize < DGD_CHUNK_HDR || fsize > 0x20000000ul ) { fclose(fp); return -1; }
	img = (unsigned char*)malloc( fsize );
	if( !img ) { fclose(fp); return -1; }
	if( fsize != fread( img, 1, fsize, fp ) ) {
		fclose(fp); free(img); return -1;
	}
	fclose(fp);

	// The recovery data of a .dgdr: the words that were damaged are
	// restored before anything else is read.
	if( DgcFindRecovery( img, fsize ) )
		DgcRepairRecovery( img, fsize );

	if( DgdFindChunks( img, fsize, &info, &infolen, &dfdt, &dfdtlen,
					   &data, &datalen )!=0 ) {
		free(img);
		return -1;
	}
	if( infolen < 0x64 ) { free(img); return -1; }

	pwlen = ( pw && *pw ) ? (unsigned long)strlen(pw) : 0;

	// the material: the 96 bytes of the INFO chunk
	mat = (unsigned char*)malloc( 96 );
	if( !mat ) { free(img); return -1; }
	memcpy( mat, info, 96 );
	p->encrypted = ( DgdRd32( mat ) & 1 ) ? 1 : 0;
	if( p->encrypted ) {
		if( !pwlen ) { free(mat); free(img); return -1; }
		// key1: the 92 bytes of the material at INFO+4
		DgcCrypt c;
		c.SetKeyHeader( (const unsigned char*)pw, pwlen );
		c.Decrypt( mat + 4, 92 );
	}

	p->index  = DgdRd32( mat + 4 );
	p->nparts = DgdRd32( mat + 8 );
	p->psize  = DgdRd32( mat + 0x10 );
	p->total  = DgdRd32( mat + 0x18 );
	memcpy( p->sha, mat + 0x20, 64 );

	// the name: the length and the text are encrypted with key2
	if( infolen > 100 ) {
		unsigned long nl = 0;
		unsigned char* nm = (unsigned char*)malloc( infolen - 96 );
		if( nm ) {
			memcpy( nm, info + 96, infolen - 96 );
			if( p->encrypted ) {
				DgcCrypt c;
				c.SetKeyPayload( mat, 96, (const unsigned char*)pw, pwlen );
				c.Decrypt( nm, infolen - 96 );
			}
			nl = DgdRd32( nm );
			if( nl > infolen - 100 ) nl = 0;
			if( nl > DGD_MAX_NAME - 1 ) nl = DGD_MAX_NAME - 1;
			memcpy( p->name, nm + 4, nl );
			p->name[nl] = 0;
			free(nm);
		}
	}

	// the data of this part
	p->datalen = datalen;
	p->data = (unsigned char*)malloc( datalen ? datalen : 1 );
	if( !p->data ) { free(mat); free(img); return -1; }
	memcpy( p->data, data, datalen );
	if( p->encrypted ) {
		DgcCrypt c;
		c.SetKeyPayload( mat, 96, (const unsigned char*)pw, pwlen );
		c.Decrypt( p->data, datalen );
	}

	// the DFDT chunk: the date and the time of the file
	if( dfdt && dfdtlen >= 0x18 ) {
		unsigned char f[8];
		memcpy( f, dfdt + 0x10, 8 );
		if( p->encrypted ) {
			DgcCrypt c;
			c.SetKeyPayload( mat, 96, (const unsigned char*)pw, pwlen );
			c.Decrypt( f, 8 );
		}
		p->mtime = (unsigned long)f[0] | ((unsigned long)f[1]<<8) |
				   ((unsigned long)f[2]<<16) | ((unsigned long)f[3]<<24);
		p->mtime |= (unsigned __int64)f[4] << 32;
		p->mtime |= (unsigned __int64)f[5] << 40;
		p->mtime |= (unsigned __int64)f[6] << 48;
		p->mtime |= (unsigned __int64)f[7] << 56;
	}

	free(mat);
	free(img);
	return 0;
}

// ------------------------------------------------------------------
// The candidates of the directory
// ------------------------------------------------------------------

static DgdPart g_parts[DGD_MAX_PART];
static int g_npart;

static int DgdScan( const char* dir, const char* pw,
					const unsigned char* refhash, unsigned long nparts )
{
	WIN32_FIND_DATAA fd;
	HANDLE h;
	char pat[MAX_PATH];
	int pass;

	g_npart = 0;
	for( pass=0; pass<2; pass++ ) {
		char* e;
		lstrcpyn( pat, dir, sizeof(pat) );
		e = pat + lstrlen(pat);
		while( e>pat && (e[-1]=='\\' || e[-1]=='/') ) e--;
		lstrcpy( e, pass ? "\\*.dgdr" : "\\*.dgd" );
		h = FindFirstFileA( pat, &fd );
		if( h == INVALID_HANDLE_VALUE ) continue;
		do {
			char full[MAX_PATH];
			DgdPart p;
			if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;
			lstrcpyn( full, dir, sizeof(full) );
			e = full + lstrlen(full);
			while( e>full && (e[-1]=='\\' || e[-1]=='/') ) e--;
			lstrcpy( e, "\\" );
			lstrcat( e, fd.cFileName );
			if( DgdReadPart( full, pw, &p )!=0 ) continue;
			if( p.nparts!=nparts || memcmp( p.sha, refhash, 64 ) ) {
				if( p.data ) free( p.data );
				continue;
			}
			if( g_npart >= DGD_MAX_PART ) { if( p.data ) free(p.data); break; }
			g_parts[g_npart] = p;
			lstrcpyn( g_parts[g_npart].path, full, sizeof(g_parts[0].path) );
			g_npart++;
		} while( FindNextFileA( h, &fd ) );
		FindClose( h );
	}
	return g_npart;
}

// Every combination of the candidates that uses one file per part index
// and whose part sizes add up to the size of the whole file.  A .dgd set
// and a .dgdr set hold the same file with different part boundaries, so
// more than one combination can be possible.
#define DGD_MAX_COMBO	64
static int g_combo[DGD_MAX_COMBO][DGD_MAX_PART];
static int g_ncombo;

static void DgdCombos( int idx, unsigned long need, int nparts )
{
	int i;
	if( g_ncombo >= DGD_MAX_COMBO ) return;
	if( idx >= nparts ) {
		if( need == 0 ) g_ncombo++;
		return;
	}
	for( i=0; i<g_npart; i++ ) {
		if( g_parts[i].index != (unsigned long)idx ) continue;
		if( g_parts[i].psize > need ) continue;
		g_combo[g_ncombo][idx] = i;
		DgdCombos( idx + 1, need - g_parts[i].psize, nparts );
		if( g_ncombo >= DGD_MAX_COMBO ) return;
	}
}

// ------------------------------------------------------------------

bool DgdOpen( const char* path, const char* password, DgdFile *f )
{
	DgdPart ref;
	char dir[MAX_PATH];
	int i, c;

	memset( f, 0, sizeof(*f) );

	if( DgdReadPart( path, password, &ref )!=0 ) {
		f->encrypted = ref.encrypted;
		return false;
	}
	f->encrypted = ref.encrypted;

	// the directory of the part
	lstrcpyn( dir, path, sizeof(dir) );
	{
		char* e = dir + lstrlen(dir);
		while( e>dir && (e[-1]!='\\' && e[-1]!='/') ) e--;
		if( e>dir ) e--;
		*e = 0;
	}

	if( ref.nparts == 0 || ref.nparts > DGD_MAX_PART ) {
		// the part count is a file value: DgdCombos indexes
		// g_combo[][part] and would run off its row
		if( ref.data ) free( ref.data );
		return false;
	}
	if( DgdScan( dir, password, ref.sha, ref.nparts )<=0 ) {
		if( ref.data ) free( ref.data );
		return false;
	}
	if( !ref.name[0] ) {
		for( i=0; i<g_npart; i++ ) { if( g_parts[i].data ) free(g_parts[i].data); }
		g_npart = 0;
		if( ref.data ) free( ref.data );
		return false;
	}
	lstrcpyn( f->name, ref.name, sizeof(f->name) );

	// the combinations
	g_ncombo = 0;
	DgdCombos( 0, ref.total, (int)ref.nparts );

	for( c=0; c<g_ncombo; c++ ) {
		unsigned char* buf = (unsigned char*)malloc( ref.total ? ref.total : 1 );
		unsigned char hash[64];
		unsigned long off = 0;
		int ok = 1;
		if( !buf ) break;
		for( i=0; i<(int)ref.nparts; i++ ) {
			DgdPart* p = &g_parts[ g_combo[c][i] ];
			if( off + p->datalen > ref.total ) { ok = 0; break; }
			memcpy( buf + off, p->data, p->datalen );
			off += p->datalen;
		}
		if( ok ) {
			DgdHash( buf, off, hash );
			if( !memcmp( hash, ref.sha, 64 ) ) {
				f->data = buf;
				f->datalen = off;
				f->total = ref.total;
				f->nparts = (int)ref.nparts;
				f->encrypted = ref.encrypted;
				f->mtime = ref.mtime;
				for( i=0; i<g_npart; i++ ) { if( g_parts[i].data ) free(g_parts[i].data); }
				g_npart = 0;
				if( ref.data ) free( ref.data );
				return true;
			}
		}
		free(buf);
	}

	for( i=0; i<g_npart; i++ ) { if( g_parts[i].data ) free(g_parts[i].data); }
	g_npart = 0;
	if( ref.data ) free( ref.data );
	return false;
}

// The information of the file, without putting the parts together.
bool DgdInfo( const char* path, const char* password, DgdFile *f )
{
	DgdPart ref;

	memset( f, 0, sizeof(*f) );
	if( DgdReadPart( path, password, &ref )!=0 ) {
		f->encrypted = ref.encrypted;
		return false;
	}
	lstrcpyn( f->name, ref.name, sizeof(f->name) );
	f->total = ref.total;
	f->nparts = (int)ref.nparts;
	f->encrypted = ref.encrypted;
	f->mtime = ref.mtime;
	if( ref.data ) free( ref.data );
	return true;
}

void DgdClose( DgdFile *f )
{
	if( !f ) return;
	if( f->data ) free( f->data );
	memset( f, 0, sizeof(*f) );
}
