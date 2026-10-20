#include "stdafx.h"
#include "CabDecode.h"
#include "zlib/zlib.h"

// The largest data block: the uncompressed size is a 16 bit field, so
// 64 KB is the most that can ever come out of one block.
#define CAB_BLOCK_MAX	65536
#define CAB_DATA_MAX	65536

// How much of the file is scanned for the "MSCF" signature, so that a
// self extracting archive with the cabinet at the end is found too.
#define CAB_SCAN_MAX	(150*1024)

static unsigned long cabGet32( const unsigned char *p )
{
	return (unsigned long)p[0] | ((unsigned long)p[1]<<8) |
		   ((unsigned long)p[2]<<16) | ((unsigned long)p[3]<<24);
}

static unsigned int cabGet16( const unsigned char *p )
{
	return (unsigned int)p[0] | ((unsigned int)p[1]<<8);
}

// The checksum of a data block: the 32 bit words of the data XORed
// together, with the bytes that are left over packed into the last word.
static unsigned long cabChecksum( const unsigned char *data,
								  unsigned long bytes, unsigned long cksum )
{
	unsigned long i, ul = 0;

	for( i=0; i+4<=bytes; i+=4 ) {
		cksum ^= cabGet32( data+i );
	}
	switch( bytes & 3 ) {
	case 3:	ul |= (unsigned long)data[i]<<16; i++;	/*@fallthrough@*/
	case 2:	ul |= (unsigned long)data[i]<<8;  i++;	/*@fallthrough@*/
	case 1:	ul |= (unsigned long)data[i];
	}
	return cksum ^ ul;
}

// Read a NUL terminated string.  The string is cut off at the end of
// the buffer, so a name that is too long cannot run over it.
static bool cabReadString( FILE *fp, char *out, int outsz )
{
	int n = 0;
	int c;

	for(;;) {
		c = fgetc( fp );
		if( c==EOF ) return false;
		if( c==0 ) break;
		if( n<outsz-1 ) out[n++] = (char)c;
	}
	out[n] = 0;
	return true;
}

long CabFindHeader( FILE *fp, unsigned long size )
{
	BYTE* buf;
	unsigned long want = ( size>CAB_SCAN_MAX ? CAB_SCAN_MAX : size );
	unsigned long i;
	long result = -1;

	if( want<8 ) return -1;
	buf = new BYTE[want];
	if( !buf ) return -1;
	if( fseek( fp, 0, SEEK_SET )!=0 ) {
		delete [] buf;
		return -1;
	}
	if( want!=fread( buf, 1, want, fp ) ) {
		delete [] buf;
		return -1;
	}
	for( i=0; i+8<=want; i++ ) {
		if( buf[i]=='M' && buf[i+1]=='S' && buf[i+2]=='C' && buf[i+3]=='F' &&
			buf[i+4]==0 && buf[i+5]==0 && buf[i+6]==0 && buf[i+7]==0 ) {
			result = (long)i;
			break;
		}
	}
	delete [] buf;
	return result;
}

const char *CabCompName( unsigned int typeCompress )
{
	switch( typeCompress & 0x0F ) {
	case CAB_COMP_NONE:		return "stored";
	case CAB_COMP_MSZIP:	return "MSZIP";
	case CAB_COMP_QUANTUM:	return "Quantm";
	case CAB_COMP_LZX:		return "LZX";
	}
	return "?";
}

bool CabReadHeader( FILE *fp, long base, CabArchive *cab )
{
	unsigned char hdr[36];
	unsigned char ext[4];
	char name[CAB_MAX_NAME];
	int i;

	memset( cab, 0, sizeof(*cab) );
	if( fseek( fp, base, SEEK_SET )!=0 ) return false;
	if( 36!=fread( hdr,1,36,fp ) ) return false;
	if( memcmp( hdr, "MSCF", 4 )!=0 ) return false;

	cab->baseOffset = (unsigned long)base;
	cab->cbCabinet  = cabGet32( hdr+8 );
	cab->coffFiles  = cabGet32( hdr+16 );
	cab->cFolders   = cabGet16( hdr+26 );
	cab->cFiles     = cabGet16( hdr+28 );
	cab->flags      = cabGet16( hdr+30 );
	cab->setID      = cabGet16( hdr+32 );
	cab->iCabinet   = cabGet16( hdr+34 );

	if( cab->cFolders==0 || cab->cFiles==0 ) return false;
	if( cab->cFolders>4096 || cab->cFiles>65535 ) return false;

	// the sizes of the three reserve areas, and the header reserve area
	if( cab->flags & 4 ) {
		if( 4!=fread( ext,1,4,fp ) ) return false;
		cab->cbCFHeader = cabGet16( ext );
		cab->cbCFFolder = ext[2];
		cab->cbCFData   = ext[3];
		if( cab->cbCFHeader>60000 ) return false;
		if( cab->cbCFHeader ) {
			if( fseek( fp, (long)cab->cbCFHeader, SEEK_CUR )!=0 ) return false;
		}
	}

	// the names of the cabinets of the set, and of the disks they are on
	if( cab->flags & 1 ) {
		if( !cabReadString( fp, cab->prevCabinet, (int)sizeof(cab->prevCabinet) ) ) return false;
		if( !cabReadString( fp, name, (int)sizeof(name) ) ) return false;
	}
	if( cab->flags & 2 ) {
		if( !cabReadString( fp, cab->nextCabinet, (int)sizeof(cab->nextCabinet) ) ) return false;
		if( !cabReadString( fp, name, (int)sizeof(name) ) ) return false;
	}

	// the folders
	cab->nFolders = (int)cab->cFolders;
	cab->folders = new CabFolderEntry[cab->nFolders];
	if( !cab->folders ) {
		cab->nFolders = 0;
		return false;
	}
	for( i=0; i<cab->nFolders; i++ ) {
		unsigned char fb[8];
		if( 8!=fread( fb,1,8,fp ) ) { CabFreeHeader( cab ); return false; }
		cab->folders[i].coffCabStart  = cabGet32( fb );
		cab->folders[i].cCFData       = cabGet16( fb+4 );
		cab->folders[i].typeCompress  = cabGet16( fb+6 );
		if( cab->cbCFFolder ) {
			if( fseek( fp, (long)cab->cbCFFolder, SEEK_CUR )!=0 ) { CabFreeHeader( cab ); return false; }
		}
	}

	// the files
	if( fseek( fp, base+(long)cab->coffFiles, SEEK_SET )!=0 ) { CabFreeHeader( cab ); return false; }
	cab->nFiles = (int)cab->cFiles;
	cab->files = new CabFileEntry[cab->nFiles];
	if( !cab->files ) {
		cab->nFiles = 0;
		CabFreeHeader( cab );
		return false;
	}
	for( i=0; i<cab->nFiles; i++ ) {
		unsigned char fb[16];
		if( 16!=fread( fb,1,16,fp ) ) { CabFreeHeader( cab ); return false; }
		cab->files[i].size    = cabGet32( fb );
		cab->files[i].offset  = cabGet32( fb+4 );
		cab->files[i].folder  = cabGet16( fb+8 );
		cab->files[i].date    = cabGet16( fb+10 );
		cab->files[i].time    = cabGet16( fb+12 );
		cab->files[i].attribs = cabGet16( fb+14 );
		if( !cabReadString( fp, cab->files[i].name, (int)sizeof(cab->files[i].name) ) ) {
			CabFreeHeader( cab );
			return false;
		}
	}
	return true;
}

void CabFreeHeader( CabArchive *cab )
{
	if( !cab ) return;
	if( cab->folders ) delete [] cab->folders;
	if( cab->files ) delete [] cab->files;
	cab->folders = NULL;
	cab->files = NULL;
	cab->nFolders = 0;
	cab->nFiles = 0;
}

// ------------------------------------------------------------------
// The reader for the uncompressed stream of one folder.
// ------------------------------------------------------------------
struct CabFolderReader
{
	FILE*	fp;
	int		method;
	unsigned long	blocksLeft;
	unsigned long	blockFilePos;	// where the next block header is
	unsigned long	streamPos;		// the bytes given out so far
	unsigned char*	blockBuf;		// the uncompressed data of the block
	unsigned long	blockLen;
	unsigned long	blockPos;
	unsigned short	blockResv;
	unsigned short	folderResv;
	// the MSZIP state: the window is shared by the blocks of a folder
	z_stream	zs;
	bool		zReady;
	unsigned char*	inBuf;
};

static void cabCloseFolder( CabFolderReader *r )
{
	if( r->zReady ) {
		inflateEnd( &r->zs );
		r->zReady = false;
	}
	if( r->blockBuf ) delete [] r->blockBuf;
	if( r->inBuf ) delete [] r->inBuf;
	r->blockBuf = NULL;
	r->inBuf = NULL;
}

static bool cabOpenFolder( FILE *fp, CabArchive *cab, int index, CabFolderReader *r )
{
	memset( r, 0, sizeof(*r) );
	r->fp = fp;
	r->method = cab->folders[index].typeCompress & 0x0F;
	r->blocksLeft = cab->folders[index].cCFData;
	r->blockFilePos = (unsigned long)cab->baseOffset + cab->folders[index].coffCabStart;
	r->blockResv = cab->cbCFData;
	r->folderResv = cab->cbCFFolder;

	r->blockBuf = new BYTE[CAB_BLOCK_MAX];
	r->inBuf = new BYTE[CAB_DATA_MAX];
	if( !r->blockBuf || !r->inBuf ) {
		cabCloseFolder( r );
		return false;
	}

	switch( r->method )
	{
	case CAB_COMP_NONE:
	case CAB_COMP_MSZIP:
		break;
	default:
		// Quantum and LZX are not decoded yet; refuse them rather
		// than write something that is not the file
		cabCloseFolder( r );
		return false;
	}
	return true;
}

// Read the next data block of the folder and unpack it.
static bool cabNextBlock( CabFolderReader *r )
{
	unsigned char hdr[8];
	unsigned char resv[64];
	unsigned long csum;
	unsigned int cbData, cbUncomp;
	bool reset = false;

	if( r->blocksLeft==0 ) return false;
	r->blocksLeft--;

	if( fseek( r->fp, (long)r->blockFilePos, SEEK_SET )!=0 ) return false;
	if( 8!=fread( hdr,1,8,r->fp ) ) return false;
	csum     = cabGet32( hdr );
	cbData   = cabGet16( hdr+4 );
	cbUncomp = cabGet16( hdr+6 );
	r->blockFilePos += 8;

	// the reserve area of the block.  Its first byte is the size of
	// the area and the second one the flags, where bit 0 means that
	// the compression history is reset before this block.
	if( r->blockResv ) {
		unsigned int n = r->blockResv;
		if( n>sizeof(resv) ) return false;
		if( n!=fread( resv,1,n,r->fp ) ) return false;
		r->blockFilePos += n;
		if( n>=2 && (resv[1] & 1) ) reset = true;
	}

	if( cbData>CAB_DATA_MAX || cbUncomp>CAB_BLOCK_MAX ) return false;
	if( cbData!=(unsigned int)fread( r->inBuf, 1, cbData, r->fp ) ) return false;
	r->blockFilePos += cbData;

	// the checksum of the block
	if( csum ) {
		unsigned long sum = cabChecksum( r->inBuf, cbData, 0 );
		sum = cabChecksum( hdr+4, 4, sum );
		if( sum!=csum ) return false;
	}

	r->blockLen = 0;
	r->blockPos = 0;

	switch( r->method )
	{
	case CAB_COMP_NONE:
		if( cbData!=cbUncomp ) return false;
		memcpy( r->blockBuf, r->inBuf, cbData );
		r->blockLen = cbData;
		break;

	case CAB_COMP_MSZIP: {
		int ret;
		if( cbData<2 ) return false;
		if( r->inBuf[0]!='C' || r->inBuf[1]!='K' ) return false;

		if( reset ) {
			// the history is dropped, so the window has to start
			// empty again
			if( r->zReady ) {
				inflateEnd( &r->zs );
				r->zReady = false;
			}
		}
		if( !r->zReady ) {
			memset( &r->zs, 0, sizeof(r->zs) );
			if( inflateInit2( &r->zs, -MAX_WBITS )!=Z_OK ) return false;
			r->zReady = true;
		} else {
			// the window of the previous block is kept, which is
			// what makes MSZIP better than a plain deflate stream.
			// inflateReset would drop it, so the "keep" version is
			// the one that is wanted here.
			if( inflateResetKeep( &r->zs )!=Z_OK ) return false;
		}

		r->zs.next_in = r->inBuf+2;
		r->zs.avail_in = cbData-2;
		r->zs.next_out = r->blockBuf;
		r->zs.avail_out = CAB_BLOCK_MAX;
		for(;;) {
			ret = inflate( &r->zs, Z_NO_FLUSH );
			if( ret==Z_STREAM_END ) break;
			if( ret!=Z_OK ) return false;
			if( r->zs.avail_out==0 || r->zs.avail_in==0 ) return false;
		}
		r->blockLen = CAB_BLOCK_MAX - r->zs.avail_out;
		if( r->blockLen!=cbUncomp ) return false;
		break;
	}

	default:
		return false;
	}
	return true;
}

// Give "len" bytes of the folder's uncompressed stream.
static bool cabReadBytes( CabFolderReader *r, BYTE *buf, unsigned long len )
{
	while( len>0 ) {
		unsigned long n;
		if( r->blockPos>=r->blockLen ) {
			if( !cabNextBlock( r ) ) return false;
		}
		n = r->blockLen - r->blockPos;
		if( n>len ) n = len;
		memcpy( buf, r->blockBuf+r->blockPos, n );
		r->blockPos += n;
		r->streamPos += n;
		buf += n;
		len -= n;
	}
	return true;
}

// ------------------------------------------------------------------

bool CabCanDecode( const CabArchive *cab )
{
	int i;
	if( !cab || cab->nFolders<=0 ) return false;
	for( i=0; i<cab->nFolders; i++ ) {
		switch( cab->folders[i].typeCompress & 0x0F ) {
		case CAB_COMP_NONE:
		case CAB_COMP_MSZIP:
			break;
		default:
			return false;
		}
	}
	return true;
}

// Set the date and the time of a file that was written.
static void cabSetTime( const char *path, unsigned short date, unsigned short time )
{
	FILETIME ft, lc;
	HANDLE h;

	if( !DosDateTimeToFileTime( date, time, &lc ) ) return;
	if( !LocalFileTimeToFileTime( &lc, &ft ) ) return;
	h = CreateFile( path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
					OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if( h==INVALID_HANDLE_VALUE ) return;
	SetFileTime( h, &ft, NULL, &ft );
	CloseHandle( h );
}

// Is the file with the index "b" stored before the one with "a"?
static bool cabFileBefore( CabArchive *cab, int a, int b )
{
	if( cab->files[a].folder!=cab->files[b].folder ) {
		return cab->files[a].folder < cab->files[b].folder;
	}
	return cab->files[a].offset < cab->files[b].offset;
}

bool CabExtractAll( FILE *fp, CabArchive *cab, void *user,
					CabFileCallback callback )
{
	int* order;
	int i, f;
	bool ok = true;

	if( !cab || cab->nFiles<=0 || cab->nFolders<=0 ) return false;

	// the files are written in the order they are stored in, so that
	// the data of a folder is only unpacked once
	order = new int[cab->nFiles];
	if( !order ) return false;
	for( i=0; i<cab->nFiles; i++ ) order[i] = i;
	for( i=1; i<cab->nFiles; i++ ) {
		int j = i;
		int k = order[i];
		while( j>0 && cabFileBefore( cab, k, order[j-1] ) ) {
			order[j] = order[j-1];
			j--;
		}
		order[j] = k;
	}

	for( f=0; f<cab->nFolders && ok; f++ ) {
		CabFolderReader reader;
		if( !cabOpenFolder( fp, cab, f, &reader ) ) { ok = false; break; }
		for( i=0; i<cab->nFiles; i++ ) {
			int fi = order[i];
			CabFileEntry* fe = &cab->files[fi];
			char path[MAX_PATH];
			FILE* out;

			if( (int)fe->folder!=f ) continue;

			// the data of the file starts at its own offset inside
			// the folder, so anything before it is skipped
			if( fe->offset>reader.streamPos ) {
				BYTE skip[4096];
				unsigned long left = fe->offset-reader.streamPos;
				while( left>0 ) {
					unsigned long n = ( left>sizeof(skip) ? sizeof(skip) : left );
					if( !cabReadBytes( &reader, skip, n ) ) { ok = false; break; }
					left -= n;
				}
				if( !ok ) break;
			}

			if( !callback( user, fe, path, (int)sizeof(path) ) ) continue;

			out = fopen( path, "wb" );
			if( !out ) { ok = false; break; }
			{
				BYTE buf[4096];
				unsigned long left = fe->size;
				while( left>0 ) {
					unsigned long n = ( left>sizeof(buf) ? sizeof(buf) : left );
					if( !cabReadBytes( &reader, buf, n ) ) { ok = false; break; }
					if( n!=fwrite( buf, 1, n, out ) ) { ok = false; break; }
					left -= n;
				}
			}
			fclose( out );
			if( !ok ) {
				DeleteFile( path );
				break;
			}
			// the date and the time and the attributes of the file
			cabSetTime( path, fe->date, fe->time );
			if( fe->attribs & (_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_ARCH) ) {
				SetFileAttributes( path,
					fe->attribs & (_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_ARCH) );
			}
		}
		cabCloseFolder( &reader );
	}
	delete [] order;
	return ok;
}
