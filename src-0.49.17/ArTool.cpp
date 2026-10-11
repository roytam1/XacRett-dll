
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
	char* strtab=NULL;// "//" string table for "/offset" long names
	long strtablen=0;

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

		date = (unsigned long)strtol( (char*)buf+16, NULL,10 );
		origsiz =(unsigned long)strtol( (char*)buf+48, NULL,10 );
		unsigned long nextpos = ftell(ar) + origsiz + (origsiz&1);

		// string table "//": capture for "/offset" long names, never extract it
		if( buf[0]=='/' && buf[1]=='/' && buf[2]==' ' )
		{
			long tablen = (long)origsiz;
			if(strtab){ delete[] strtab; strtab=NULL; strtablen=0; }
			if( tablen>0 && tablen<0x100000 )
			{
				strtab = new char[tablen+1];
				if( strtab )
				{
					long got = fread( strtab, 1, tablen, ar );
					strtab[got]='\0';
					strtablen = got;
				}
			}
			fseek( ar, nextpos, SEEK_SET );
			continue;
		}
		// linker member "/" and other specials (e.g. "/SYMDEF/"): skip
		if( buf[0]=='/' && (buf[1]==' ' || buf[1]<'0' || buf[1]>'9') )
		{
			fseek( ar, nextpos, SEEK_SET );
			continue;
		}
		if( buf[0]=='/' )
		{
			// "/offset" long name via the string table
			long off = strtol( (char*)buf+1, NULL,10 );
			if( strtab && off>=0 && off<strtablen )
			{
				lstrcpyn(outname, strtab+off, MAX_PATH);
				char* p; for(p=outname;*p && *p!='\n';p=CharNext(p)); *p='\0';
				int n=lstrlen(outname); if(n>0&&outname[n-1]=='/')outname[n-1]='\0';
			}
			else
				wsprintf(outname,"%.16s",buf);
		}
		else
		{
			int i;
			for( i=0; buf[i]!=' ' && i!=16; i++ )
				outname[i] = (char)buf[i];
			outname[i]='\0';
			// strip trailing '/' of the short "name/" form
			if( i>0 && outname[i-1]=='/' ) outname[i-1]='\0';
		}

		if (m_Endflg){ if(strtab) delete[] strtab; return true; }//追加 By Bonty
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

	if(strtab) delete[] strtab;
	fclose( ar );
	return true;
}






