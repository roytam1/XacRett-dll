
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
	bool failed = false;
	char password[128] = {'\0'};
	ZipLocalHeader hdr;

	if( !chain.Open( fname ) )
		return false;

	while( true )
	{
		AlzMember m;
		char name[ALZ_NAME_MAX];

		int r = AlzNextMember( &chain, &m, name );
		if( r < 0 )            // structural error
			{failed = true; break;}
		if( r == 0 )           // END Record
			break;
		if( m_Endflg )
			break;

		if( m.name_len < ALZ_NAME_MAX && CheckStr( name )
			&& ( !(m.fd & ALZ_FD_NB)
				|| m.method == ALZ_M_STORE
				|| m.method == ALZ_M_DEFLATE ) )
		{
			if( !(m.fd & ALZ_FD_NB) && (m.attr & 0x10) )
			{
				// directory
				char* name2 = kiutil::pathMake( name );
				::CreateDirectory( name2, NULL );
				continue;
			}

			char* name2 = kiutil::pathMake( name );
			if( out=fopen(name2,"wb") )
			{
				bool bfail = false;
				write_init();   // output CRC reset

				hdr.ver = 0;
				hdr.flg = (m.fd & ALZ_FD_ENC) ? 1 : 0;
				hdr.mhd = m.method;
				hdr.tim = m.time;
				hdr.dat = m.date;
				hdr.crc = m.crc;
				hdr.csz = m.csize;
				hdr.usz = m.usize;

				if( hdr.flg )
					hdr.csz += 12;    // the password check comes first

				if( hdr.flg&1 ) // set the password
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
						if( i==3 ) // password mismatch
						{
							bfail = true;
							failed = true;
							fclose( out );
							DeleteFile( name2 );
							break;
						}
					}
				}
				else // no password
					read_init();

				if( !bfail )
				{
					// decode
					if( hdr.mhd == ALZ_M_DEFLATE )
						Inflate( hdr.usz, hdr.csz );
					else
						Unstore( hdr.usz, hdr.csz );

					fclose( out );

					// the member CRC is over the (uncompressed) output
					if( (m.fd & ALZ_FD_NB) && wrtcrc != m.crc )
					{
						DeleteFile( name2 );
						failed = true;
					}
					else
						kiutil::timeSet( name2,(WORD)hdr.dat,(WORD)hdr.tim );
				}
			}
			else
			{
				// could not even create the file
				failed = true;
				AlzSkipMember( &chain, &m );
			}
		}
		else
			AlzSkipMember( &chain, &m ); // skip unsupported or unsafe members
	}

	chain.Close();
	return !failed;
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

DWORD CAlzTool::crc32( DWORD crc, const BYTE* dat, int len )
{
	return ZipCrypto::Crc( crc, dat, len );
}

//-- パスワード解除関係 ------------------------

#define RAND_HEAD_LEN (12)
bool CAlzTool::read_init( const char* pwd, ZipLocalHeader* hdr )
{
	if( pwd == NULL )
	{
		needdec = false;
		return true;
	}

	needdec = true;

	// init with this password
	zc.Init( pwd );

	// remember the position so a failed check can rewind
	int ov; long op;
	chain.getpos( &ov, &op );

	// read the password check bytes
	BYTE eh[RAND_HEAD_LEN];
	bool ans = false;
	if( RAND_HEAD_LEN == chain.read( eh, RAND_HEAD_LEN ) )
	{
		zc.DecodeBuffer( eh, RAND_HEAD_LEN );

		// the check value depends on the flags; ALZ only ever sets
		// bit 0, so this is the high 16 bits of the CRC
		WORD checkerval = ((hdr->flg&8)==8 ? hdr->tim : (WORD)(hdr->crc>>16));

		// verify
		if( (eh[RAND_HEAD_LEN-2] | (eh[RAND_HEAD_LEN-1]<<8)) == checkerval )
			ans = true;
	}

	if( ans ) // checked: shrink the size by the verifier
		hdr->csz = hdr->csz>RAND_HEAD_LEN ? hdr->csz-RAND_HEAD_LEN : 0;
	else      // failed the check: rewind
		chain.setpos( ov, op );

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
