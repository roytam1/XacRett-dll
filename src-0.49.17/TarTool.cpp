
#include "stdafx.h"
#include "TarTool.h"
#include "LzmaTool.h"
#include "XzTool.h"
#include "ZstdTool.h"

struct TarHeader
{
	char filename[100];
	char attrib[8];
	char userID[8];	// 無視(^^;
	char groupID[8];// 無視(^^;
	char filesize[12];
	char update[12];//更新日時
	char checksum[8];
	char type;
	char linkedfilename[100];
	char magic_and_ver[8];
	char username[32];	// 無視(^^;
	char groupname[32];	// 無視(^^;
	char deviceNoM[8];
	char deviceNom[8];
//		以下GNU Tar形式
	char lastaccessdate[12];
	char creationdate[12];
	char offset[12];
};

static DWORD tar_s2i(char* d,int l)
{
	DWORD s=0;
	for( int i=0; i!=l; i++ )
	{
		if( '0'<=d[i] && d[i]<='7' )
			s = (s<<3)+(d[i]-'0');
		else if( d[i]!=' ' )
			break;
	}
	return s;
}

static bool CheckSum(TarHeader* hd)
{
	BYTE* dat=(BYTE*)hd;
	DWORD sum=0,i=0;
	for( ; i!=148; i++ )sum += dat[i];
	for( ; i!=156; i++ )sum += ' ';
	for( ; i!=512; i++ )sum += dat[i];
	return sum==tar_s2i(hd->checksum,8);
}

bool CTarTool::Check( const char* fname, unsigned long fsize )
{
	if( !TarOpen(fname) )
		return false;
	bool ans=true;

	BYTE hdr[512];
	// ヘッダチェック
	if( 512!=TarRead(hdr,512) )
		ans=false;
	else
	{
		TarHeader* head=(TarHeader*)hdr;
		if( !CheckSum(head) )
			ans=false;
	}
	TarClose();
	return ans;
}

bool CTarTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	if( !TarOpen(fname) )
		return false;

	// 展開
	TarHeader* head;
	bool bLongName=false;
	char LongBuf[MAX_PATH];
	BYTE zeroheader[512];
	memset( zeroheader,0,sizeof(zeroheader) );

	char* realfname;
	DWORD realfsize;
	DWORD realattrib;
	DWORD realupdate;
	DWORD realcreatetime;

	while( 512==TarRead(buf,512) )
	{
		// ヘッダ読み込み
		head = (TarHeader*)buf;
		if( 0==memcmp( zeroheader,buf,512 ) )
			break;

		// ファイルネーム処理
		if( bLongName )	realfname=LongBuf;
		else			realfname=head->filename;
		bLongName = false;

		// ブロックサイズ
		realfsize=tar_s2i(head->filesize,12);

		// タイプで処理分け
		switch( head->type )
		{
		case '1':
		case '2':
		case '3':
		case '4':
		case '6':
		case '7':
		case 'M':
		case 'S':
		case 'V'://とりあえずハンドリング不能と判断したもの全部
			if( realfsize!=0 )
				SkipBlock( (realfsize>>9)+1 );
			continue;
		case 'L':
		case 'N'://longname
			if( realfsize!=0 && 512==TarRead(buf,512) )
			{
				bLongName=true;
				strncpy(LongBuf,(char*)buf,MAX_PATH);
				SkipBlock( realfsize>>9 );
			}
			continue;
		case '5':
		case 'D'://ディレクトリだと思う
			realattrib|=(1<<14);
		default:{//展開
			realattrib = tar_s2i(head->attrib,8);
			realupdate = tar_s2i(head->update,12);
			if( head->magic_and_ver[6]==' '
			 && head->magic_and_ver[7]==' ' )
				realcreatetime = tar_s2i(head->creationdate,12);
			else
				realcreatetime = realupdate;

			// ファイル名が変なら飛ばす
			if( *realfname==0 )
			{
				if( realfsize!=0 )
					SkipBlock( (realfsize>>9)+1 );
				continue;
			}
			if (m_Endflg) return true;//追加 By Bonty
			if (!CheckStr(realfname))//追加 By Bonty
			{
				//ディレクトリメンバだったら飛ばす
				if( *realfname==0 || realattrib&(1<<14) )
				{
					if( realfsize!=0 )
						SkipBlock( (realfsize>>9)+1 );
					continue;
				}
				while( realfsize>>9 )
				{
					TarRead(buf,512);
					realfsize-=512;
				}
				if( realfsize!=0 )
				{
					TarRead(buf,512);
				}
				continue;
			}
			char* outname = kiutil::pathMake( realfname );

			//ディレクトリメンバだったら飛ばす
			if( realattrib&(1<<14) )
			{
				if( realfsize!=0 )
					SkipBlock( (realfsize>>9)+1 );
				continue;
			}

			FILE* out = fopen( outname,"wb" );
			char c_outname[MAX_PATH];
			lstrcpy(c_outname,outname);
			if( out!=NULL  )
			{
				while( realfsize>>9 )
				{
					TarRead(buf,512);
					fwrite( buf,1,512,out );
					realfsize-=512;
				}
				if( realfsize!=0 )
				{
					TarRead(buf,512);
					fwrite( buf,1,realfsize,out );
				}

				fclose( out );

				kiutil::timeSet( c_outname,realupdate );

				int st_mode=realattrib;
				realattrib=((st_mode & S_IWRITE)==0)*FA_RDONLY /* Read Only */
				+((st_mode & S_IREAD)==0)*FA_HIDDEN /* Hidden */
				+ 0 *FA_SYSTEM						/* System File */
				+ 0 *FA_LABEL						/* volume label */
				+((st_mode & S_IFDIR)!=0)*FA_DIREC /* Directory */
				+0*FA_ARCH;							/* Archive Attribute */
				SetFileAttributes(c_outname,realattrib);
			}
			else
			{
				// エラーなら飛ばす
				if( realfsize!=0 )
					SkipBlock( (realfsize>>9)+1 );
			}
			}break;
		}
	}

	TarClose();
	return true;
}

void CTarTool::SkipBlock(int num)
{
	static char dummy[512];
	for( ; num!=0; num-- )
		TarRead(dummy,512);
}

/******ごく普通なTarのRead/Write****************/

bool CTarTool::TarOpen(const char* fname)
{
	TarClose();
	fp = fopen( fname,"rb" );
	return (fp!=NULL);
}

size_t CTarTool::TarRead(void* red,size_t size)
{
	if( fp==NULL )return 0;
	size_t ans=fread( red,1,size,fp );
	return (ans==-1)?0:ans;
}

void CTarTool::TarClose()
{
	if( fp!=NULL )
		fclose(fp),fp=NULL;
}

/*********Tar+Bzip2のRead****************/

bool CTbzTool::TarOpen(const char* fname)
{
	TarClose();
	bfp=BZ2_bzopen( fname,"rb" );
	return (bfp!=NULL);
}

size_t CTbzTool::TarRead(void* red,size_t size)
{
	if( bfp==NULL )return 0;
	size_t ans=BZ2_bzread( bfp,red,size );
	return (ans==-1)?0:ans;
}

void CTbzTool::TarClose()
{
	if( bfp!=NULL )
		BZ2_bzclose( bfp ),bfp=NULL;
}

/*********Tar+GzipのRead*****************/

bool CTgzTool::TarOpen(const char* fname)
{
	TarClose();

	BYTE Magic[10];
	FILE* fp=fopen( fname,"rb" );
	if( 10!=fread(Magic,1,10,fp) )
	{fclose(fp);return false;}
	fclose(fp);
	if( Magic[0]!=0x1f || (Magic[1]!=0x8b && Magic[1]!=0x9e) )
		return false;

	gfp=gzopen( fname,"rb" );
	return (gfp!=NULL);
}

size_t CTgzTool::TarRead(void* red,size_t size)
{
	if( gfp==NULL )return 0;
	size_t ans=gzread( gfp,red,size );
	return (ans==-1)?0:ans;
}

void CTgzTool::TarClose()
{
	if( gfp!=NULL )
		gzclose( gfp ),gfp=NULL;
}

/*********Tar+CompressのRead*****************/

bool CTaZTool::TarOpen(const char* fname)
{
	TarClose();

	BYTE Magic[10];
	FILE* fp=fopen( fname,"rb" );
	if( 10!=fread(Magic,1,10,fp) )
	{fclose(fp);return false;}
	fclose(fp);
	if( Magic[0]!=0x1f || Magic[1]!=0x9d )
		return false;

	zfp=Z_open( fname,"rb" );
	return (zfp!=NULL);
}

size_t CTaZTool::TarRead(void* red,size_t size)
{
	if( zfp==NULL )return 0;
	size_t ans=Z_read( red,size,zfp );
	return (ans==-1)?0:ans;
}

void CTaZTool::TarClose()
{
	if( zfp!=NULL )
		Z_close( zfp ),zfp=NULL;
}

/*********Tar+PackのRead*****************/

bool CTlzmaTarTool::TarOpen(const char* fname)
{
	char tmp[MAX_PATH];
	DWORD n;
	if( tmpPath[0] ) { DeleteFile(tmpPath); tmpPath[0]=0; }
	tmpLive = false;
	n = GetTempPath(MAX_PATH, tmp);
	if( n==0 || n+20>=MAX_PATH ) return false;
	lstrcat(tmp, "XacRettTlzma.tmp");
	if( !LzmaDecodeFile(fname, tmp) ) { DeleteFile(tmp); return false; }
	lstrcpy(tmpPath, tmp);
	if( !CTarTool::TarOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t CTlzmaTarTool::TarRead(void* red,size_t size)
{
	return CTarTool::TarRead(red, size);
}
void CTlzmaTarTool::TarClose()
{
	CTarTool::TarClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

bool CTxzTool::TarOpen(const char* fname)
{
	char tmp[MAX_PATH];
	DWORD n;
	if( tmpPath[0] ) { DeleteFile(tmpPath); tmpPath[0]=0; }
	tmpLive = false;
	n = GetTempPath(MAX_PATH, tmp);
	if( n==0 || n+20>=MAX_PATH ) return false;
	lstrcat(tmp, "XacRettTxz.tmp");
	if( !XzDecodeFile(fname, tmp) ) { DeleteFile(tmp); return false; }
	lstrcpy(tmpPath, tmp);
	if( !CTarTool::TarOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t CTxzTool::TarRead(void* red,size_t size)
{
	return CTarTool::TarRead(red, size);
}
void CTxzTool::TarClose()
{
	CTarTool::TarClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

bool CTzstTool::TarOpen(const char* fname)
{
	char tmp[MAX_PATH];
	DWORD n;
	if( tmpPath[0] ) { DeleteFile(tmpPath); tmpPath[0]=0; }
	tmpLive = false;
	n = GetTempPath(MAX_PATH, tmp);
	if( n==0 || n+20>=MAX_PATH ) return false;
	lstrcat(tmp, "XacRettTzst.tmp");
	if( !ZstdDecodeFile(fname, tmp) ) { DeleteFile(tmp); return false; }
	lstrcpy(tmpPath, tmp);
	if( !CTarTool::TarOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t CTzstTool::TarRead(void* red,size_t size)
{
	return CTarTool::TarRead(red, size);
}
void CTzstTool::TarClose()
{
	CTarTool::TarClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

bool CTazTool::TarOpen(const char* fname)
{
	TarClose();

	BYTE Magic[10];
	FILE* fp=fopen( fname,"rb" );
	if( 10!=fread(Magic,1,10,fp) )
	{fclose(fp);return false;}
	fclose(fp);
	if( Magic[0]!=0x1f || Magic[1]!=0x1e )
		return false;

	pfp=P_open( fname,"rb" );
	return (pfp!=NULL);
}

size_t CTazTool::TarRead(void* red,size_t size)
{
	if( pfp==NULL )return 0;
	size_t ans=P_read( red,size,pfp );
	return (ans==-1)?0:ans;
}

void CTazTool::TarClose()
{
	if( pfp!=NULL )
		P_close( pfp ),pfp=NULL;
}
