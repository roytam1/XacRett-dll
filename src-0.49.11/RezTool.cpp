
#include "stdafx.h"
#include "RezTool.h"

bool CRezTool::Check(  const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<=26 )
		return false;
	if( hdr[0]!='R' || hdr[1]!='E' || hdr[2]!='Z' || hdr[3]!='\0' )
		return false;
	return true;
}

bool CRezTool::Extract( const char* fname, const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	// rezのあるディレクトリ保存
	char rezdir[MAX_PATH];
	int y,d;
	kiutil::pathSplit( fname,&y,&d );
	strncpy( rezdir,fname,y+1 ); 
	rezdir[y+1] = '\0';

	// rezを開く
	FILE* rez = fopen( fname,"rb" );
	if( !rez ) return false;

	// 先頭10byte
	// [REZ\0 (4)][split_num (4)][comment_len (2)]
	if( 10!=fread( buf, 1, 10, rez ) )
		{ fclose( rez ); return false; }
	unsigned long split_num =
			(buf[4] | (buf[5]<<8) | (buf[6]<<16) | (buf[7]<<24));
	unsigned short comment_len =
			(buf[8] | (buf[9]<<8));

	// コメント読み捨て
	// [comment with no '\0' (comment_len)]
	if( comment_len!=fread( buf, 1, comment_len, rez ) )
		{ fclose( rez ); return false; }

	// [filename_len (2)]
	if( 2!=fread( buf, 1, 2, rez ) )
		{ fclose( rez ); return false; }
	unsigned short filename_len =
			((buf[0]) | (buf[1]<<8));

	// ファイル名
	// [filename with no '\0' (filename_len)]
	if( filename_len!=fread( buf, 1, filename_len, rez ) )
		{ fclose( rez ); return false; }
	char outname[ MAX_PATH ];
	filename_len = filename_len > MAX_PATH-1 ? MAX_PATH-1 : filename_len;
	memcpy( outname, buf, filename_len );
	outname[ filename_len ] = '\0';
	kiutil::pathMake(outname);

	// [original-size (4)][00 00 ? (2)]
	if( 6!=fread( buf, 1, 6, rez ) )
		{ fclose( rez ); return false; }
	unsigned long original_size =
			(buf[0] | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24));

	FILE *out, *in;
	if( !(out=fopen( outname, "wb" )) )
		{ fclose( rez ); return false; }
	bool ans = true;

	while( original_size )
	{
		// [sp_filename_len (2)]
		if( 2!=fread( buf, 1, 2, rez ) )
			{ ans = false; break; }
		unsigned short sp_filename_len =
				(buf[0] | (buf[1]<<8));

		// [split_file_name with no '\0' (sp_filename_len)]
		if( sp_filename_len!=fread( buf, 1, sp_filename_len, rez ) )
			{ ans = false; break; }
		char name[ MAX_PATH ], inname[ MAX_PATH*2 ];
		sp_filename_len = sp_filename_len > MAX_PATH-1
							? MAX_PATH-1 : sp_filename_len;
		memcpy( name, buf, sp_filename_len );
		name[ sp_filename_len ] = '\0';
		strcpy( inname, rezdir );
		strcat( inname, name );

		// [recode_len (4)][crc (2)]
		if( 6!=fread( buf, 1, 6, rez ) )
			{ ans = false; break; }
		unsigned long recode_len =
				(buf[0] | (buf[1]<<8) | (buf[2]<<16) | (buf[3]<<24));
		unsigned short crc =
				(buf[4] | (buf[5]<<8));

		// 分割ファイルを開く
		if( !(in = fopen( inname,"rb" )) )
			{ ans = false; break; }

		// 読み出す(した)データサイズ
		unsigned long rest = original_size >= recode_len ?
								 recode_len : original_size;
		original_size -= rest;

		// 読み出し
		while( rest )
		{
			if( feof(in) )
				break;

			unsigned long read;
			if( rest>XACR_BUFSIZE )
				read = fread( buf, 1, XACR_BUFSIZE, in );
			else
				read = fread( buf, 1, rest, in );
			fwrite( buf, 1, read, out );
			rest -= read;
		}

		// 読み出し終わり
		fclose( in );
		if( rest!=0 )
			{ ans = false; break; }
	}

	fclose( out );
	fclose( rez );
	return ans;
}
