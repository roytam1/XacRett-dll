
#include "stdafx.h"
#include "ArTool.h"

bool CArTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<68 )
		return false;

	return (hdr[66]=='`' && hdr[67]=='\n')
		&& ( 0==memcmp( hdr, "!<arch>\n", 8 )
		  || 0==memcmp( hdr, "!<bout>\n", 8 ) );
}

bool CArTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	// 開く
	FILE* ar = fopen( fname, "rb" );
	if( !ar )
		return false;
	if( 8!=fread( buf, 1, 8, ar ) )
	{
		fclose( ar );
		return false;
	}

	// 展開
	char outname[MAX_PATH];
	unsigned long origsiz;
	unsigned long date;

	while( !feof(ar) )
	{
		// filename : 16bytes;
		// date     : 12bytes;
		// uid      :  6
		// gid      :  6
		// mode?    :  8
		// orisize  : 10
		// magic    : must be "`\n"

		// ヘッダ読み込み
		if( 60!=fread( buf, 1, 60, ar ) || buf[58]!='`' || buf[59]!='\n' )
			break;

		// LongFileName
		if( buf[0]=='/' && buf[1]=='/' && buf[2]==' ' )
		{
			long namelen = strtol( (char*)buf+48, NULL,10 );
			long vr = namelen>MAX_PATH-1 ? MAX_PATH-1 : namelen;
			namelen -= fread( outname, 1, vr, ar );
			outname[vr]='\0';
			fseek( ar, namelen, SEEK_CUR );
		}
		else
		{
			int i;
			for( i=0; buf[i]!=' ' && i!=16; i++ )
				outname[i] = (char)buf[i];
			outname[i]='\0';
		}

		date = (unsigned long)strtol( (char*)buf+16, NULL,10 );
		origsiz =(unsigned long)strtol( (char*)buf+48, NULL,10 );
		unsigned long nextpos = ftell(ar) + origsiz + (origsiz&1);

		if (m_Endflg) return true;//追加 By Bonty
		if (!CheckStr(outname))//追加 By Bonty
		{
			fseek( ar, nextpos, SEEK_SET );
			continue;
		}

		// 出力先を開く
		char* cn;
		FILE* out = fopen( cn=kiutil::pathMake(outname), "wb" );
		if( out )
		{
			// そのまま書き出す
			unsigned long l,h;
			while( origsiz )
			{
				l = origsiz>XACR_BUFSIZE ? XACR_BUFSIZE : origsiz;
				h = fread( buf, 1, l, ar );
				fwrite( buf, 1, h, out );
				origsiz -= h;
				if( h!=l )
					break;
			}

			// 閉じる
			fclose( out );
			kiutil::timeSet( cn, date );
		}

		fseek( ar, nextpos, SEEK_SET );
	}

	fclose( ar );
	return true;
}






