
#include "stdafx.h"
#include "AlzTool.h"
#include "zlib/zlib.h"

//-- CAlzTool 部分 -----------------------

bool CAlzTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
	//--------------------------------------------------------------------//

	int i;
	for( i=0; i<(signed)siz-30; i++ )
	{
		if( hdr[i+0] != 'A'  )continue;
		if( hdr[i+1] != 'L'  )continue;
		if( hdr[i+2] != 'Z' )continue;
		if( hdr[i+3] != 0x01 )continue;
		break;
	}
	if( (unsigned)i+30>=siz )
		return false;

	return true;
}

bool CAlzTool::Extract( const char* fname,const char* ddir )
{
	char password[128] = {'\0'};
	ZipLocalHeader hdr;

	if( !(zip=fopen(fname,"rb")) )
		return false;

	while( doHeader( &hdr,CTool::common_buf ) )
	{
		unsigned long posnext = ftell( zip ) + hdr.csz;

		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(hdr.fnm))//追加 By Bonty
		{
			fseek( zip, posnext, SEEK_SET );
			continue;
		}

		char *name = kiutil::pathMake( hdr.fnm );
		if( out=fopen(name,"wb") )
		{
			write_init();   // 出力CRCをクリア
			if( hdr.flg&1 ) // 暗号化されてるのでパスワードをセット
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
			switch( hdr.mhd )
			{
			case Stored:	 Unstore( hdr.usz, hdr.csz );	break;
			case Deflated:	 Inflate( hdr.usz, hdr.csz );	break;
			case Shrunk:	Unshrink( hdr.usz, hdr.csz );	break;
			case Reduced1:	Unreduce( hdr.usz, hdr.csz, 1 );	break;
			case Reduced2:	Unreduce( hdr.usz, hdr.csz, 2 );	break;
			case Reduced3:	Unreduce( hdr.usz, hdr.csz, 3 );	break;
			case Reduced4:	Unreduce( hdr.usz, hdr.csz, 4 );	break;
			case Imploded:
				Explode( hdr.usz, hdr.csz,
							 0!=(hdr.flg&0x02), 0!=((hdr.flg)&0x04) );
				break;
			case EnhDeflated:
				EnableDFL64(1);
				Inflate( hdr.usz, hdr.csz );
				EnableDFL64(0);
				break;
			}

			// 事後処理
			fclose( out );

			kiutil::timeSet( name, (WORD)hdr.dat,(WORD)hdr.tim );
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
static DWORD crc_table[256] = {
0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

// update crc
#define calc_crc(c,b) (crc_table[(c&0xff)^(b&0xff)]^((c)>>8))

DWORD CAlzTool::crc32( DWORD crc, const BYTE* dat, int len )
{
	crc ^= 0xffffffffL;
	while( len-- )
		crc = calc_crc( crc,*dat++ );
	return (crc^0xffffffffL);
}

//-- パスワード解除関係 ------------------------

int CAlzTool::decrypt_byte()
{
	unsigned temp = ((unsigned)(keys[2]&0xffff))|2;
	return (int)(((temp*(temp^1))>>8)&0xff);
}

int CAlzTool::update_keys( int c )
{
	keys[0] = calc_crc( keys[0],c );
	keys[1]+= (keys[0]&0xff);
	keys[1] = keys[1]*134775813L + 1;
	keys[2] = calc_crc( keys[2], (keys[1]>>24) );
	return c;
}

#define RAND_HEAD_LEN (12)
bool CAlzTool::read_init( const char* pwd, ZipLocalHeader* hdr )
{
	if( pwd == NULL )
	{
		needdec = false;
		return true;
	}

	needdec = true;

	// このパスワードで初期化
	keys[0] = 305419896L;
	keys[1] = 591751049L;
	keys[2] = 878082192L;
	while( *pwd )
		update_keys( *pwd++ );

	// 今の位置を記憶
	int x = ftell( zip );

	// passwordチェック部読み込み
	BYTE eh[RAND_HEAD_LEN];
	bool ans = false;
	if( RAND_HEAD_LEN == fread( eh, 1, RAND_HEAD_LEN, zip ) )
	{
		for( int i=0; i!=RAND_HEAD_LEN; i++ )
			zdecode( eh[i] );

		// フラグによって、チェックに用いる値は違う
		WORD checkerval = ((hdr->flg&8)==8 ? hdr->tim : (WORD)(hdr->crc>>16));

		// 検査
		if( (eh[RAND_HEAD_LEN-2] | (eh[RAND_HEAD_LEN-1]<<8)) == checkerval )
			ans = true;
	}

	if( ans ) // チェック部を読んだ分、圧縮サイズを変更
		hdr->csz = hdr->csz>RAND_HEAD_LEN ? hdr->csz-RAND_HEAD_LEN : 0;
	else	  // チェック部の分、戻る
		fseek( zip, x, SEEK_SET );
	return ans;
}

//-- ヘッダ処理 -----------------------------------

bool CAlzTool::read_header( ZipLocalHeader* hdr,unsigned char* buf )
{
	bool del = (buf==NULL);
	if( buf==NULL ) buf = new unsigned char[65536];

	
	int x=ftell(zip);
	if( sizeof(BLZ)+1 != fread(buf,1,sizeof(BLZ)+1,zip) )
		{ if(del)delete [] buf; return false; }

	fseek( zip,x+9,SEEK_SET );

	BLZ_8 al_hdr;

	al_hdr.blz.fn_len=buf[0];
	al_hdr.blz.atter=((buf[1])|(buf[2]<<8));
	al_hdr.blz.time=((buf[3])|(buf[4]<<8));
	al_hdr.blz.date=((buf[5])|(buf[6]<<8));

	

	switch ((buf[7]&0x01)?(buf[7]^0x01):buf[7])
	{
	case 0x00:
		al_hdr.blz.method=0;
		al_hdr.blz.crc=0;
		al_hdr.ext_size=0;
		al_hdr.cmp_size=0;
		fread(&buf[9],1,al_hdr.blz.fn_len+1,zip);
		al_hdr.fname=&buf[9];
		break;
	case 0x10:
		fread(&buf[9],1,8,zip);
		al_hdr.cmp_size=buf[15];
		al_hdr.ext_size=buf[16];
		fread(&buf[17],1,al_hdr.blz.fn_len+1,zip);
		al_hdr.fname=&buf[17];
		break;
	case 0x20:
		fread(&buf[9],1,10,zip);
		al_hdr.cmp_size=((buf[15])|(buf[16]<<8));
		al_hdr.ext_size=((buf[17])|(buf[18]<<8));
		fread(&buf[19],1,al_hdr.blz.fn_len+1,zip);
		al_hdr.fname=&buf[19];
		break;
	case 0x40:
		fread(&buf[9],1,14,zip);
		al_hdr.cmp_size=((buf[15])|(buf[16]<<8)|(buf[17]<<16)|(buf[18]<<24));
		al_hdr.ext_size=((buf[19])|(buf[20]<<8)|(buf[21]<<16)|(buf[22]<<24));
		fread(&buf[23],1,al_hdr.blz.fn_len+1,zip);
		al_hdr.fname=&buf[23];
		break;
	case 0x80:
		fread(&buf[9],1,22,zip);
		al_hdr.cmp_size=((buf[15])|(buf[16]<<8)|(buf[17]<<16)|(buf[18]<<24)|(buf[19]<<32)|(buf[20]<<40)|(buf[21]<<48)|(buf[22]<<56));
		al_hdr.ext_size=((buf[23])|(buf[24]<<8)|(buf[25]<<16)|(buf[26]<<24)|(buf[27]<<32)|(buf[28]<<40)|(buf[29]<<48)|(buf[30]<<56));
		fread(&buf[31],1,al_hdr.blz.fn_len+1,zip);
		al_hdr.fname=&buf[31];
		break;
	};
	if((buf[7]&0x01)?(buf[7]^0x01):buf[7]!=0)
	{
		al_hdr.blz.method=((buf[9])|(buf[10]<<8));
		al_hdr.blz.crc=((buf[11])|(buf[12]<<8)|(buf[13]<<16)|(buf[14]<<24));
	}
	fseek( zip,-1,SEEK_CUR );



	hdr->ver = 0;

	hdr->flg = buf[7]&0x01?1:0;
	hdr->mhd = al_hdr.blz.method&0x02?Deflated:0;
	hdr->tim = al_hdr.blz.time;
	hdr->dat = al_hdr.blz.date;
	hdr->crc = al_hdr.blz.crc;
	hdr->csz = al_hdr.cmp_size;
	hdr->usz = al_hdr.ext_size;
	hdr->fnl = al_hdr.blz.fn_len;
	hdr->exl = 0;
	lstrcpyn(hdr->fnm,(LPCSTR)al_hdr.fname,al_hdr.blz.fn_len+1);

	if(hdr->flg)
		hdr->csz+=12;
	if(del)
		delete [] buf;
	return true;
}

bool CAlzTool::doHeader( ZipLocalHeader* hdr,unsigned char* buf )
{
	int c, stage=0;
	while( EOF != (c=fgetc(zip)) )
	{
			 if( c=='B'              ) stage++;
		else if( c=='L' &&  stage==1 ) stage++;
		else if( c=='Z' && stage==2 ) stage++;
		else if( c==0x01 && stage==3 )
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

void CAlzTool::Unstore( LONGLONG usz, LONGLONG csz )
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

void CAlzTool::Inflate( LONGLONG usz, LONGLONG csz )
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
	inflateInit2( &zs, -15 );

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
			break;

		zipwrite( outbuf, outsiz-zs.avail_out );
		zs.next_out  = outbuf;
		zs.avail_out = outsiz;
	}

	// 終了
	inflateEnd(&zs);
}

////////////////////////////////////////////////////////////////
// Shrink ： LZW with partial_clear PkZip 0.x-1.x
////////////////////////////////////////////////////////////////

void CAlzTool::Unshrink( LONGLONG usz, LONGLONG csz )
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

void CAlzTool::LoadFollowers( BYTE* Slen, BYTE followers[][64] )
{
	for( int x=255; x>=0; x-- )
	{
		Slen[x] = getbits(6);
		for( int i=0; i<Slen[x]; i++ )
			followers[x][i] = getbits(8);
	}
}

void CAlzTool::Unreduce( LONGLONG usz,LONGLONG csz, int factor )
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

void CAlzTool::Explode( LONGLONG usz, LONGLONG csz, bool _8k, bool littree )
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

static void SortLengths( CAlzTool::sf_tree* tree )
{ 
	int gap,a,b;
	CAlzTool::sf_entry t; 
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

void CAlzTool::ReadLengths( sf_tree* tree )
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

static void GenerateTrees( CAlzTool::sf_tree* tree )
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

static void ReverseBits( CAlzTool::sf_tree* tree )
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

void CAlzTool::LoadTree( sf_tree* tree, int treesize )
{ 
	tree->entries = treesize; 
	ReadLengths( tree ); 
	SortLengths( tree ); 
	GenerateTrees( tree ); 
	ReverseBits( tree ); 
} 

int CAlzTool::ReadTree( sf_tree* tree )
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
