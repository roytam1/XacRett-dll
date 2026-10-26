
#include "stdafx.h"
#include "CpioTool.h"
#include "LzmaTool.h"
#include "XzTool.h"
#include "ZstdTool.h"

bool CCpioTool::Check( const char* fname, unsigned long fsize )
{
	if( !CpioOpen( fname ) )
		return false;

	bool ans = DoCpioHeader();

	CpioClose();
	return ans;
}

bool CCpioTool::Extract( const char* fname,const char* ddir )
{
	if( !CpioOpen( fname ) )
		return false;

	while( DoCpioHeader() )
	{
		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(outname)) continue;//追加 By Bonty

		ProcessData();
	}
	CpioClose();
	return true;
}
/////////////////////////////////////////////////////////////////////

bool CCpioTool::DoCpioHeader()
{
	unsigned long bytes_read = 0;
	unsigned char hdr[2000];
	if( 6!=CpioRead( hdr, 6 ) )
		return false;

	if( 0==memcmp( hdr, "070701", 6 )
	 || 0==memcmp( hdr, "070702", 6 ) )
		hdr_type = 1; // new_ascii
	else if( 0==memcmp( hdr, "070707", 6 ) )
		hdr_type = 2; // old_ascii
	else if( *((unsigned short*)hdr) == 070707 )
		hdr_type = 3; // old_binary
	else if( *((unsigned short*)hdr) == (((070707)&0xff)<<8)+((070707)>>8) )
		hdr_type = 4; // old_binary_swapped;
	else
		return false;

	bytes_read += 6;

	unsigned long ino,mode,uid,gid,nlink,dev_maj,dev_min,
		rdev_maj,rdev_min,namelen,chksum,dev,rdev;
	if( hdr_type == 1 )
	{
		if( 104!=CpioRead( hdr, 104 ) )
			return false;
		hdr[104]='\0';
		if( 13 != sscanf( (char*)hdr,
			"%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx",
			&ino, &mode, &uid, &gid, &nlink, &mtime, &orisiz,
			&dev_maj, &dev_min, &rdev_maj, &rdev_min, &namelen, &chksum ) )
			return false;

		bytes_read += 104;
	}
	else if( hdr_type == 2 )
	{
		if( 70!=CpioRead( hdr, 70 ) )
			return false;
		hdr[70]='\0';

		if( 10 != sscanf( (char*)hdr,
			"%6lo%6lo%6lo%6lo%6lo%6lo%6lo%11lo%6lo%11lo",
			&dev, &ino,	&mode, &uid, &gid, &nlink, &rdev, &mtime,
			&namelen, &orisiz ) )
			return false;

		bytes_read += 70;
	}
	else if( hdr_type == 3 || hdr_type==4 )
	{
		if( 20!=CpioRead( hdr, 20 ) )
			return false;
		if( hdr_type==3 )
		{
			mode = hdr[0] + (hdr[1]<<8);
			rdev = hdr[8] + (hdr[9]<<8);
			mtime = (hdr[10]<<16)+(hdr[11]<<24)+(hdr[12])+(hdr[13]<<8);
			namelen = hdr[14] + (hdr[15]<<8);
			orisiz = (hdr[16]<<16)+(hdr[17]<<24)+(hdr[18])+(hdr[19]<<8);
		}
		else
		{
			mode = (hdr[0]<<8) + hdr[1];
			rdev = (hdr[8]<<8) + hdr[9];
			mtime = (hdr[10]<<24)+(hdr[11]<<16)+(hdr[12]<<8)+(hdr[13]);
			namelen = (hdr[14]<<8) + hdr[15];
			orisiz = (hdr[16]<<24)+(hdr[17]<<16)+(hdr[18]<<8)+(hdr[19]);
		}
	}
	else if( hdr_type == 4 )
	{
	}

	// ファイル名
	if( namelen >= 2000 || namelen==0 )
		return false;
	bytes_read += namelen;
	namelen = CpioRead( hdr, namelen );
	unsigned long rl = 256 > namelen ? namelen : 256;
	memcpy( outname, hdr, rl );
	outname[ rl ] = '\0';

	if( 0==strcmp( outname, "TRAILER!!!" ) )
		return false;

	// ディレクトリかどうか
	isdir = ( (mode & _S_IFMT) == _S_IFDIR );

	// HP/UX の cpio の場合の補正
	if( hdr_type >= 2 )
	{
		switch( mode & 0170000 )
		{
		case 0020000:
		case 0060000:
		case 0140000:
		case 0010000:
			if( (rdev>>8)==0 && (rdev&0xff)==1 )
				orisiz = 0;
		}
	}

	// 境界に合わせる
	unsigned long plusalpha = 0;
	if( hdr_type == 1 )
		plusalpha = (bytes_read%4==0) ? 0 : (4-bytes_read%4);
	else if( hdr_type == 3 )
		plusalpha = (bytes_read%2==0) ? 0 : 1;
	CpioRead( hdr, plusalpha );

	// ヘッダ読み込み完了
	return true;
}

void CCpioTool::ProcessData()
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	unsigned long len,red,plusalpha = 0;
	if( hdr_type == 1 )
		plusalpha = (orisiz%4==0) ? 0 : (4-orisiz%4);
	else if( hdr_type == 3 )
		plusalpha = (orisiz%2==0) ? 0 : 1;

	if( isdir )
		strcat( outname, "\\" );
	char* cn = kiutil::pathMake( outname );
	if( !isdir )
	{
		FILE* out = fopen( cn, "wb" );
		if( out )
		{
			while( orisiz )
			{
				len = orisiz > XACR_BUFSIZE ? XACR_BUFSIZE : orisiz;
				red = CpioRead( buf, len );
				fwrite( buf, 1, red, out );
				orisiz -= red;
				if( red != len )
					break;
			}
			fclose( out );
			kiutil::timeSet( cn, mtime );
		}
	}

	// 4byte/2byte境界に揃える
	orisiz += plusalpha;
	while( orisiz )
	{
		len = orisiz > XACR_BUFSIZE ? XACR_BUFSIZE : orisiz;
		red = CpioRead( buf, len );
		orisiz -= red;
		if( red != len )
			break;
	}
}

// 普通に開く場合 ////////////////////////////////////////////////////

bool CCpioTool::CpioOpen(const char* fname)
{
	CpioClose();
	fp = fopen( fname,"rb" );
	return (fp!=NULL);
}

size_t CCpioTool::CpioRead(void* red,size_t size)
{
	if( fp==NULL )return 0;
	size_t ans=fread( red,1,size,fp );
	return (ans==-1)?0:ans;
}

void CCpioTool::CpioClose()
{
	if( fp!=NULL )
		fclose(fp),fp=NULL;
}

//////////////////////////////////////////////////////////////////////

bool CCpioBzTool::CpioOpen(const char* fname)
{
	CpioClose();
	bfp=BZ2_bzopen( fname,"rb" );
	return (bfp!=NULL);
}

size_t CCpioBzTool::CpioRead(void* red,size_t size)
{
	if( bfp==NULL )return 0;
	size_t ans=BZ2_bzread( bfp,red,size );
	return (ans==-1)?0:ans;
}

void CCpioBzTool::CpioClose()
{
	if( bfp!=NULL )
		BZ2_bzclose( bfp ),bfp=NULL;
}

//////////////////////////////////////////////////////////////////////

bool CCpioZTool::CpioOpen(const char* fname)
{
	CpioClose();

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

size_t CCpioZTool::CpioRead(void* red,size_t size)
{
	if( zfp==NULL )return 0;
	size_t ans=Z_read( red,size,zfp );
	return (ans==-1)?0:ans;
}

void CCpioZTool::CpioClose()
{
	if( zfp!=NULL )
		Z_close( zfp ),zfp=NULL;
}

//////////////////////////////////////////////////////////////////////
/*
struct rpmlead
{
	unsigned char magic[4]; // ed ab ee db
	unsigned char major, minor;
	short type;
	short archnum;
	char name[66];
	short osnum;
	short signature_type;
	char reserved[16];
};
struct rpm_sig_header
{
	unsigned char magic[3]; // 8e ad e8
	unsigned char ver;
	int reserved;
	int index_num;
	int data_len;
};
*/
bool CCpioGzTool::CpioOpen(const char* fname)
{
	CpioClose();

	BYTE Magic[10];
	FILE* fp=fopen( fname,"rb" );
	if( 10!=fread(Magic,1,10,fp) )
		{ fclose(fp); return false; }
	if( Magic[0]==0x1f && (Magic[1]==0x8b || Magic[1]==0x9e) )
	{
		// cpio.gz として開く場合 //////////////////////////////////////
		fclose(fp);
		gfp=gzopen( fname,"rb" );
	}
	else
	{
		// rpm-MagicNumber
		if( Magic[0] != 0xed || Magic[1] != 0xab 
		 || Magic[2] != 0xee || Magic[3] != 0xdb )
			{ fclose(fp); return false; }

		// signature-type を lead 部から取得
		unsigned char lead_rest[96-10], sighdr[16];
		if( 86 != fread( lead_rest, 1, 86, fp ) )
			{ fclose(fp); return false; }
		unsigned short sig = (lead_rest[78-10]<<8) + lead_rest[79-10];

		if( sig == 1 )
			fseek( fp, 256, SEEK_SET );
		else if( sig == 5 )
		{
			// signature解析
			if( 16 != fread( sighdr, 1, 16, fp ) )
				{ fclose(fp); return false; }
			// signature-MagicNumber
			if( sighdr[0] != 0x8e || sighdr[1] != 0xad 
			 || sighdr[2] != 0xe8 )
				{ fclose(fp); return false; }
			// 飛ばす長さ
			int skiplen = 
				16*((sighdr[8]<<24)+(sighdr[9]<<16)+(sighdr[10]<<8)+sighdr[11])
				+ ((sighdr[12]<<24)+(sighdr[13]<<16)+(sighdr[14]<<8)+sighdr[15]);
			// 4byte単位でジャンプ!
			skiplen = (skiplen%8==0) ? skiplen : (skiplen/8+1)*8;
			fseek( fp, skiplen, SEEK_CUR );
		}
		else if( sig != 0 )
			{ fclose(fp); return false; }

		// header解析
		if( 16 != fread( sighdr, 1, 16, fp ) )
			{ fclose(fp); return false; }
		// signature-MagicNumber
		if( sighdr[0] != 0x8e || sighdr[1] != 0xad 
		 || sighdr[2] != 0xe8 )
			{ fclose(fp); return false; }
		// 飛ばす長さ
		int skiplen = 
			16*((sighdr[8]<<24)+(sighdr[9]<<16)+(sighdr[10]<<8)+sighdr[11])
			+ ((sighdr[12]<<24)+(sighdr[13]<<16)+(sighdr[14]<<8)+sighdr[15]);
		fseek( fp, skiplen, SEEK_CUR );

		// rpm として開く場合 ///////////////////////////////////////////
		gfp = gzdopen( fileno(fp),"rb" );
	}

	return (gfp!=NULL);
}

size_t CCpioGzTool::CpioRead(void* red,size_t size)
{
	if( gfp==NULL )return 0;
	size_t ans=gzread( gfp,red,size );
	return (ans==-1)?0:ans;
}

void CCpioGzTool::CpioClose()
{
	if( gfp!=NULL )
		gzclose( gfp ),gfp=NULL;
}

/*********Cpio Read (temp-delegated)*****************/

bool CCpioLzmaTool::CpioOpen(const char* fname)
{
	char tmp[MAX_PATH];
	DWORD n;
	if( tmpPath[0] ) { DeleteFile(tmpPath); tmpPath[0]=0; }
	tmpLive = false;
	n = GetTempPath(MAX_PATH, tmp);
	if( n==0 || n+20>=MAX_PATH ) return false;
	lstrcat(tmp, "XacRettCpioLzma.tmp");
	if( !LzmaDecodeFile(fname, tmp) ) { DeleteFile(tmp); return false; }
	lstrcpy(tmpPath, tmp);
	if( !CCpioTool::CpioOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t CCpioLzmaTool::CpioRead(void* red,size_t size)
{
	return CCpioTool::CpioRead(red, size);
}
void CCpioLzmaTool::CpioClose()
{
	CCpioTool::CpioClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

/*********Cpio Read (temp-delegated)*****************/

bool CCpioXzTool::CpioOpen(const char* fname)
{
	char tmp[MAX_PATH];
	DWORD n;
	if( tmpPath[0] ) { DeleteFile(tmpPath); tmpPath[0]=0; }
	tmpLive = false;
	n = GetTempPath(MAX_PATH, tmp);
	if( n==0 || n+20>=MAX_PATH ) return false;
	lstrcat(tmp, "XacRettCpioXz.tmp");
	if( !XzDecodeFile(fname, tmp) ) { DeleteFile(tmp); return false; }
	lstrcpy(tmpPath, tmp);
	if( !CCpioTool::CpioOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t CCpioXzTool::CpioRead(void* red,size_t size)
{
	return CCpioTool::CpioRead(red, size);
}
void CCpioXzTool::CpioClose()
{
	CCpioTool::CpioClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

/*********Cpio Read (temp-delegated)*****************/

bool CCpioZstTool::CpioOpen(const char* fname)
{
	char tmp[MAX_PATH];
	DWORD n;
	if( tmpPath[0] ) { DeleteFile(tmpPath); tmpPath[0]=0; }
	tmpLive = false;
	n = GetTempPath(MAX_PATH, tmp);
	if( n==0 || n+20>=MAX_PATH ) return false;
	lstrcat(tmp, "XacRettCpioZst.tmp");
	if( !ZstdDecodeFile(fname, tmp) ) { DeleteFile(tmp); return false; }
	lstrcpy(tmpPath, tmp);
	if( !CCpioTool::CpioOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t CCpioZstTool::CpioRead(void* red,size_t size)
{
	return CCpioTool::CpioRead(red, size);
}
void CCpioZstTool::CpioClose()
{
	CCpioTool::CpioClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}
