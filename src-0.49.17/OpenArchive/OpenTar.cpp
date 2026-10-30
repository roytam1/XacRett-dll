#include "stdafx.h"
#include "OpenTar.h"
#include "..\LzmaTool.h"
#include "..\XzTool.h"
#include "..\ZstdTool.h"
#include "..\LzDecode.h"

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
// ---- GNU sparse support (old-GNU 'S' + GNU 1.0 via PAX 'x') ----
// toybox-master/toys/posix/tar.c parity; see TarTool.cpp. The listing
// path never writes data: it only consumes x-headers and reports the
// apparent size, so no 64-bit output seeking is needed here.

#define TAR_SPARSE_MAX_PAIRS 1048576

static unsigned __int64 tar_s2i64(char* d,int l)
{
	unsigned __int64 s=0;
	int i;
	for( i=0; i!=l; i++ )
	{
		if( '0'<=d[i] && d[i]<='7' )
			s = (s<<3)+(d[i]-'0');
		else if( d[i]!=' ' && d[i]!=0 )
			break;
	}
	return s;
}

static unsigned __int64 tar_a2i64(const char* d,int l)
{
	unsigned __int64 s=0;
	int i;
	for( i=0; i!=l; i++ )
	{
		if( '0'<=d[i] && d[i]<='9' )
		{
			if( s > ((~(unsigned __int64)0)-(unsigned __int64)(d[i]-'0'))/10 )
				return ~(unsigned __int64)0;
			s = s*10+(unsigned __int64)(d[i]-'0');
		}
		else break;
	}
	return s;
}

static bool TarSparseAppend(TarSparsePair** pmap, DWORD* n, DWORD* cap,
			    unsigned __int64 off, unsigned __int64 len)
{
	DWORD nc, i;
	TarSparsePair* nm;
	if( *n >= TAR_SPARSE_MAX_PAIRS )
		return false;
	if( *n==*cap )
	{
		nc = (*cap==0) ? 64 : (*cap)*2;
		nm = new TarSparsePair[nc];
		for( i=0; i!=*n; i++ ) { nm[i].off=(*pmap)[i].off; nm[i].len=(*pmap)[i].len; }
		if( *pmap!=NULL ) delete[] *pmap;
		*pmap = nm;
		*cap = nc;
	}
	(*pmap)[*n].off = off;
	(*pmap)[*n].len = len;
	(*n)++;
	return true;
}

bool COpenTar::ReadXHeader(DWORD xsize, char* nameOut, unsigned __int64* realOut)
{
	// toybox parity: payloads over 1MB are skipped, not parsed
	if( xsize>(1<<20) )
	{
		unsigned __int64 sk;
		for( sk=((unsigned __int64)xsize+511)/512; sk>0; )
		{
			DWORD c = sk>1000000 ? 1000000 : (DWORD)sk;
			SkipBlock((int)c);
			sk -= c;
		}
		return true;
	}
	if( xsize==0 ) return true;
	{
		char* payload = new char[xsize];
		DWORD off = 0;
		DWORD p, q, rs, e, rend;
		unsigned __int64 reclen;
		unsigned __int64 total, have;
		char d[512];
		size_t r;
		DWORD chunk;
		while( off<xsize )
		{
			r = TarRead(payload+off, xsize-off);
			if( r==0 ) break;
			off += (DWORD)r;
		}
		p = 0;
		while( p<off )
		{
			// record: "<len> <keyword>=<value>\n", len covers the record
			q = p;
			while( q<off && payload[q]>='0' && payload[q]<='9' ) q++;
			if( q==p || q>=off || payload[q]!=' ' ) break;
			reclen = tar_a2i64(payload+p, (int)(q-p));
			rs = q+1;
			if( reclen<4 || reclen > (unsigned __int64)(off-p) ) break;
			rend = p+(DWORD)reclen;
			if( payload[rend-1]!='\n' ) break;
			e = rs;
			while( e<rend-1 && payload[e]!='=' ) e++;
			if( e<rend-1 )
			{
				if( (e-rs==15 && 0==memcmp(payload+rs, "GNU.sparse.name", 15))
				 || (e-rs==4 && 0==memcmp(payload+rs, "path", 4)) )
				{
					DWORD vlen = rend-1-(e+1);
					DWORD cl = vlen>=MAX_PATH ? MAX_PATH-1 : vlen;
					DWORD k;
					for( k=0; k!=cl; k++ ) nameOut[k] = payload[e+1+k];
					nameOut[cl] = 0;
				}
				else if( e-rs==19 && 0==memcmp(payload+rs, "GNU.sparse.realsize", 19) )
					*realOut = tar_a2i64(payload+e+1, (int)(rend-1-(e+1)));
			}
			p = rend;
		}
		delete[] payload;
		// discard block padding after the payload
		total = ((unsigned __int64)xsize+511)/512*512;
		have = off;
		while( have<total )
		{
			unsigned __int64 want = total-have;
			chunk = want>512 ? 512 : (DWORD)want;
			r = TarRead(d, chunk);
			if( r==0 ) break;
			have += r;
		}
	}
	return true;
}

bool COpenTar::ReadSparseOld(BYTE* hdr, TarSparsePair** pmap, DWORD* npairs,
			       unsigned __int64* apparent, DWORD* mapBlocks)
{
	DWORD n = 0, cap = 0;
	unsigned __int64 end = 0;
	int i;
	BYTE ext;
	*pmap = NULL; *npairs = 0; *apparent = 0; *mapBlocks = 0;
	for( i=0; i!=4; i++ )
	{
		unsigned __int64 off = tar_s2i64((char*)hdr+386+24*i, 12);
		unsigned __int64 len = tar_s2i64((char*)hdr+386+24*i+12, 12);
		if( !TarSparseAppend(pmap, &n, &cap, off, len) ) { if(*pmap!=NULL) delete[] *pmap; *pmap=NULL; return false; }
		if( off+len > end ) end = off+len;
	}
	{
		unsigned __int64 real = tar_s2i64((char*)hdr+483, 12);
		if( real > end ) end = real;
	}
	ext = hdr[482];
	while( ext!=0 )
	{
		BYTE blk[512];
		int j;
		if( 512!=TarRead(blk,512) ) { if(*pmap!=NULL) delete[] *pmap; *pmap=NULL; return false; }
		(*mapBlocks)++;
		for( j=0; j!=21; j++ )
		{
			unsigned __int64 off = tar_s2i64((char*)blk+24*j, 12);
			unsigned __int64 len = tar_s2i64((char*)blk+24*j+12, 12);
			if( !TarSparseAppend(pmap, &n, &cap, off, len) ) { delete[] *pmap; *pmap=NULL; return false; }
			if( off+len > end ) end = off+len;
		}
		ext = blk[504];
	}
	*npairs = n;
	*apparent = end;
	return true;
}

bool COpenTar::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	CloseArchive();
	if( !TarOpen(COpenTar::szArcFileName) )
		{TarClose();return false;}

	return Find_sub(lpSubInfo);
}

bool COpenTar::FindNext(INDIVIDUALINFO *lpSubInfo)
{

	return Find_sub(lpSubInfo);
}

bool COpenTar::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	unsigned char* buf = COpenTar::common_buf;
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

	// pending PAX sparse state for the member the next header describes
	char xName[MAX_PATH];
	bool xHave=false;
	unsigned __int64 xReal=0;
	char curXName[MAX_PATH];
	bool curXHave=false;
	unsigned __int64 curXReal=0;

read:
	if(512!=TarRead(buf,512))
	{TarClose();return false;}

	// ヘッダ読み込み
	head = (TarHeader*)buf;
	if( 0==memcmp( zeroheader,buf,512 ) )
		{TarClose();return false;}

	// PAX sparse state belongs to this header's member, unless the
	// header only carries state itself (x/g accumulate it, L/N keep it)
	curXHave = false; curXReal = 0; curXName[0] = 0;
	if( head->type!='x' && head->type!='g' && head->type!='L' && head->type!='N' )
	{
		curXHave = xHave; curXReal = xReal;
		if( xHave ) lstrcpy(curXName, xName);
		xHave = false; xName[0] = 0; xReal = 0;
	}

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
	case 'V'://とりあえずハンドリング不能と判断したもの全部
		if( realfsize!=0 )
			SkipBlock( (realfsize>>9)+1 );
		goto read;
	case 'x':
	case 'g':
	{
		// PAX extended header: metadata, never a member. The PAX path and
		// GNU sparse keys update the pending state (toybox parity).
		DWORD xs = tar_s2i(head->filesize,12);
		if( head->type=='x' )
		{
			char nm[MAX_PATH]; unsigned __int64 rl;
			nm[0] = 0; rl = xReal;
			ReadXHeader(xs, nm, &rl);
			if( nm[0]!=0 ) { lstrcpy(xName, nm); xHave = true; }
			if( rl!=0 ) xReal = rl;
		}
		else if( xs!=0 )
		{
			unsigned __int64 sk;
			for( sk=((unsigned __int64)xs+511)/512; sk>0; )
			{
				DWORD c = sk>1000000 ? 1000000 : (DWORD)sk;
				SkipBlock((int)c);
				sk -= c;
			}
		}
		goto read;
	}
	case 'S':
	{
		// old-GNU sparse member: list under its apparent size
		TarSparsePair* smap = NULL;
		DWORD snp = 0, smb = 0;
		unsigned __int64 apparent = 0;
		unsigned __int64 stored = tar_s2i64(head->filesize,12);
		char hname[101];
		int hi2;
		DWORD sAttrib, sUpdate;
		char* mname;
		unsigned __int64 sk;
		for( hi2=0; hi2!=100; hi2++ ) hname[hi2] = head->filename[hi2];
		hname[100] = 0;
		sAttrib = tar_s2i(head->attrib,8);
		sUpdate = tar_s2i(head->update,12);
		if( !ReadSparseOld((BYTE*)head, &smap, &snp, &apparent, &smb) ) { if(smap!=NULL) delete[] smap; TarClose(); return false; }
		mname = (curXHave && curXName[0]!=0) ? curXName : (bLongName ? LongBuf : hname);
		bLongName = false;
		if( mname[0]==0 )
		{
			if(smap!=NULL) delete[] smap;
			for( sk=(stored+511)/512; sk>0; )
			{
				DWORD c = sk>1000000 ? 1000000 : (DWORD)sk;
				SkipBlock((int)c);
				sk -= c;
			}
			goto read;
		}
		lpSubInfo->dwCompressedSize=0;
		// house-wide DWORD limit (cf. the 4.4GB 7z member)
		lpSubInfo->dwOriginalSize = (DWORD)apparent;
		{
			int st_mode=sAttrib;
			sAttrib=((st_mode & S_IWRITE)==0)*FA_RDONLY
			+((st_mode & S_IREAD)==0)*FA_HIDDEN
			+ 0 *FA_SYSTEM
			+ 0 *FA_LABEL
			+((st_mode & S_IFDIR)!=0)*FA_DIREC
			+0*FA_ARCH;
		}
		lpSubInfo->szAttribute[0]= (sAttrib & FA_ARCH)	? 'A' : '-';
		lpSubInfo->szAttribute[1]= (sAttrib & FA_SYSTEM)	? 'S' : '-';
		lpSubInfo->szAttribute[2]= (sAttrib & FA_HIDDEN)	? 'H' : '-';
		lpSubInfo->szAttribute[3]= (sAttrib & FA_RDONLY)	? 'R' : '-';
		lpSubInfo->szAttribute[4]='\0';
		lstrcpy(lpSubInfo->szMode,"sparse");
		lstrcpy(lpSubInfo->szFileName,mname);
		{
			struct tm* time=localtime((long*)&sUpdate);
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
		}
		for( sk=(stored+511)/512; sk>0; )
		{
			DWORD c = sk>1000000 ? 1000000 : (DWORD)sk;
			SkipBlock((int)c);
			sk -= c;
		}
		if(smap!=NULL) delete[] smap;
		}
		break;
	case 'L':
	case 'N'://longname
		if( realfsize!=0 && 512==TarRead(buf,512) )
		{
			bLongName=true;
			strncpy(LongBuf,(char*)buf,MAX_PATH);
			SkipBlock( realfsize>>9 );
		}
		goto read;
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
		//ディレクトリメンバだったら飛ばす
		if( *realfname==0 || realattrib&(1<<14))
		{
			if( realfsize!=0 )
				SkipBlock( (realfsize>>9)+1 );
			goto read;
		}
		
		// GNU sparse via PAX (format 1.0): renamed member, apparent size
		if( curXHave && curXName[0]!=0 ) realfname = curXName;
		bool isSparse10 = (curXReal>0);
		lpSubInfo->dwCompressedSize=0;
		// house-wide DWORD limit (cf. the 4.4GB 7z member)
		lpSubInfo->dwOriginalSize = isSparse10 ? (DWORD)curXReal : realfsize;

		int st_mode=realattrib;
		realattrib=((st_mode & S_IWRITE)==0)*FA_RDONLY /* Read Only */
		+((st_mode & S_IREAD)==0)*FA_HIDDEN /* Hidden */
		+ 0 *FA_SYSTEM						/* System File */
		+ 0 *FA_LABEL						/* volume label */
		+((st_mode & S_IFDIR)!=0)*FA_DIREC /* Directory */
		+0*FA_ARCH;							/* Archive Attribute */
		/*lstrcpy(lpSubInfo->szAttribute ,"----");
		if (realattrib & FILE_ATTRIBUTE_READONLY)
			lpSubInfo->szAttribute[2] = 'R';
		if (realattrib & FILE_ATTRIBUTE_HIDDEN)
			lpSubInfo->szAttribute[1] = 'H';
		if (realattrib & FILE_ATTRIBUTE_SYSTEM)
			lpSubInfo->szAttribute[0] = 'S';
		if (realattrib & FILE_ATTRIBUTE_DIRECTORY)
			lpSubInfo->szAttribute[3] = 'D';
		if (realattrib & FILE_ATTRIBUTE_ARCHIVE)
			lpSubInfo->szAttribute[3] = 'A';
			*/
		lpSubInfo->szAttribute[0]= (realattrib & FA_ARCH)	? 'A' : '-'; /* Archive */
		lpSubInfo->szAttribute[1]= (realattrib & FA_SYSTEM)	? 'S' : '-'; /* System */
		lpSubInfo->szAttribute[2]= (realattrib & FA_HIDDEN)	? 'H' : '-'; /* Hide */
		lpSubInfo->szAttribute[3]= (realattrib & FA_RDONLY)	? 'R' : '-'; /* Read Only*/
		lpSubInfo->szAttribute[4]='\0';

		//tar members are stored, not compressed
		lstrcpy(lpSubInfo->szMode, isSparse10 ? "sparse" : "stored");
		lstrcpy(lpSubInfo->szFileName,realfname);

		struct tm* time=localtime((long*)&realupdate);
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
		while( realfsize>>9 )
		{
			TarRead(buf,512);
			realfsize-=512;
		}
		if( realfsize!=0 )
		{
			TarRead(buf,512);
		}

		};
		}break;
	}

	return true;
}

void COpenTar::SkipBlock(int num)
{
	static char dummy[512];
	for( ; num!=0; num-- )
		TarRead(dummy,512);
}

/******ごく普通なTarのRead/Write****************/

bool COpenTar::TarOpen(LPCSTR fname)
{
	TarClose();
	fp = fopen( fname,"rb" );
	return (fp!=NULL);
}

size_t COpenTar::TarRead(void* red,size_t size)
{
	if( fp==NULL )return 0;
	size_t ans=fread( red,1,size,fp );
	return (ans==-1)?0:ans;
}

void COpenTar::TarClose()
{
	if( fp!=NULL )
		fclose(fp),fp=NULL;
}

/*********Tar+Bzip2のRead****************/

bool COpenTbz::TarOpen(LPCSTR fname)
{
	TarClose();
	bfp=BZ2_bzopen( fname,"rb" );
	return (bfp!=NULL);
}

size_t COpenTbz::TarRead(void* red,size_t size)
{
	if( bfp==NULL )return 0;
	size_t ans=BZ2_bzread( bfp,red,size );
	return (ans==-1)?0:ans;
}

void COpenTbz::TarClose()
{
	if( bfp!=NULL )
		BZ2_bzclose( bfp ),bfp=NULL;
}

/*********Tar+GzipのRead*****************/

bool COpenTgz::TarOpen(LPCSTR fname)
{
	TarClose();

	BYTE Magic[10];
	FILE* _fp=fopen( fname,"rb" );
	if( 10!=fread(Magic,1,10,_fp) )
	{fclose(_fp);return false;}
	fclose(_fp);
	if( Magic[0]!=0x1f || (Magic[1]!=0x8b && Magic[1]!=0x9e) )
		return false;

	gfp=gzopen( fname,"rb" );
	return (gfp!=NULL);
}

size_t COpenTgz::TarRead(void* red,size_t size)
{
	if( gfp==NULL )return 0;
	size_t ans=gzread( gfp,red,size );
	return (ans==-1)?0:ans;
}

void COpenTgz::TarClose()
{
	if( gfp!=NULL )
		gzclose( gfp ),gfp=NULL;
}

/*********Tar+CompressのRead*****************/

bool COpenTaZ::TarOpen(LPCSTR fname)
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

size_t COpenTaZ::TarRead(void* red,size_t size)
{
	if( zfp==NULL )return 0;
	size_t ans=Z_read( red,size,zfp );
	return (ans==-1)?0:ans;
}

void COpenTaZ::TarClose()
{
	if( zfp!=NULL )
		Z_close( zfp ),zfp=NULL;
}

/*********Tar+PackのRead*****************/

bool COpenTlzma::TarOpen(LPCSTR fname)
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
	if( !COpenTar::TarOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t COpenTlzma::TarRead(void* red,size_t size)
{
	return COpenTar::TarRead(red, size);
}
void COpenTlzma::TarClose()
{
	COpenTar::TarClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

bool COpenTlz::TarOpen(LPCSTR fname)
{
	char tmp[MAX_PATH];
	DWORD n;
	if( tmpPath[0] ) { DeleteFile(tmpPath); tmpPath[0]=0; }
	tmpLive = false;
	n = GetTempPath(MAX_PATH, tmp);
	if( n==0 || n+20>=MAX_PATH ) return false;
	lstrcat(tmp, "XacRettTlz.tmp");
	if( !LzDecodeFile(fname, tmp) ) { DeleteFile(tmp); return false; }
	lstrcpy(tmpPath, tmp);
	if( !COpenTar::TarOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t COpenTlz::TarRead(void* red,size_t size)
{
	return COpenTar::TarRead(red, size);
}
void COpenTlz::TarClose()
{
	COpenTar::TarClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

bool COpenTxz::TarOpen(LPCSTR fname)
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
	if( !COpenTar::TarOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t COpenTxz::TarRead(void* red,size_t size)
{
	return COpenTar::TarRead(red, size);
}
void COpenTxz::TarClose()
{
	COpenTar::TarClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

bool COpenTzst::TarOpen(LPCSTR fname)
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
	if( !COpenTar::TarOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t COpenTzst::TarRead(void* red,size_t size)
{
	return COpenTar::TarRead(red, size);
}
void COpenTzst::TarClose()
{
	COpenTar::TarClose();
	if( tmpLive ) { DeleteFile(tmpPath); tmpPath[0]=0; tmpLive = false; }
}

bool COpenTaz::TarOpen(LPCSTR fname)
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

size_t COpenTaz::TarRead(void* red,size_t size)
{
	if( pfp==NULL )return 0;
	size_t ans=P_read( red,size,pfp );
	return (ans==-1)?0:ans;
}

void COpenTaz::TarClose()
{
	if( pfp!=NULL )
		P_close( pfp ),pfp=NULL;
}
