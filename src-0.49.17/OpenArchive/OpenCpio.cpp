#include "stdafx.h"
#include "OpenCpio.h"

bool COpenCpio::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	if( !CpioOpen(COpenCpio::szArcFileName) )
		return false;
	return Find_sub(lpSubInfo);
}

bool COpenCpio::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}

bool COpenCpio::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	unsigned long bytes_read = 0;
	unsigned char hdr[2000];
	if( 6!=CpioRead( hdr, 6 ) )
	{CpioClose();return false;}

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
		{CpioClose();return false;}

	bytes_read += 6;

	unsigned long ino,mode,uid,gid,nlink,dev_maj,dev_min,
		rdev_maj,rdev_min,namelen,chksum,dev,rdev;
	if( hdr_type == 1 )
	{
		if( 104!=CpioRead( hdr, 104 ) )
			{CpioClose();return false;}
		hdr[104]='\0';
		if( 13 != sscanf( (char*)hdr,
			"%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx%8lx",
			&ino, &mode, &uid, &gid, &nlink, &mtime, &orisiz,
			&dev_maj, &dev_min, &rdev_maj, &rdev_min, &namelen, &chksum ) )
			{CpioClose();return false;}

		bytes_read += 104;
	}
	else if( hdr_type == 2 )
	{
		if( 70!=CpioRead( hdr, 70 ) )
			{CpioClose();return false;}
		hdr[70]='\0';

		if( 10 != sscanf( (char*)hdr,
			"%6lo%6lo%6lo%6lo%6lo%6lo%6lo%11lo%6lo%11lo",
			&dev, &ino,	&mode, &uid, &gid, &nlink, &rdev, &mtime,
			&namelen, &orisiz ) )
			{CpioClose();return false;}

		bytes_read += 70;
	}
	else if( hdr_type == 3 || hdr_type==4 )
	{
		if( 20!=CpioRead( hdr, 20 ) )
			{CpioClose();return false;}
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
		{CpioClose();return false;}
	bytes_read += namelen;
	namelen = CpioRead( hdr, namelen );
	unsigned long rl = 256 > namelen ? namelen : 256;
	memcpy( outname, hdr, rl );
	outname[ rl ] = '\0';

	if( 0==strcmp( outname, "TRAILER!!!" ) )
		{CpioClose();return false;}

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
	lpSubInfo->dwOriginalSize=orisiz;
	lpSubInfo->dwCompressedSize=SkipBlock();//データ部分飛ばし

	if (isdir)
		return Find_sub(lpSubInfo);

	lstrcpy(lpSubInfo->szFileName,outname);
	
	struct tm* time=localtime((long*)&mtime);
	if( time!=NULL )
	{
		FILETIME ft;
		SYSTEMTIME sys;

		sys.wYear =      time->tm_year+1900;
		sys.wMonth =     time->tm_mon+1;
		sys.wDayOfWeek = time->tm_wday;
		sys.wDay =       time->tm_mday;
		sys.wHour =      time->tm_hour;
		sys.wMinute =    time->tm_min;
		sys.wSecond =    time->tm_sec;
		sys.wMilliseconds = 0;
		SystemTimeToFileTime(&sys,&ft);
		FileTimeToDosDateTime(&ft,&lpSubInfo->wDate,&lpSubInfo->wTime);
	}
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	//lstrcpy(lpSubInfo->szMode,"");
	return true;
}

unsigned long COpenCpio::SkipBlock()
{
	unsigned char* buf = COpenCpio::common_buf;
	unsigned long len,red,clen = 0;

	clen=ftell(COpenCpio::fp);
	while( orisiz )
	{
		len = orisiz > XACR_BUFSIZE ? XACR_BUFSIZE : orisiz;
		red = CpioRead( buf, len );
		orisiz -= red;
		if( red != len )
			break;
	}
	clen=ftell(COpenCpio::fp)-clen;
	return clen;

}

// 普通に開く場合 ////////////////////////////////////////////////////

bool COpenCpio::CpioOpen(const char* fname)
{
	CpioClose();
	fp = fopen( fname,"rb" );
	return (fp!=NULL);
}

size_t COpenCpio::CpioRead(void* red,size_t size)
{
	if( fp==NULL )return 0;
	size_t ans=fread( red,1,size,fp );
	return (ans==-1)?0:ans;
}

void COpenCpio::CpioClose()
{
	if( fp!=NULL )
		fclose(fp),fp=NULL;
}

//////////////////////////////////////////////////////////////////////

bool COpenCpioBz::CpioOpen(const char* fname)
{
	CpioClose();
	bfp=BZ2_bzopen( fname,"rb" );
	return (bfp!=NULL);
}

size_t COpenCpioBz::CpioRead(void* red,size_t size)
{
	if( bfp==NULL )return 0;
	size_t ans=BZ2_bzread( bfp,red,size );
	return (ans==-1)?0:ans;
}

void COpenCpioBz::CpioClose()
{
	if( bfp!=NULL )
		BZ2_bzclose( bfp ),bfp=NULL;
}

//////////////////////////////////////////////////////////////////////

bool COpenCpioZ::CpioOpen(const char* fname)
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

size_t COpenCpioZ::CpioRead(void* red,size_t size)
{
	if( zfp==NULL )return 0;
	size_t ans=Z_read( red,size,zfp );
	return (ans==-1)?0:ans;
}

void COpenCpioZ::CpioClose()
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
bool COpenCpioGz::CpioOpen(const char* fname)
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

size_t COpenCpioGz::CpioRead(void* red,size_t size)
{
	if( gfp==NULL )return 0;
	size_t ans=gzread( gfp,red,size );
	return (ans==-1)?0:ans;
}

void COpenCpioGz::CpioClose()
{
	if( gfp!=NULL )
		gzclose( gfp ),gfp=NULL;
}
