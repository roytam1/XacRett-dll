
#include "stdafx.h"
#include "ZipTool.h"
#include "LzmaDecode.h"
#include "XzDecode.h"
#include "libbzip2/bzlib.h"
#include "zlib/zlib.h"
extern "C" {
#include "pkdcl/pkdcl.h"
#include "lzma/LzmaDec.h"
#include "lzma/Lzma2Dec.h"
#include "xceedbwt/xceedbwt.h"
#include "zstd/zstd.h"
}


//-- CZipTool 部分 -----------------------

bool CZipTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	//--------------------------------------------------------------------//

	int i;
	for( i=0; i<(signed)siz-30; i++ )
	{
		if( hdr[i+0] != 'P'  )continue;
		if( hdr[i+1] != 'K'  )continue;
		if( hdr[i+2] != 0x03 )continue;
		if( hdr[i+3] != 0x04 )continue;
		if( hdr[i+8]+(hdr[i+9]<<8) > BZip2 && hdr[i+8]+(hdr[i+9]<<8)!=Lzma && hdr[i+8]+(hdr[i+9]<<8)!=Aes && hdr[i+8]+(hdr[i+9]<<8)!=Ppmd && hdr[i+8]+(hdr[i+9]<<8)!=Bwt && hdr[i+8]+(hdr[i+9]<<8)!=Xz && hdr[i+8]+(hdr[i+9]<<8)!=Zstd )continue;
		if( hdr[i+26]==0 && hdr[i+27]==0 )continue;
		break;
	}
	if( (unsigned)i+30>=siz )
		return false;

	// 念のため読み込んでみて確かめる
	if( !(zip=fopen( fname, "rb" )) )
		return false;
	fseek( zip, i+4, SEEK_SET );
	ZipLocalHeader zhdr;
	bool ans = read_header( &zhdr );
	fclose( zip );

	return ans;
}

/* Central-directory size lookup for data-descriptor members, whose local
   sizes are zero. Matches by local-header offset. Returns false when
   unresolvable (spans, ZIP64 central, garbage) so callers refuse cleanly. */
static bool ZipCentralSizes(FILE *zip, DWORD localpos, DWORD *csz, DWORD *usz, DWORD *crc)
{
	long save = ftell(zip);
	long fsize, tailsz, i, pos;
	unsigned char *tail;
	fseek(zip, 0, SEEK_END);
	fsize = ftell(zip);
	if (fsize < 22) return false;
	tailsz = fsize > 66000 ? 66000 : fsize;
	tail = new unsigned char[tailsz];
	fseek(zip, fsize - tailsz, SEEK_SET);
	if ((long)fread(tail, 1, tailsz, zip) != tailsz) { delete [] tail; return false; }
	long eo = -1;
	for (i = tailsz - 22; i >= 0; i--) {
		if (tail[i]=='P' && tail[i+1]=='K' && tail[i+2]==0x05 && tail[i+3]==0x06) { eo = i; break; }
	}
	if (eo < 0) { delete [] tail; return false; }
	DWORD cdoff = (DWORD)(tail[eo+16]|(tail[eo+17]<<8)|(tail[eo+18]<<16)|(tail[eo+19]<<24));
	WORD thisdisk = (WORD)(tail[eo+4]|(tail[eo+5]<<8));
	WORD cddisk = (WORD)(tail[eo+6]|(tail[eo+7]<<8));
	if (cdoff == 0xFFFFFFFF || thisdisk != 0 || cddisk != 0) { delete [] tail; return false; }
	pos = (long)cdoff;
	bool found = false;
	while (pos >= 0 && (unsigned long)pos < (unsigned long)fsize) {
		unsigned char fixed[46];
		fseek(zip, pos, SEEK_SET);
		if (46 != fread(fixed, 1, 46, zip)) break;
		if (!(fixed[0]=='P' && fixed[1]=='K' && fixed[2]==0x01 && fixed[3]==0x02)) break;
		DWORD fnl = (DWORD)(fixed[28]|(fixed[29]<<8));
		DWORD exl = (DWORD)(fixed[30]|(fixed[31]<<8));
		DWORD fcl = (DWORD)(fixed[32]|(fixed[33]<<8));
		DWORD loff = (DWORD)(fixed[42]|(fixed[43]<<8)|(fixed[44]<<16)|(fixed[45]<<24));
		if (loff == localpos) {
			*csz = (DWORD)(fixed[20]|(fixed[21]<<8)|(fixed[22]<<16)|(fixed[23]<<24));
			*usz = (DWORD)(fixed[24]|(fixed[25]<<8)|(fixed[26]<<16)|(fixed[27]<<24));
			*crc = (DWORD)(fixed[16]|(fixed[17]<<8)|(fixed[18]<<16)|(fixed[19]<<24));
			found = true;
			break;
		}
		pos += 46 + (long)fnl + (long)exl + (long)fcl;
	}
	delete [] tail;
	fseek(zip, save, SEEK_SET);
	return found;
}

bool CZipTool::Extract( const char* fname,const char* ddir )
{
	char password[128] = {'\0'};
	ZipLocalHeader hdr;

	if( !(zip=fopen(fname,"rb")) )
		return false;

	while( doHeader( &hdr,CTool::common_buf ) )
	{
		if ((hdr.flg & 8) && hdr.csz == 0 && hdr.usz == 0) {
			/* Data descriptor: resolve real sizes from central dir by offset. */
			unsigned long hdrpos = (unsigned long)ftell(zip) - (30 + hdr.fnl + hdr.exl);
			DWORD csz, usz, crc;
			if (!ZipCentralSizes(zip, hdrpos, &csz, &usz, &crc) ||
			    csz > 0x7FFFFFFF || usz > 0x7FFFFFFF) { fclose(zip); return false; }
			hdr.csz = csz; hdr.usz = usz; hdr.crc = crc;
		}
		unsigned long posnext = ftell( zip ) + hdr.csz;

		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(hdr.fnm))//追加 By Bonty
		{
			fseek( zip, posnext, SEEK_SET );
			continue;
		}

		if (hdr.flg & 0x0800) { /* bit-11 UTF-8 names go through ACP conversion */
			char tmp[MAX_PATH];
			if (kiutil::utf8toAnsi(tmp, sizeof(tmp), hdr.fnm))
				lstrcpy(hdr.fnm, tmp);
		}
		char *name = kiutil::pathMake( hdr.fnm );
		if( out=fopen(name,"wb") )
		{
			write_init();   // 出力CRCをクリア
			DWORD aesInner = 0;
			bool isAes = (hdr.mhd==Aes);
			if( isAes ) // WinZip AES: password + CTR layer, then the inner method
			{
				if( *password=='\0' || !aes_init(password,&hdr,&aesInner) )
				{
					int i;
					for( i=0; i!=3; i++ )
					{
						strncpy( password, GetPassword(), sizeof(password)-1 );
						if( aes_init( password, &hdr, &aesInner ) )
							break;
					}
					if( i==3 )
					{
						fclose( out );
						DeleteFile( name );
						break;
					}
				}
				// refuse unsupported inner methods cleanly: skip, keep going
				if( hdr.aesMethod==Tokenized || hdr.aesMethod==Aes || hdr.aesMethod==Bwt ||
				    (hdr.aesMethod>BZip2 && hdr.aesMethod!=Lzma && hdr.aesMethod!=Ppmd && hdr.aesMethod!=Xz && hdr.aesMethod!=Zstd) )
					{ fclose( out ); DeleteFile( name ); needaes = false; fseek( zip, posnext, SEEK_SET ); continue; }
			}
			else 			if( hdr.flg&1 ) // 暗号化されてるのでパスワードをセット
			{
				if( *password=='\0' || !read_init(password,&hdr) )
				{
					int i;
					for( i=0; i!=3; i++ )
					{
						strncpy( password, GetPassword(), sizeof(password)-1 );
						if( read_init( password, &hdr ) )
							break;
					}
					if( i==3 ) // パスワード不一致
					{
						fclose( out );
						DeleteFile( name );
						break;
					}
				}
			}
			else // パスワードを使わない
				read_init();

			// 展開！
			WORD decMhd = isAes ? hdr.aesMethod : hdr.mhd;
			DWORD decCsz = isAes ? aesInner : hdr.csz;
			switch( decMhd )
			{
			case Stored:	 Unstore( hdr.usz, decCsz );	break;
			case Deflated:	 Inflate( hdr.usz, decCsz, false );	break;
			case Shrunk:	Unshrink( hdr.usz, decCsz );	break;
			case Reduced1:	Unreduce( hdr.usz, decCsz, 1 );	break;
			case Reduced2:	Unreduce( hdr.usz, decCsz, 2 );	break;
			case Reduced3:	Unreduce( hdr.usz, decCsz, 3 );	break;
			case Reduced4:	Unreduce( hdr.usz, decCsz, 4 );	break;
			case Imploded:
				Explode( hdr.usz, decCsz,
							 0!=(hdr.flg&0x02), 0!=((hdr.flg)&0x04) );
				break;
			case EnhDeflated:Inflate( hdr.usz, decCsz, true );	break;
			case DclImploded:UnDcl( hdr.usz, decCsz );	break;
			case Lzma:UnLzma( hdr.usz, decCsz );	break;
			case BZip2:		 UnBZip2( hdr.usz, decCsz );		break;
			case Ppmd:UnPpmd( hdr.usz, decCsz );	break;
			case Bwt:UnBwt( hdr.usz, decCsz, hdr.crc );	break;
			case Zstd:UnZstd( hdr.usz, decCsz );	break;
			case Xz:UnXz( hdr.usz, decCsz );	break;
			}

			// 事後処理
			fclose( out );
			if( isAes ) // verify the WinZip auth tag, then drop the CTR state
			{
				BYTE tag[10], expect[10];
				bool aok = (10==fread(tag,1,10,zip));
				iz_aes_auth( &aesctx, expect );
				iz_aes_wipe( &aesctx, sizeof(aesctx) );
				needaes = false;
				if( !aok || 0!=memcmp(tag,expect,10) )
					{ DeleteFile( name ); fclose( zip ); return false; }
			}

			kiutil::timeSet( name, (WORD)hdr.dat,(WORD)hdr.tim );
		}
		else
		{
			/* Refuse loudly instead of silently skipping, except for
			   directory entries which have no file content. An empty
			   name (dir stripped by -j) with no data is skipped. */
			if (lstrlen(name) == 0) {
				if (hdr.csz == 0) { fseek(zip, posnext, SEEK_SET); continue; }
				fclose(zip); return false;
			}
			size_t nl = lstrlen(name);
			bool isdir = (nl > 0 && (name[nl-1] == '/' || name[nl-1] == '\\'));
			if (!isdir) {
				DWORD at = GetFileAttributes(name);
				isdir = (at != 0xFFFFFFFF && (at & FILE_ATTRIBUTE_DIRECTORY));
			}
			if (!isdir)
				{ fclose(zip); return false; }
		}
		if(kiutil::getfilesize(name)<hdr.usz)
		{
			fclose(zip);
			return false;
		}
		fseek( zip, posnext, SEEK_SET );
	}

	fclose( zip );
	return true;
}

//---- CRC 関係 ---------------------

// make crc table
//static void zip_make_crc_table()
//{
//	for( DWORD c,n=0; n!=256; n++ )
//	{
//		c = n;
//		for( DWORD k=8; k; k-- )
//			c = (c&1) ? ((0xedb88320L)^(c>>1)) : (c>>1);
//		crc_table[n] = c;
//	}
//}

// the result

DWORD CZipTool::crc32( DWORD crc, const BYTE* dat, int len )
{
	return ZipCrypto::Crc( crc, dat, len );
}

//-- パスワード解除関係 ------------------------

#define RAND_HEAD_LEN (12)
bool CZipTool::read_init( const char* pwd, ZipLocalHeader* hdr )
{
	if( pwd == NULL )
	{
		needdec = false;
		needaes = false;
		return true;
	}

	needdec = true;
	needaes = false;

	// このパスワードで初期化
	zc.Init( pwd );

	// 今の位置を記憶
	int x = ftell( zip );

	// passwordチェック部読み込み
	BYTE eh[RAND_HEAD_LEN];
	bool ans = false;
	if( RAND_HEAD_LEN == fread( eh, 1, RAND_HEAD_LEN, zip ) )
	{
		zc.DecodeBuffer( eh, RAND_HEAD_LEN );

		// The password check byte is a single byte taken from the PKWARE encryption header.
		BYTE checkerval = BYTE(hdr->exl ? hdr->tim>>8 : hdr->crc>>24);
		BYTE checkedval = eh[RAND_HEAD_LEN-1];
		if( checkedval == checkerval )
			ans = true;
	}

	if( ans ) // チェック部を読んだ分、圧縮サイズを変更
		hdr->csz = hdr->csz>RAND_HEAD_LEN ? hdr->csz-RAND_HEAD_LEN : 0;
	else	  // チェック部の分、戻る
		fseek( zip, x, SEEK_SET );
	return ans;
}

//-- ヘッダ処理 -----------------------------------

bool CZipTool::read_header( ZipLocalHeader* hdr,unsigned char* buf )
{
	bool del = (buf==NULL);
	if( buf==NULL ) buf = new unsigned char[65536];

	if( 26 != fread(buf,1,26,zip) )
		{ if(del)delete [] buf; return false; }

	hdr->ver = ((buf[ 0])|(buf[ 1]<<8));
	hdr->flg = ((buf[ 2])|(buf[ 3]<<8));
	hdr->mhd = ((buf[ 4])|(buf[ 5]<<8));
	hdr->tim = ((buf[ 6])|(buf[ 7]<<8));
	hdr->dat = ((buf[ 8])|(buf[ 9]<<8));
	hdr->crc = ((buf[10])|(buf[11]<<8)|(buf[12]<<16)|(buf[13]<<24));
	hdr->csz = ((buf[14])|(buf[15]<<8)|(buf[16]<<16)|(buf[17]<<24));
	hdr->usz = ((buf[18])|(buf[19]<<8)|(buf[20]<<16)|(buf[21]<<24));
	hdr->fnl = ((buf[22])|(buf[23]<<8));
	hdr->exl = ((buf[24])|(buf[25]<<8));
	hdr->aesStrength = 0;
	hdr->aesMethod = 0;

	if( hdr->fnl!=fread(buf,1,hdr->fnl,zip) )
		{ if(del)delete [] buf; return false; }

	int l = (hdr->fnl>MAX_PATH-1)?(MAX_PATH-1):(hdr->fnl);
	memcpy( hdr->fnm, buf, l );
	hdr->fnm[l]='\0';
	char* pp;
	for( pp=hdr->fnm; *pp!=0; pp++ );

	if( (hdr->mhd > BZip2 && hdr->mhd!=Lzma && hdr->mhd!=Aes && hdr->mhd!=Ppmd && hdr->mhd!=Bwt && hdr->mhd!=Xz && hdr->mhd!=Zstd) || hdr->mhd==Tokenized )
		{ if(del)delete [] buf; return false; }
	if( pp-hdr->fnm != l )
		{ if(del)delete [] buf; return false; }
	if( hdr->exl != fread(buf,1,hdr->exl,zip) )
		{ if(del)delete [] buf; return false; }

	/* ZIP64: resolve 0xFFFFFFFF sizes from extra field 0x0001.
	   Only members fitting signed-long positioning are accepted;
	   anything else is refused (never hang on unbounded streams). */
	if ((hdr->usz == 0xFFFFFFFF || hdr->csz == 0xFFFFFFFF) && hdr->exl >= 4) {
		DWORD off = 0;
		while (off + 4 <= (DWORD)hdr->exl) {
			WORD tag = (WORD)(buf[off] | (buf[off+1] << 8));
			WORD sz = (WORD)(buf[off+2] | (buf[off+3] << 8));
			if (tag == 1 && off + 4 + (DWORD)sz <= (DWORD)hdr->exl) {
				DWORD epos = off + 4, end = off + 4 + (DWORD)sz;
				if (hdr->usz == 0xFFFFFFFF) {
					if (epos + 8 > end) { if(del)delete [] buf; return false; }
					DWORD lo = (DWORD)(buf[epos]|(buf[epos+1]<<8)|(buf[epos+2]<<16)|(buf[epos+3]<<24));
					DWORD hi = (DWORD)(buf[epos+4]|(buf[epos+5]<<8)|(buf[epos+6]<<16)|(buf[epos+7]<<24));
					if (hi != 0 || lo > 0x7FFFFFFF) { if(del)delete [] buf; return false; }
					hdr->usz = lo; epos += 8;
				}
				if (hdr->csz == 0xFFFFFFFF) {
					if (epos + 8 > end) { if(del)delete [] buf; return false; }
					DWORD lo = (DWORD)(buf[epos]|(buf[epos+1]<<8)|(buf[epos+2]<<16)|(buf[epos+3]<<24));
					DWORD hi = (DWORD)(buf[epos+4]|(buf[epos+5]<<8)|(buf[epos+6]<<16)|(buf[epos+7]<<24));
					if (hi != 0 || lo > 0x7FFFFFFF) { if(del)delete [] buf; return false; }
					hdr->csz = lo; epos += 8;
				}
				break;
			}
			off += 4 + (DWORD)sz;
		}
	}
	if (hdr->usz == 0xFFFFFFFF || hdr->csz == 0xFFFFFFFF ||
	    hdr->usz > 0x7FFFFFFF || hdr->csz > 0x7FFFFFFF)
		{ if(del)delete [] buf; return false; }

	/* WinZip AES: extra field 0x9901 carries strength + real method. */
	if( hdr->mhd==Aes && hdr->exl >= 4 )
	{
		DWORD off = 0;
		while( off + 4 <= (DWORD)hdr->exl )
		{
			WORD tag = (WORD)(buf[off] | (buf[off+1] << 8));
			WORD sz = (WORD)(buf[off+2] | (buf[off+3] << 8));
			if( tag==0x9901 && sz==7 && off+11 <= (DWORD)hdr->exl )
			{
				WORD aver = (WORD)(buf[off+4] | (buf[off+5] << 8));
				if( buf[off+6]=='A' && buf[off+7]=='E' &&
				    (aver==1 || aver==2) &&
				    buf[off+8]>=1 && buf[off+8]<=3 )
				{
					hdr->aesStrength = (WORD)(64 + 64*buf[off+8]);
					hdr->aesMethod = (WORD)(buf[off+9] | (buf[off+10] << 8));
				}
				break;
			}
			off += 4 + (DWORD)sz;
		}
		if( hdr->aesStrength==0 )
			{ if(del)delete [] buf; return false; }
	}

	if(del)
		delete [] buf;
	return true;
}

bool CZipTool::doHeader( ZipLocalHeader* hdr,unsigned char* buf )
{
	int c, stage=0;
	while( EOF != (c=fgetc(zip)) )
	{
			 if( c=='P'              ) stage++;
		else if( c=='K' &&  stage==1 ) stage++;
		else if( c==0x03 && stage==2 ) stage++;
		else if( c==0x04 && stage==3 )
		{
			stage++;
			int x=ftell(zip);
			if( read_header( hdr,buf ) )
				return true;
			fseek( zip,x,SEEK_SET );
		}
		else stage=0;
	}

	return false;
}

////////////////////////////////////////////////////////////////
// Store : 無圧縮格納
////////////////////////////////////////////////////////////////

void CZipTool::Unstore( DWORD usz, DWORD csz )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	int how_much;
	while( csz )
	{
		how_much = csz > XACR_BUFSIZE ? XACR_BUFSIZE : csz;
		if( 0>=(how_much=zipread( buf, how_much )) )
			break;
		zipwrite( buf, how_much );
		csz -= how_much;
	}
}

////////////////////////////////////////////////////////////////
// Deflate ： lzss+huffman。現在のpkzipのメソッド
////////////////////////////////////////////////////////////////

void CZipTool::Inflate( DWORD usz, DWORD csz, bool defl64 )
{
	unsigned char* outbuf = CTool::common_buf;
	unsigned char*  inbuf = CTool::common_buf + (XACR_BUFSIZE/2);
//--------------------------------------------------------------------//

	// zlib準備
	z_stream_s zs;
	zs.zalloc   = NULL;
	zs.zfree    = NULL;

	// 出力バッファ
	int outsiz = (XACR_BUFSIZE/2);
	zs.next_out = outbuf;
	zs.avail_out= outsiz;

	// 入力バッファ
	int insiz = zipread( inbuf,
		(XACR_BUFSIZE/2) > csz ? csz : (XACR_BUFSIZE/2) );
	if( insiz<=0 )
		return;
	csz        -= insiz;
	zs.next_in  = inbuf;
	zs.avail_in = insiz;

	// スタート
	if(defl64) zlibSwitchToDeflate64();
	inflateInit2( &zs, (defl64 ? -16 : -15) );

	// 書庫から入力し終わるまでループ
	int err = Z_OK;
	while( csz )
	{
		while( zs.avail_out > 0 )
		{
			err = inflate( &zs,Z_PARTIAL_FLUSH );
			if( err!=Z_STREAM_END && err!=Z_OK )
				csz=0;
			if( !csz )
				break;

			if( zs.avail_in<=0 )
			{
				int insiz = zipread( inbuf, (XACR_BUFSIZE/2) > csz ?
												csz : (XACR_BUFSIZE/2) );
				if( insiz<=0 )
				{
					err = Z_STREAM_END;
					csz = 0;
					break;
				}

				csz        -= insiz;
				zs.next_in  = inbuf;
				zs.avail_in = insiz;
			}
		}

		zipwrite( outbuf, outsiz-zs.avail_out );
		zs.next_out  = outbuf;
		zs.avail_out = outsiz;
	}

	// 出力残しを無くす。
	while( err!=Z_STREAM_END )
	{
		err = inflate(&zs,Z_PARTIAL_FLUSH);
		if( err!=Z_STREAM_END && err!=Z_OK )
		{
			zipwrite( outbuf, outsiz-zs.avail_out );
			break;
		}

		zipwrite( outbuf, outsiz-zs.avail_out );
		zs.next_out  = outbuf;
		zs.avail_out = outsiz;
	}

	// 終了
	inflateEnd(&zs);
	if(defl64) zlibSwitchToDeflate_old();
}

////////////////////////////////////////////////////////////////
// Shrink ： LZW with partial_clear PkZip 0.x-1.x
////////////////////////////////////////////////////////////////

void CZipTool::Unshrink( DWORD usz, DWORD csz )
{
	// 辞書サイズ = 8192Bytes = 13bits
	unsigned char* stack     = CTool::common_buf;
	unsigned char* suffix_of = CTool::common_buf + 8192 + 1;
	unsigned int*  prefix_of = (unsigned int*)CTool::common_buf + (8192 + 1) * 2;

	#define GetCode() getbits(codesize)
	int left=(signed)usz-1;
	//--------------------------------------------------------------------//


	int codesize, maxcode, free_ent, offset, sizex;
	int code, stackp, finchar, oldcode, incode;

	// 下準備…
	initbits();
	codesize = 9; // 初期bit数
	maxcode  = (1<<codesize) - 1;
	free_ent = 257; // 使われてない辞書領域の最初
	offset   = 0;
	sizex    = 0;

	// 辞書初期化
	for( code=(1<<13); code > 255; code-- )
		prefix_of[code] = -1;
	for( code=255; code >= 0; code-- )
	{
		prefix_of[code] = 0;
		suffix_of[code] = code;
	}

	// その他もろもろ
	oldcode = GetCode();
	if( bits_eof )
		return;
	finchar = oldcode;
	BYTE f=finchar;
	zipwrite( &f,1 );

	stackp = 8192;

	while( left>0 && !bits_eof )
	{
		// code を一個取り出す
		code = GetCode();
		if( bits_eof )
			break;

		// clear!
		while( code == 256 )
		{
			switch( GetCode() )
			{
			case 1:
				codesize++;
				if( codesize == 13 )
					maxcode = (1 << codesize);
				else
					maxcode = (1 << codesize) - 1;
				break;

			case 2: // partial_clear !!
				{
					int pr,cd;

					for( cd=257; cd<free_ent; cd++ )
						prefix_of[cd] |= 0x8000;

					for( cd=257; cd<free_ent; cd++)
					{
						pr = prefix_of[cd] & 0x7fff;
						if( pr >= 257 )
							prefix_of[pr] &= 0x7fff;
					}

					for( cd=257; cd<free_ent; cd++ )
						if( (prefix_of[cd] & 0x8000) != 0 )
							prefix_of[cd] = -1;

					cd = 257;
					while( (cd < (1<<13)) && (prefix_of[cd] != -1) )
						cd++;
					free_ent = cd;
				}
				break;
			}

			code = GetCode();
			if( bits_eof )
				break;
		}
		if( bits_eof )
			break;

		// 特殊な場合( KwKwK )
		incode = code;
		if( prefix_of[code] == -1 )
		{
            stack[--stackp] = finchar;
			code = oldcode;
		}

		// 辞書からスタックへ
		while( code >= 257 )
		{
			stack[--stackp] = suffix_of[code];
			code = prefix_of[code];
		}
		finchar = suffix_of[code];
		stack[--stackp] = finchar;
		// スタックから出力
		left -= (8192-stackp);
		zipwrite( stack+stackp, (8192-stackp) );
		stackp = 8192;

		// 新しいエントリを追加
		code = free_ent;
		if( code < (1<<13) )
		{
			prefix_of[code] = oldcode;
			suffix_of[code] = finchar;

			do
				code++;
			while( (code < (1<<13)) && (prefix_of[code] != -1) );

			free_ent = code;
		}

		// 前のを覚えてとく
		oldcode = incode;
	}

#undef GetCode
}

////////////////////////////////////////////////////////////////
// Reduce ： lz77 + 変な符号化(^^; PkZip 0.x
////////////////////////////////////////////////////////////////

void CZipTool::LoadFollowers( BYTE* Slen, BYTE followers[][64] )
{
	for( int x=255; x>=0; x-- )
	{
		Slen[x] = getbits(6);
		for( int i=0; i<Slen[x]; i++ )
			followers[x][i] = getbits(8);
	}
}

void CZipTool::Unreduce( DWORD usz, DWORD csz, int factor )
{
	unsigned char* outbuf = CTool::common_buf;
	unsigned char* outpos=outbuf;
	memset( outbuf,0,0x4000 );
	int left = (signed)usz;
#define RED_FLUSH() zipwrite(outbuf,outpos-outbuf), outpos=outbuf;
#define RED_OUTC(c) {(*outpos++)=c; left--; if(outpos-outbuf==0x4000) RED_FLUSH();}
//--------------------------------------------------------------------//

	static const int Length_table[] = {0, 0x7f, 0x3f, 0x1f, 0x0f};
	static const int D_shift[] = {0, 0x07, 0x06, 0x05, 0x04};
	static const int D_mask[]  = {0, 0x01, 0x03, 0x07, 0x0f};
	static const int B_table[] = {
		8, 1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8};

	initbits();

	BYTE followers[256][64];
	BYTE Slen[256];
	LoadFollowers( Slen, followers );

	int l_stage = 0;
	int l_char  = 0;
	int ch, V, Len;

	while( !bits_eof && left>0 )
	{
		if( Slen[l_char] == 0 )
			ch = getbits(8);
		else
		{
			if( getbits(1) )
				ch = getbits(8);
			else
			{
				int bitsneeded = B_table[ Slen[l_char] ];
				ch = followers[ l_char ][ getbits(bitsneeded) ];
			}
		}

		// Repeater Decode
		switch( l_stage )
		{
		case 0:
			if( ch == 0x90 )// hitを示すフラグなら、次のステージへ
				l_stage++;
			else			// そのまま出力
				RED_OUTC( ch )
			break;

		case 1:
			if( ch == 0x00 )// hitフラグをそのまま出力
			{
				RED_OUTC(0x90)
				l_stage = 0;
			}
			else			// 長さを取得( メソッド依存 )
			{
				V   = ch;
				Len = V & Length_table[factor];
				if( Len == Length_table[factor] )
					l_stage = 2;
				else
					l_stage = 3;
			}
			break;

		case 2:				// 長さを追加取得
			Len += ch;
			l_stage++;
			break;

		case 3:				// マッチした部分を出力( メソッド依存 )
			{
				int i = Len+3;
				int offset = (((V>>D_shift[factor]) & D_mask[factor]) << 8) + ch + 1;
				int op = (((outpos-outbuf)-offset) & 0x3fff);

				while( i-- )
				{
					RED_OUTC( outbuf[op++] );
					op &= 0x3fff;
				}

				l_stage = 0;
			}
			break;
		}
		l_char = ch;
	}

	RED_FLUSH();

#undef RED_FLUSH
#undef RED_OUTC
}

////////////////////////////////////////////////////////////////
// Implode ： lz77 + shanon-fano ? PkZip 1.x
////////////////////////////////////////////////////////////////

void CZipTool::Explode( DWORD usz, DWORD csz, bool _8k, bool littree )
{
	unsigned char* outbuf = CTool::common_buf;
	unsigned char* outpos=outbuf;
	memset( outbuf,0,0x4000 );
	int left = (signed)usz;
#define EXP_FLUSH() zipwrite(outbuf,outpos-outbuf), outpos=outbuf;
#define EXP_OUTC(c) {(*outpos++)=c; left--; if(outpos-outbuf==0x4000) EXP_FLUSH();}
//--------------------------------------------------------------------//

	BYTE ch;
	initbits();

	int dict_bits     = ( _8k ? 7 : 6);
	int min_match_len = (littree ? 3 : 2);
	sf_tree lit_tree; 
	sf_tree length_tree; 
	sf_tree distance_tree; 

	// 木をロード
	if( littree ) 
		LoadTree( &lit_tree, 256 );
	LoadTree( &length_tree, 64 ); 
	LoadTree( &distance_tree, 64 ); 

	// データ処理
	while( !bits_eof && left>0 )
	{
		if( getbits(1) ) // 普通の文字データ
		{
			if( littree )
				ch = ReadTree( &lit_tree );
			else
				ch = getbits(8);

			EXP_OUTC( ch );
		}
		else // スライド辞書にマッチしてるデータ
		{
			// 距離
			int Distance = getbits(dict_bits);
			Distance |= ( ReadTree(&distance_tree) << dict_bits );

			// 長さ
			int Length = ReadTree( &length_tree );

			if( Length == 63 )
				Length += getbits(8);
			Length += min_match_len;

			// 出力
			int op = (((outpos-outbuf)-(Distance+1)) & 0x3fff);
			while( Length-- )
			{
				EXP_OUTC( outbuf[op++] );
				op &= 0x3fff;
			}
		}
	}

	EXP_FLUSH();

#undef EXP_OUTC
#undef EXP_FLUSH
}

static void SortLengths( sf_tree* tree )
{ 
	int gap,a,b;
	sf_entry t; 
	bool noswaps;

	gap = tree->entries >> 1; 

	do
	{
		do
		{
			noswaps = true;
			for( int x=0; x<=(tree->entries - 1)-gap; x++ )
			{
				a = tree->entry[x].BitLength;
				b = tree->entry[x + gap].BitLength;
				if( (a>b) || ((a==b) && (tree->entry[x].Value > tree->entry[x + gap].Value)) )
				{
					t = tree->entry[x];
					tree->entry[x] = tree->entry[x + gap];
					tree->entry[x + gap] = t;
					noswaps = false;
				}
			}
		} while (!noswaps);
		
		gap >>= 1;
	} while( gap > 0 );
}

void CZipTool::ReadLengths( sf_tree* tree )
{ 
	int treeBytes,i,num,len;

	treeBytes = getbits(8) + 1;
	i = 0; 

	tree->MaxLength = 0;

	while( treeBytes-- )
	{
		len = getbits(4)+1;
		num = getbits(4)+1;

		while( num-- )
		{
			if( len > tree->MaxLength )
				tree->MaxLength = len;
			tree->entry[i].BitLength = len;
			tree->entry[i].Value = i;
			i++;
		}
	} 
} 

static void GenerateTrees( sf_tree* tree )
{ 
	WORD Code = 0;
	int CodeIncrement = 0, LastBitLength = 0;

	int i = tree->entries - 1;   // either 255 or 63
	while( i >= 0 )
	{
		Code += CodeIncrement;
		if( tree->entry[i].BitLength != LastBitLength )
		{
			LastBitLength = tree->entry[i].BitLength;
			CodeIncrement = 1 << (16 - LastBitLength);
		}

		tree->entry[i].Code = Code;
		i--;
	}
}

static void ReverseBits( sf_tree* tree )
{
	for( int i=0; i<=tree->entries-1; i++ )
	{
		WORD o = tree->entry[i].Code,
			 v = 0,
		  mask = 0x0001,
		  revb = 0x8000;

		for( int b=0; b!=16; b++ )
		{
			if( (o&mask) != 0 )
				v = v | revb;

			revb = (revb >> 1);
			mask = (mask << 1);
		}

		tree->entry[i].Code = v;
	}
}

void CZipTool::LoadTree( sf_tree* tree, int treesize )
{ 
	tree->entries = treesize; 
	ReadLengths( tree ); 
	SortLengths( tree ); 
	GenerateTrees( tree ); 
	ReverseBits( tree ); 
} 

int CZipTool::ReadTree( sf_tree* tree )
{ 
	int bits=0, cur=0, b;
	WORD cv=0;

	while( true )
	{
		b = getbits(1);
		cv = cv | (b << bits);
		bits++;

		while( tree->entry[cur].BitLength < bits )
		{
			cur++;
			if( cur >= tree->entries )
				return -1;
		}

		while( tree->entry[cur].BitLength == bits )
		{
			if( tree->entry[cur].Code == cv )
				return tree->entry[cur].Value;
			cur++;
			if( cur >= tree->entries )
				return -1;
		}
	}
}

void CZipTool::UnBZip2( DWORD usz, DWORD csz )
{
	unsigned char* outbuf = CTool::common_buf;
	unsigned char*  inbuf = CTool::common_buf + (XACR_BUFSIZE/2);
//--------------------------------------------------------------------//

	// libbz2準備
	bz_stream bs = {0};

	// 出力バッファ
	int outsiz   = (XACR_BUFSIZE/2);
	bs.next_out  = (char*)outbuf;
	bs.avail_out = outsiz;

	// 入力バッファ
	int insiz = zipread( inbuf, (XACR_BUFSIZE/2) > csz ? csz : (XACR_BUFSIZE/2) );
	if( insiz<=0 )
		return;
	csz        -= insiz;
	bs.next_in  = (char*)inbuf;
	bs.avail_in = insiz;

	// スタート
	BZ2_bzDecompressInit( &bs, 0, 0 );

	// 書庫から入力し終わるまでループ
	int err = BZ_OK;
	while( csz )
	{
		while( bs.avail_out > 0 )
		{
			err = BZ2_bzDecompress( &bs );
			if( err!=BZ_STREAM_END && err!=BZ_OK )
				csz=0;
			if( !csz )
				break;

			if( bs.avail_in<=0 )
			{
				int insiz = zipread( inbuf, (XACR_BUFSIZE/2) > csz ?
												csz : (XACR_BUFSIZE/2) );
				if( insiz<=0 )
				{
					err = BZ_STREAM_END;
					csz = 0;
					break;
				}

				csz        -= insiz;
				bs.next_in  = (char*)inbuf;
				bs.avail_in = insiz;
			}
		}

		zipwrite( outbuf, outsiz-bs.avail_out );
		bs.next_out  = (char*)outbuf;
		bs.avail_out = outsiz;
	}

	// 出力残しを無くす。
	while( err!=BZ_STREAM_END )
	{
		err = BZ2_bzDecompress( &bs );
		if( err!=BZ_STREAM_END && err!=BZ_OK )
			break;

		zipwrite( outbuf, outsiz-bs.avail_out );
		bs.next_out  = (char*)outbuf;
		bs.avail_out = outsiz;
	}

	// 終了
	BZ2_bzDecompressEnd( &bs );
}

int CZipTool::lzmaZipFill( void *ctx, BYTE *buf, int want )
{
	LzmaZipIO *io = (LzmaZipIO*)ctx;
	DWORD w = (DWORD)want;
	int got;
	if( io->left==0 || w==0 )
		return 0;
	if( w > io->left )
		w = io->left;
	got = io->tool->zipread( buf, (int)w );
	if( got <= 0 )
		return 0;
	io->left -= (DWORD)got;
	return got;
}

void CZipTool::lzmaZipWrite( void *ctx, const BYTE *buf, int len )
{
	LzmaZipIO *io = (LzmaZipIO*)ctx;
	io->tool->zipwrite( (BYTE*)buf, len );
}

////////////////////////////////////////////////////////////////
// DCL-implode (pkdcl.c, PKWARE Data Compression Library, ZIP method 10)
////////////////////////////////////////////////////////////////

unsigned short CZipTool::dclRead( unsigned char *buf, unsigned short *size, void *how )
{
	DclIO *io = (DclIO*)how;
	DWORD want = (DWORD)*size;
	if( want > io->left )
		want = io->left;
	if( want == 0 )
		{ *size = 0; return 0; }
	int got = io->tool->zipread( buf, (int)want );
	if( got <= 0 )
		{ *size = 0; return 0; }
	io->left -= (DWORD)got;
	*size = (unsigned short)got;
	return *size;
}

void CZipTool::dclWrite( unsigned char *buf, unsigned short *size, void *how )
{
	DclIO *io = (DclIO*)how;
	io->tool->zipwrite( buf, (int)*size );
}

void CZipTool::UnDcl( DWORD usz, DWORD csz )
{
	DclIO io;
	io.tool = this;
	io.left = csz;
	if( usz == 0 )
		{
		// an empty DCL member is exactly 4 bytes; the legacy entry point
		// rejects a first fill of 4 bytes, so use the extended one.
		pkdcl_explode_ex( dclRead, dclWrite, &io );
		}
	else
		{
		unsigned char *work = new unsigned char[PKDCL_EXPLODE_WORK_SIZE];
		pkdcl_explode( dclRead, dclWrite, work, &io );
		delete [] work;
		}
}
////////////////////////////////////////////////////////////////
// LZMA (LzmaDec.c, ZIP method 14)
////////////////////////////////////////////////////////////////

void CZipTool::UnLzma( DWORD usz, DWORD csz )
{
	BYTE props[LZMA_PROPS_SIZE];
	BYTE hdr[4];
	DWORD left = csz;
	DWORD psize;
	LzmaZipIO io;

	if( usz == 0 )
		return;
	if( csz < 5 )
		return;

	// framing: [v0 v1 props_size(LE16) props][stream]
	// some writers omit the 4 byte prefix, so fall back to a bare
	// 5 byte property blob when the size field does not read as 5.
	if( zipread( hdr, 4 ) <= 0 )
		return;
	left -= 4;
	psize = (DWORD)hdr[2] | ((DWORD)hdr[3]<<8);
	if( psize == LZMA_PROPS_SIZE )
		{
		if( zipread( props, LZMA_PROPS_SIZE ) <= 0 )
			return;
		left -= LZMA_PROPS_SIZE;
		}
	else
		{
		memcpy( props, hdr, 4 );
		if( zipread( props+4, 1 ) <= 0 )
			return;
		left -= 1;
		}

	io.tool = this;
	io.left = left;
	LzmaDecodeStream( props, lzmaZipFill, &io, lzmaZipWrite, &io, usz, true );
}

////////////////////////////////////////////////////////////////
// WinZip AES (wzaes.c, ZIP method 99)
////////////////////////////////////////////////////////////////

bool CZipTool::aes_init( const char* pwd, ZipLocalHeader* hdr, DWORD* innerCsz )
{
	BYTE salt[16];
	BYTE verify[2], expect[2];
	DWORD saltLen;
	long x = ftell( zip );
	bool ok = false;
	needaes = false;
	needdec = false;
	if( pwd!=NULL && *pwd!='\0' && hdr->aesStrength!=0 )
		{
		if( hdr->aesStrength==128 ) saltLen = 8;
		else if( hdr->aesStrength==192 ) saltLen = 12;
		else if( hdr->aesStrength==256 ) saltLen = 16;
		else saltLen = 0;
		if( saltLen!=0 && hdr->csz >= saltLen+2+10 &&
		    saltLen==fread(salt,1,saltLen,zip) &&
		    2==fread(verify,1,2,zip) &&
		    iz_aes_init(&aesctx,pwd,salt,hdr->aesStrength,expect) &&
		    verify[0]==expect[0] && verify[1]==expect[1] )
			{ needaes = true; *innerCsz = hdr->csz - saltLen - 2 - 10; ok = true; }
		}
	if( !ok )
		fseek( zip, x, SEEK_SET );
	return ok;
}

////////////////////////////////////////////////////////////////
// Xceed BWT (xceedbwt.c, ZIP method 18)
////////////////////////////////////////////////////////////////

int CZipTool::bwtRead( void *op )
{
	BwtIn *io = (BwtIn*)op;
	BYTE b;
	if( io->left==0 )
		return -1;
	if( 1 != io->tool->zipread( &b, 1 ) )
		return -1;
	io->left--;
	return b;
}

int CZipTool::bwtWrite( void *op, const unsigned char *data, size_t n )
{
	BwtIn *io = (BwtIn*)op;
	io->tool->zipwrite( (BYTE*)data, (int)n );
	return 0;
}

void CZipTool::UnBwt( DWORD usz, DWORD csz, DWORD crc )
{
	BwtIn io;
	io.tool = this;
	io.left = csz;
	iz_xceedbwt_decode( bwtRead, bwtWrite, &io, 0UL, (unsigned long)usz, (unsigned long)crc );
}

////////////////////////////////////////////////////////////////
// XZ (method 95): one complete XZ stream per member, LZMA2 blocks
////////////////////////////////////////////////////////////////

int CZipTool::xzZipFill( void *ctx, BYTE *buf, int want )
{
	XzZipIO *io = (XzZipIO*)ctx;
	DWORD w = (DWORD)want;
	int got;
	if( io->left==0 || w==0 )
		return 0;
	if( w > io->left )
		w = io->left;
	got = io->tool->zipread( buf, (int)w );
	if( got <= 0 )
		return 0;
	io->left -= (DWORD)got;
	return got;
}

void CZipTool::xzZipWrite( void *ctx, const BYTE *buf, int len )
{
	XzZipIO *io = (XzZipIO*)ctx;
	io->tool->zipwrite( (BYTE*)buf, len );
}

void CZipTool::UnXz( DWORD usz, DWORD csz )
{
	XzZipIO io;
	io.tool = this;
	io.left = csz;
	XzDecodeStream( xzZipFill, &io, xzZipWrite, &io, usz, true );
}
////////////////////////////////////////////////////////////////
// Zstd (zstd 1.5.6, ZIP method 93: one zstd frame per member)
////////////////////////////////////////////////////////////////

void CZipTool::UnZstd( DWORD usz, DWORD csz )
{
	BYTE *in, *out;
	DWORD off = 0;
	int r;
	size_t got;

	if( csz==0 )
		return; // empty member: 0-byte output, Extract size check decides
	in = new BYTE[csz];
	while( off < csz )
		{
		r = zipread( in+off, (int)(csz-off) );
		if( r <= 0 )
			{ delete [] in; return; }
		off += (DWORD)r;
		}
	out = new BYTE[usz ? usz : 1];
	got = ZSTD_decompress( out, usz, in, csz );
	delete [] in;
	// short or mismatched output fails loudly via the Extract size check
	if( ZSTD_isError(got) || got != (size_t)usz )
		{ delete [] out; return; }
	if( got != 0 )
		zipwrite( out, (int)got );
	delete [] out;
}

