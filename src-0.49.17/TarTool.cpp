
#include "stdafx.h"
#include "TarTool.h"
#include "LzmaTool.h"
#include "XzTool.h"
#include "ZstdTool.h"
#include "LzDecode.h"

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

// ---- GNU sparse support (old-GNU 'S' + GNU 1.0 via PAX 'x') ----
// toybox-master/toys/posix/tar.c parity: 'x' records GNU.sparse.name /
// GNU.sparse.realsize; old-GNU pairs are 12-byte octal at raw offset
// 386 (continuations: 21 pairs + flag at 504); the 1.0 inline map is
// decimal text (any non-digit separates, NUL ends), first value is
// the pair count. A trailing zero-length pair extends the file,
// which is how the apparent size materialises.

#define TAR_SPARSE_MAX_PAIRS 1048576

// FSCTL_SET_SPARSE = CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 49, METHOD_BUFFERED,
// FILE_ANY_ACCESS) = 0x900C4; spelled out so no winioctl.h is needed.
#ifndef FSCTL_SET_SPARSE
#define FSCTL_SET_SPARSE 0x000900C4
#endif

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

static bool TarSeekOut(FILE* out, unsigned __int64 off)
{
	HANDLE h;
	LONG lo, hi;
	fflush(out);
	h = (HANDLE)_get_osfhandle(_fileno(out));
	if( h==(HANDLE)-1 ) return false;
	lo = (LONG)(off & 0xFFFFFFFF);
	hi = (LONG)(off >> 32);
	if( SetFilePointer(h, lo, &hi, FILE_BEGIN)==0xFFFFFFFF && GetLastError()!=NO_ERROR )
		return false;
	return true;
}

bool CTarTool::ReadXHeader(DWORD xsize, char* nameOut, unsigned __int64* realOut)
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

bool CTarTool::ReadSparseOld(BYTE* hdr, TarSparsePair** pmap, DWORD* npairs,
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

bool CTarTool::ReadSparseMap10(TarSparsePair** pmap, DWORD* npairs, DWORD* mapBlocks)
{
	// streaming decimal map: first value sizes the zeroed table (toybox
	// parity: NUL ends without storing, any other non-digit stores)
	DWORD count = 0, stored = 0;
	unsigned __int64 v = 0;
	bool have = false, sized = false;
	BYTE blk[512];
	DWORD bi = 512, bn = 512;
	*pmap = NULL; *npairs = 0; *mapBlocks = 0;
	for(;;)
	{
		BYTE c;
		if( bi>=bn )
		{
			if( 512!=TarRead(blk,512) ) return false;
			bi = 0; bn = 512;
			(*mapBlocks)++;
		}
		c = blk[bi++];
		if( c>='0' && c<='9' )
		{
			if( v > ((~(unsigned __int64)0)-(unsigned __int64)(c-'0'))/10 ) return false;
			v = v*10+(unsigned __int64)(c-'0');
			have = true;
		}
		else if( c==0 )
			break;
		else if( !have )
			continue;
		else if( !sized )
		{
			DWORD k;
			if( v > TAR_SPARSE_MAX_PAIRS ) return false;
			count = (DWORD)v;
			sized = true;
			v = 0; have = false;
			if( count!=0 )
			{
				*pmap = new TarSparsePair[count];
				for( k=0; k!=count; k++ ) { (*pmap)[k].off = 0; (*pmap)[k].len = 0; }
			}
		}
		else
		{
			if( stored/2 >= count ) break;
			if( (stored&1)==0 ) (*pmap)[stored/2].off = (unsigned __int64)v;
			else (*pmap)[stored/2].len = (unsigned __int64)v;
			stored++;
			v = 0; have = false;
		}
	}
	if( !sized ) return true;
	*npairs = count;
	return true;
}

bool CTarTool::WriteSparseData(FILE* out, TarSparsePair* map, DWORD npairs,
			    unsigned __int64 apparent, unsigned __int64 budget)
{
	unsigned char data[512];
	unsigned __int64 dataRead = 0;
	unsigned __int64 pos = 0;
	DWORD i;
	DWORD chunk;
	// best effort: let the FS keep holes unallocated where supported
	{
		HANDLE h = (HANDLE)_get_osfhandle(_fileno(out));
		if( h!=(HANDLE)-1 )
		{
			DWORD br = 0;
			DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &br, NULL);
		}
	}
	if( npairs==0 )
	{
		// degenerate map: dense copy of the stored bytes (toybox parity)
		unsigned __int64 left = budget;
		while( left>0 )
		{
			DWORD chunk = left>512 ? 512 : (DWORD)left;
			if( chunk!=TarRead(data,chunk) ) return false;
			if( chunk!=fwrite(data,1,chunk,out) ) return false;
			left -= chunk;
			pos += chunk;
		}
	}
	else
	{
		for( i=0; i!=npairs; i++ )
		{
			unsigned __int64 off = map[i].off;
			unsigned __int64 len = map[i].len;
			// a zero-length pair moves the position only; the
			// alignment discard below still applies uniformly
			if( dataRead+len > budget || dataRead+len < dataRead ) return false;
			if( !TarSeekOut(out, off) ) return false;
			while( len>0 )
			{
				chunk = len>512 ? 512 : (DWORD)len;
				if( chunk!=TarRead(data,chunk) ) return false;
				if( chunk!=fwrite(data,1,chunk,out) ) return false;
				dataRead += chunk;
				pos += chunk;
				len -= chunk;
			}
			// GNU packs each run block-aligned: discard the leftover
			while( (pos & 511)!=0 )
			{
				chunk = (DWORD)(512-(pos & 511));
				if( chunk!=TarRead(data,chunk) ) return false;
				pos += chunk;
			}
		}
	}
	if( pos < budget )
	{
		// trailing data padding after the last aligned run
		unsigned __int64 left = budget-pos;
		while( left>0 )
		{
			DWORD chunk = left>512 ? 512 : (DWORD)left;
			if( chunk!=TarRead(data,chunk) ) return false;
			left -= chunk;
		}
	}
	fflush(out);
	{
		HANDLE h = (HANDLE)_get_osfhandle(_fileno(out));
		LONG lo, hi;
		if( h==(HANDLE)-1 ) return false;
		lo = (LONG)(apparent & 0xFFFFFFFF);
		hi = (LONG)(apparent >> 32);
		if( SetFilePointer(h, lo, &hi, FILE_BEGIN)==0xFFFFFFFF && GetLastError()!=NO_ERROR )
			return false;
		if( !SetEndOfFile(h) ) return false;
	}
	return true;
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

	// pending PAX sparse state for the member the next header describes
	char xName[MAX_PATH];
	bool xHave=false;
	unsigned __int64 xReal=0;
	char curXName[MAX_PATH];
	bool curXHave=false;
	unsigned __int64 curXReal=0;

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

		// PAX sparse state belongs to this header's member, unless the
		// header only carries state itself (x/g accumulate it, L/N keep it)
		curXHave = false; curXReal = 0; curXName[0] = 0;
		if( head->type!='x' && head->type!='g' && head->type!='L' && head->type!='N' )
		{
			curXHave = xHave; curXReal = xReal;
			if( xHave ) lstrcpy(curXName, xName);
			xHave = false; xName[0] = 0; xReal = 0;
		}

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
			continue;
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
			continue;
		}
		case 'S':
		{
			// old-GNU sparse member: inline pairs + continuations
			TarSparsePair* smap = NULL;
			DWORD snp = 0, smb = 0;
			unsigned __int64 apparent = 0;
			unsigned __int64 stored = tar_s2i64(head->filesize,12);
			char hname[101];
			int hi2;
			DWORD sAttrib, sUpdate;
			unsigned __int64 budget;
			FILE* sout;
			char c_soutname[MAX_PATH];
			unsigned __int64 sk;
			for( hi2=0; hi2!=100; hi2++ ) hname[hi2] = head->filename[hi2];
			hname[100] = 0;
			sAttrib = tar_s2i(head->attrib,8);
			sUpdate = tar_s2i(head->update,12);
			if( !ReadSparseOld((BYTE*)head, &smap, &snp, &apparent, &smb) )
				return false;
			{
				char* mname = (curXHave && curXName[0]!=0) ? curXName : (bLongName ? LongBuf : hname);
				bLongName = false;
				if( m_Endflg ) { if(smap!=NULL) delete[] smap; return true; }
				if( !CheckStr(mname) )
				{
					if(smap!=NULL) delete[] smap;
					for( sk=(stored+511)/512; sk>0; )
					{
						DWORD c = sk>1000000 ? 1000000 : (DWORD)sk;
						SkipBlock((int)c);
						sk -= c;
					}
					continue;
				}
				{
					char* oname = kiutil::pathMake( mname );
					sout = fopen( oname,"wb" );
					lstrcpy(c_soutname,oname);
				}
				if( sout!=NULL )
				{
					if( (unsigned __int64)smb*512 > stored )
					{ fclose(sout); DeleteFile(c_soutname); if(smap!=NULL) delete[] smap; return false; }
					// old-GNU continuation blocks sit outside the stored
					// size, so the data budget is the full stored size
					budget = stored;
					if( !WriteSparseData(sout, smap, snp, apparent, budget) )
					{ fclose(sout); DeleteFile(c_soutname); if(smap!=NULL) delete[] smap; return false; }
					fclose( sout );
					kiutil::timeSet( c_soutname,sUpdate );
					{
						int st_mode=sAttrib;
						sAttrib=((st_mode & S_IWRITE)==0)*FA_RDONLY
						+((st_mode & S_IREAD)==0)*FA_HIDDEN
						+ 0 *FA_SYSTEM
						+ 0 *FA_LABEL
						+((st_mode & S_IFDIR)!=0)*FA_DIREC
						+0*FA_ARCH;
						SetFileAttributes(c_soutname,sAttrib);
					}
				}
				else
				{
					for( sk=(stored+511)/512; sk>0; )
					{
						DWORD c = sk>1000000 ? 1000000 : (DWORD)sk;
						SkipBlock((int)c);
						sk -= c;
					}
				}
				if(smap!=NULL) delete[] smap;
			}
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
		// GNU sparse via PAX (format 1.0): the x-header renamed this
		// member and gave its apparent size; its data starts with a map
		if( curXHave && curXName[0]!=0 ) realfname = curXName;
		bool isSparse10 = (curXReal>0);
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
				if( isSparse10 )
				{
					// GNU 1.0 sparse data: decimal map first, then the runs
					TarSparsePair* map10 = NULL;
					DWORD np10 = 0, mb10 = 0;
					unsigned __int64 stored10 = tar_s2i64(head->filesize,12);
					unsigned __int64 end10 = 0;
					unsigned __int64 apparent10;
					unsigned __int64 budget10;
					DWORD k10;
					bool ok10 = ReadSparseMap10(&map10, &np10, &mb10);
					if( ok10 )
					{
						for( k10=0; k10!=np10; k10++ )
							if( map10[k10].off+map10[k10].len > end10 ) end10 = map10[k10].off+map10[k10].len;
						apparent10 = curXReal>end10 ? curXReal : end10;
						if( (unsigned __int64)mb10*512 > stored10 ) ok10 = false;
						else { budget10 = stored10-(unsigned __int64)mb10*512; ok10 = WriteSparseData(out, map10, np10, apparent10, budget10); }
					}
					if( map10!=NULL ) delete[] map10;
					if( !ok10 ) { fclose(out); DeleteFile(c_outname); return false; }
				}
				else
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

bool CLzTarTool::TarOpen(const char* fname)
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
	if( !CTarTool::TarOpen(tmpPath) ) { DeleteFile(tmpPath); tmpPath[0]=0; return false; }
	tmpLive = true;
	return true;
}
size_t CLzTarTool::TarRead(void* red,size_t size)
{
	return CTarTool::TarRead(red, size);
}
void CLzTarTool::TarClose()
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
