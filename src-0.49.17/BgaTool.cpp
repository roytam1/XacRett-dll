
#include "stdafx.h"
#include "zlib/zlib.h"
#include "libbzip2/bzlib.h"
#include "BgaTool.h"

/****************************************************/

bool CBgaTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return (-1 != FindHeader( hdr,siz>65536 ? 65536 : siz ));
}

bool CBgaTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	BgaHeader hdr;
	unsigned long nextpos;
	char* outname;

	bool result = true;
	FILE *out,*bga = fopen( fname,"rb" );
	if( !bga )
		return false;

	int hpos = FindHeader( buf, fread(buf,1,XACR_BUFSIZE,bga) );
	if( hpos == -1 )
	{
		fclose( bga );
		return false;
	}
	fseek( bga, hpos, SEEK_SET );

	while( result && ReadHeader( bga, &hdr ) )
	{
		nextpos = ftell( bga ) + hdr.compressed_size;

		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(hdr.name))//追加 By Bonty
		{
			fseek( bga, nextpos, SEEK_SET );
			continue;
		}

		out = fopen( outname=kiutil::pathMake(hdr.name),"wb" );
		if( out )
		{
			if( !IsCompressed(&hdr) )
				result = NcDec( bga, out, hdr.original_size );

			else if( 0==memcmp( hdr.type,"GZIP",4 ) )
				result = GzDec( bga, out, hdr.original_size );

			else if( 0==memcmp( hdr.type,"BZ2" ,4 ) )
				result = BzDec( bga, out, hdr.original_size );

			else
				result = false;

			fclose( out );

			kiutil::timeSet( outname, hdr.date, hdr.time );
			SetFileAttributes( outname, hdr.attrib );
		}

		fseek( bga, nextpos, SEEK_SET );
	}

	fclose( bga );
	return true;
}

/****************************************************/

int CBgaTool::FindHeader( const unsigned char* dat, unsigned long siz )
{
	DWORD checksum, fnlen;
	int sum;

	for( int i=0; i<(signed)siz-28; i++ )
	{
		if( dat[i+4]!='G' && dat[i+4]!='B' ) continue;
		if( dat[i+5]!='Z'                  ) continue;
		if( dat[i+6]!='I' && dat[i+6]!='2' ) continue;
		if( dat[i+7]!='P' && dat[i+7]!='\0') continue;

		checksum = (dat[i+0])+(dat[i+1]<<8)+(dat[i+2]<<16)+(dat[i+3]<<24);
		fnlen    = (dat[i+24])+(dat[i+25]<<8)+(dat[i+26])+(dat[i+27]<<8);

		if( i+28+fnlen > (signed)siz )		 continue;

		sum = 0;
		for( int j=i+4; j!=i+28+(signed)fnlen; j++ )
			sum += (char)dat[j];
		if( checksum == (unsigned)sum )
			return i;
	}

	return -1;
}

bool CBgaTool::ReadHeader( FILE* in, BgaHeader* hdr )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	if( 28 != fread( buf,1,28,in ) )
		return false;

	// 一応 Byte-Order-Independent に…する意味あるのかなぁ？(^^;
	hdr->checksum		 = (buf[0])+(buf[1]<<8)+
							(buf[2]<<16)+(buf[3]<<24);
	memcpy( hdr->type, buf+4, 4 );
	hdr->compressed_size = (buf[8])+(buf[9]<<8)+
							(buf[10]<<16)+(buf[11]<<24);
	hdr->original_size	 = (buf[12])+(buf[13]<<8)+
							(buf[14]<<16)+(buf[15]<<24);
	hdr->date			 = (buf[16])+(buf[17]<<8);
	hdr->time			 = (buf[18])+(buf[19]<<8);
	hdr->attrib			 = (buf[20]);
	hdr->header_type	 = (buf[21]);
	hdr->arc_type		 = (buf[22])+(buf[23]<<8);
	hdr->dir_name_len	 = (buf[24])+(buf[25]<<8);
	hdr->file_name_len	 = (buf[26])+(buf[27]<<8);

	// ファイル名( MAX_PATHまでで切ってしまう )
	DWORD fnlen = hdr->dir_name_len + hdr->file_name_len;
	if( fnlen != (unsigned)fread( buf+28, 1, fnlen, in ) )
		return false;

	int len = fnlen > MAX_PATH-1 ? MAX_PATH-1 : fnlen;
	memcpy( hdr->name, buf+28, len );
	hdr->name[ len ] = '\0';

	// チェックサム
	int sum=0;
	for( unsigned i=4; i!=28+fnlen; i++ )
		sum += (char)buf[i];
	return ( hdr->checksum == (unsigned)sum );
}

bool CBgaTool::IsCompressed( BgaHeader* hdr )
{
	if( hdr->arc_type==2 )
		return false;
	else if( hdr->arc_type==0 )
	{
		if( hdr->compressed_size == hdr->original_size )
		{
			const char* ext = kiutil::pathExt( hdr->name );
			if( 0==stricmp( ext,"arc" )
			 || 0==stricmp( ext,"arj" )
			 || 0==stricmp( ext,"bz2" )
			 || 0==stricmp( ext,"bza" )
			 || 0==stricmp( ext,"cab" )
			 || 0==stricmp( ext,"gz"  )
			 || 0==stricmp( ext,"gza" )
			 || 0==stricmp( ext,"lzh" )
			 || 0==stricmp( ext,"lzs" )
			 || 0==stricmp( ext,"pak" )
			 || 0==stricmp( ext,"rar" )
			 || 0==stricmp( ext,"taz" )
			 || 0==stricmp( ext,"tbz" )
			 || 0==stricmp( ext,"tgz" )
			 || 0==stricmp( ext,"z"   )
			 || 0==stricmp( ext,"zip" )
			 || 0==stricmp( ext,"zoo" ) )
				return false;
		}
	}

	return true;
}

/****************************************************/

bool CBgaTool::GzDec( FILE* in, FILE* out, unsigned long origsize )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	// ファイルハンドルを複製してzlibで開く

	int fd = dup( fileno(in) );
	lseek( fd, ftell(in), SEEK_SET );

	gzFile g = gzdopen( fd, "rb" );
	if( !g )
	{
		close( fd );
		return false;
	}

	// zlibを介して読みとり～

	int len = XACR_BUFSIZE > origsize ? origsize : XACR_BUFSIZE;
	while( len = gzread( g, buf, len ) )
	{
		if( len < 0 )
			break;
		fwrite( buf, 1, len, out );
		origsize -= len;
		len = XACR_BUFSIZE > origsize ? origsize : XACR_BUFSIZE;
	}

	gzclose( g );
	return true;
}

bool CBgaTool::BzDec( FILE* in, FILE* out, unsigned long origsize )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	// bzlibに接続

	int err;
	BZFILE* b = BZ2_bzReadOpen( &err, in, 0, 0, NULL, 0 );
	if( err!=BZ_OK || b==NULL )
		return false;

	// bzlibを介して読みとり～

	int len = XACR_BUFSIZE > origsize ? origsize : XACR_BUFSIZE;
	while( len = BZ2_bzRead( &err, b, buf, len ) )
	{
		fwrite( buf, 1, len, out );
		origsize -= len;
		if( err != BZ_OK )
			break;
		len = XACR_BUFSIZE > origsize ? origsize : XACR_BUFSIZE;
	}

	BZ2_bzReadClose( &err, b );
	return true;
}

bool CBgaTool::NcDec( FILE* in, FILE* out, unsigned long origsize )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	// 非圧縮なので自前で読みとり～

	int len = XACR_BUFSIZE > origsize ? origsize : XACR_BUFSIZE;
	while( len = fread( buf, 1, len, in ) )
	{
		if( len < 0 )
			break;
		fwrite( buf, 1, len, out );
		origsize -= len;
		len = XACR_BUFSIZE > origsize ? origsize : XACR_BUFSIZE;
	}

	return true;
}
