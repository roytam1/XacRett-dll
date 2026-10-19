
#include "stdafx.h"
#include "AsdTool.h"
#include "zlib/zlib.h"

#define incr(x) (x=(x+1)&0xfff)

bool CAsdTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	// 魔法の数字は A S D 0 1 [1a] !
	// ASD 0.1.x ("01") and ASD 0.2.x ("02") are both accepted
	return ( siz>6 && hdr[0]=='A' && hdr[1]=='S' && hdr[2]=='D'
		&& hdr[3]=='0' && (hdr[4]=='1' || hdr[4]=='2') && hdr[5]==0x1A );
}

bool CAsdTool::Extract( const char* fname,const char* ddir )
{
	unsigned char buf[300];
//--------------------------------------------------------------------//

	// 開く
	FILE* asd = fopen( fname, "rb" );
	if( !asd ) return false;

	// the version byte follows the "ASD" magic: '1' = 0.1.x (LZSS),
	// '2' = 0.2.x (LZ77 + dual adaptive Huffman)
	{
		unsigned char hd[6];
		if( 6!=fread( hd, 1, 6, asd ) )
		{
			fclose( asd );
			return false;
		}
		if( '2'==hd[4] )
		{
			bool r = Extract20( asd );
			fclose( asd );
			return r;
		}
	}

	// マジックナンバをとばす
	fseek( asd, 6, SEEK_SET );

	// 格納されているファイル数
	if( 2!=fread( buf, 1, 2, asd ) )
	{
		fclose( asd );
		return false;
	}
	int num_of_files = (buf[0]) + (buf[1]<<8);

	// ヘッダの開始位置
	// （ヘッダをいったん別のメモリに格納した方が速いのは明らかだが、）
	// （面倒なので(^^;、SEEKしまくりで解決する。）
	long header_pos = 8;

	// 圧縮されたデータの開始位置
	for( int t,d=0; d!=num_of_files; d++ )
		if( EOF == (t=fgetc(asd))
		 || (t+14) != (signed)fread( buf, 1, (t+14), asd ) )
		{
			fclose( asd );
			return false;
		}
	init_expander( fgetc( asd ) );
	long data_pos = ftell( asd );

	// 展開開始
	for( int c,i=0; i!=num_of_files; i++ )
	{
		// 次のファイルの情報格納位置へ
		fseek( asd, header_pos, SEEK_SET );

		// ヘッダ解析
		c = fgetc(asd);
		fread( buf, 1, c+14, asd );

		char outname[256];		// ファイル名
		memcpy( outname, buf, c ), outname[c] = '\0';
		unsigned long orisize =	// 元サイズ
			(buf[c+ 0])+(buf[c+ 1]<<8)+(buf[c+ 2]<<16)+(buf[c+ 3]<<24);
		unsigned long cr =		// CRC
			(buf[c+ 4])+(buf[c+ 5]<<8)+(buf[c+ 6]<<16)+(buf[c+ 7]<<24);
		unsigned long uptime =	// 更新日時
			(buf[c+ 8])+(buf[c+ 9]<<8)+(buf[c+10]<<16)+(buf[c+11]<<24);
		unsigned short attr =	// 属性
			(buf[c+12])+(buf[c+13]<<8);

		// ヘッダ位置ポインタを進めておく
		header_pos += (1+c+14);
		
		if (m_Endflg) return true;//追加 By Bonty
		if (!CheckStr(outname))//追加 By Bonty
		{
			unsigned char* buf = CTool::common_buf;
			fseek( asd, data_pos, SEEK_SET );
			while( orisize )//データ部分の飛ばし
			{
				while( orisize && wptr != xptr )
				{
					incr( wptr ), orisize--;
				}
				if( orisize==0 )
					break;
				if( feof(asd) )
					goto finish;

				// 展開。辞書サイズ4096bytes の LZSS。
				unsigned char flag = fgetc(asd);
				for( int sft=8; sft; sft-- )
				{
					if( flag & (1<<(sft-1)) )
					{
						// bit-ON＝符号化されたデータ
						unsigned short code = (fgetc(asd)<<8)+fgetc(asd);
						int fptr = (xptr-1-(code&0xfff))&0xfff;
						int ln = (code>>12)+3;
						if( ln==18 )
							ln += verylong;

						while( ln-- )
						{
							buf[xptr] = buf[fptr];
							incr(xptr), incr(fptr);
						}
					}
					else
					{
						// bit-OFF＝素のまま
						buf[xptr] = fgetc(asd);
						incr(xptr);
					}
				}
			}
			data_pos=ftell( asd );
			continue;
		}

		// 出力先ファイルを開く
		char* cn = kiutil::pathMake( outname );
		FILE* out = fopen( cn, "wb" );

		// 展開処理
		fseek( asd, data_pos, SEEK_SET );
		bool eof = expand_data( asd, out, orisize );
		data_pos = ftell( asd );

		// 閉じて属性設定（面倒なのでCRCチェック無し）
		if( out )
		{
			fclose( out );
			kiutil::timeSet( cn, (unsigned short)(uptime>>16),
								 (unsigned short)(uptime&0xffff) );
			SetFileAttributes( cn, attr );
		}

		if( eof )
			break;
	}
finish:
	fclose( asd );
	return true;
}

// ------------------------------------------------------------------
// ASD 0.2.x : LZ77 + dual adaptive Huffman
//   reference: UNASD 0.2.0 (C) 1996-1998 Tobias Svensson
//   the 0.1.x stream is plain LZSS and is handled by expand_data()
// ------------------------------------------------------------------
namespace {

const int ASD20_MAX_CODES = 256 + 14*4;	// window_bits is 4 bits, <= 15
const int ASD20_MINHIT    = 2;			// 0.2.x uses 2 (0.1.x uses 3)
const int ASD20_MAX_FILES = 65536;

struct Asd20Bits {
	FILE* f;
	int rack, mask;
	void init( FILE* fp )	{ f=fp; rack=0; mask=0x80; }
	int bit() {
		if( mask==0x80 ) {
			int c = fgetc(f);
			if( c==EOF ) return -1;
			rack = c;
		}
		int v = (rack & mask) ? 1 : 0;
		mask >>= 1;
		if( !mask ) mask = 0x80;
		return v;
	}
	long bits( int n ) {
		long v = 0;
		for( int i=0; i<n; i++ ) {
			int c = bit();
			if( c<0 ) return -1;
			v = (v<<1) | c;
		}
		return v;
	}
};

// adaptive two-tree Huffman, exactly as the reference decoder does it.
// from_tree is not kept: only the encoder uses it.
struct Asd20Huff {
	int n;
	unsigned long count[ASD20_MAX_CODES*2];
	int high[ASD20_MAX_CODES];
	int low[ASD20_MAX_CODES];
	short counted[ASD20_MAX_CODES*2];
	unsigned long rehuff, rescale;
	unsigned long huffCounter, scaleCounter;
};

int asd20_lowerst( Asd20Huff* h ) {
	int i, lowerst = 0;
	unsigned long val = 0xFFFFFFFFul;
	for( i=0; i<h->n*2; i++ ) {
		if( h->counted[i]==0 && h->count[i]!=0 && h->count[i]<val ) {
			val = h->count[i];
			lowerst = i;
		}
	}
	return lowerst;
}

void asd20_build( Asd20Huff* h ) {
	int i, j, counts = h->n, root = h->n;
	unsigned long k;
	for( i=0; i<h->n*2; i++ ) h->counted[i] = 0;
	for( i=h->n; i<h->n*2; i++ ) h->count[i] = 0;
	while( counts>1 ) {
		i = asd20_lowerst(h);
		k = h->count[i];
		h->counted[i] = 1;
		j = asd20_lowerst(h);
		h->count[root] = h->count[j] + k;
		h->counted[j] = 1;
		h->low[root-h->n] = i;
		h->high[root-h->n] = j;
		root++;
		counts--;
	}
}

void asd20_rescale( Asd20Huff* h ) {
	int i;
	for( i=0; i<h->n; i++ ) {
		h->count[i] /= 2;
		if( h->count[i]==0 ) h->count[i] = 1;
	}
}

int asd20_get( Asd20Huff* h, Asd20Bits* br ) {
	int node = h->n*2 - 2;
	for(;;) {
		int c = br->bit();
		if( c<0 ) return -1;
		if( c==1 ) node = h->high[node-h->n];
		else       node = h->low[node-h->n];
		if( node < h->n ) {
			h->count[node]++;
			h->huffCounter++;
			h->scaleCounter++;
			if( h->huffCounter > h->rehuff ) {
				asd20_build(h);
				h->huffCounter = 0;
			}
			if( h->scaleCounter > h->rescale ) {
				h->scaleCounter = 0;
				asd20_rescale(h);
			}
			return node;
		}
	}
}

struct Asd20Entry {
	char name[256];
	unsigned long size;
	unsigned long crc;
	unsigned long ftime;
	unsigned short attr;
};

// per member output state; the CRC is kept over a block so the
// per byte overhead of zlib crc32() is avoided
struct Asd20Out {
	FILE* out;
	char path[1024];
	bool valid;
	unsigned long ftime;
	unsigned short attr;
	unsigned long crc;
	unsigned long stored;
	unsigned char blk[16384];
	unsigned int blkn;
	bool crcBad;
};

void asd20_flush( Asd20Out* o ) {
	if( o->blkn ) {
		if( o->out ) fwrite( o->blk, 1, o->blkn, o->out );
		o->crc = crc32( o->crc, o->blk, o->blkn );
		o->blkn = 0;
	}
}

// open a member; a member filtered out by CheckStr() is still
// decoded and CRC checked, but not written, exactly like the
// 0.1.x path does
void asd20_open( CAsdTool* tool, Asd20Entry* e, Asd20Out* o ) {
	o->out = NULL;
	o->valid = false;
	o->crc = 0;
	o->blkn = 0;
	o->stored = e->crc;
	o->ftime = e->ftime;
	o->attr = e->attr;
	if( !tool->CheckStr(e->name) ) return;
	char nm[256];
	lstrcpy( nm, e->name );
	char* cn = kiutil::pathMake( nm );
	lstrcpy( o->path, cn );
	o->out = fopen( o->path, "wb" );
	o->valid = ( o->out != NULL );
}

// finish a member: flush, verify the CRC, close and restore
// the timestamp and the attributes
void asd20_close( Asd20Out* o ) {
	asd20_flush(o);
	if( o->crc != o->stored ) o->crcBad = true;
	if( o->out ) {
		fclose(o->out);
		o->out = NULL;
	}
	if( o->valid ) {
		kiutil::timeSet( o->path, (unsigned short)(o->ftime>>16),
						 (unsigned short)(o->ftime&0xffff) );
		SetFileAttributes( o->path, o->attr );
		o->valid = false;
	}
}

} // namespace

bool CAsdTool::Extract20( FILE* asd )
{
	unsigned char hb[3];
	unsigned long num, i, total = 0, decoded = 0;
	Asd20Entry* ents = NULL;
	Asd20Bits br;
	Asd20Huff* trees = NULL;
	unsigned char* win = NULL;
	bool ret = false;
	bool done = false;
	int window_bits, buffer_bits, offRescale, offRehuff, litRescale, litRehuff;
	int litCodes, offCodes, k;
	unsigned long winSize, bpos = 0, fcount = 0, fidx = 0;
	Asd20Out o;
	memset( &o, 0, sizeof(o) );

	// the member count is 3 bytes wide in 0.2.x (2 bytes in 0.1.x)
	if( 3!=fread( hb, 1, 3, asd ) ) return false;
	num = (unsigned long)hb[0] | ((unsigned long)hb[1]<<8)
			| ((unsigned long)hb[2]<<16);
	if( num > ASD20_MAX_FILES ) return false;
	if( 0==num ) return true;

	ents = (Asd20Entry*)malloc(sizeof(Asd20Entry)*num);
	if( !ents ) return false;

	// member headers: name length, name, size, crc, time, attributes
	for( i=0; i<num; i++ ) {
		int cn = fgetc(asd);
		unsigned char t[14];
		if( cn==EOF ) goto cleanup;
		if( (size_t)cn!=fread( ents[i].name, 1, cn, asd ) ) goto cleanup;
		ents[i].name[cn] = '\0';
		if( 14!=fread( t, 1, 14, asd ) ) goto cleanup;
		ents[i].size  = (unsigned long)t[ 0] | ((unsigned long)t[ 1]<<8)
					  | ((unsigned long)t[ 2]<<16) | ((unsigned long)t[ 3]<<24);
		ents[i].crc   = (unsigned long)t[ 4] | ((unsigned long)t[ 5]<<8)
					  | ((unsigned long)t[ 6]<<16) | ((unsigned long)t[ 7]<<24);
		ents[i].ftime = (unsigned long)t[ 8] | ((unsigned long)t[ 9]<<8)
					  | ((unsigned long)t[10]<<16) | ((unsigned long)t[11]<<24);
		ents[i].attr  = (unsigned short)( t[12] | (t[13]<<8) );
		total += ents[i].size;
	}

	// the 0.2.x stream opens with a bit packed parameter block
	br.init(asd);
	window_bits = (int)br.bits(4);
	buffer_bits = (int)br.bits(5);
	offRescale  = (int)br.bits(7);
	offRehuff   = (int)br.bits(7);
	litRescale  = (int)br.bits(7);
	litRehuff   = (int)br.bits(7);

	// the reference archiver only ever emits window_bits 10 and
	// buffer_bits 16..23, so anything outside that is refused
	if( window_bits < 1 || window_bits > 15 ) goto cleanup;
	if( buffer_bits < 2  || buffer_bits > 24 ) goto cleanup;

	litCodes = 256 + (window_bits-1)*4;
	offCodes = buffer_bits*2;

	trees = (Asd20Huff*)malloc(sizeof(Asd20Huff)*2);
	if( !trees ) goto cleanup;
	trees[0].n = litCodes;
	trees[1].n = offCodes;
	for( k=0; k<litCodes; k++ ) trees[0].count[k] = 1;
	trees[0].rehuff  = (unsigned long)litCodes * litRehuff;
	trees[0].rescale = (unsigned long)litCodes * litRescale;
	trees[0].huffCounter = trees[0].scaleCounter = 0;
	asd20_build( &trees[0] );
	for( k=0; k<offCodes; k++ ) trees[1].count[k] = 1;
	trees[1].rehuff  = (unsigned long)offCodes * offRehuff;
	trees[1].rescale = (unsigned long)offCodes * offRescale;
	trees[1].huffCounter = trees[1].scaleCounter = 0;
	asd20_build( &trees[1] );

	winSize = 1UL << buffer_bits;
	win = (unsigned char*)malloc(winSize + ASD20_MINHIT + 1024);
	if( !win ) goto cleanup;
	memset( win, '0', winSize + ASD20_MINHIT + 1024 );

	asd20_open( this, &ents[0], &o );

	while( !done ) {
		int d, isCopy = 0;
		unsigned char c = 0;
		long hit = 1, pos = 0, l;

		// the reference stops as soon as the last member is full
		while( fidx < num && fcount == ents[fidx].size ) {
			asd20_close( &o );
			fidx++;
			fcount = 0;
			if( fidx >= num ) { done = true; break; }
			asd20_open( this, &ents[fidx], &o );
		}
		if( done ) break;
		if( m_Endflg ) break;

		d = asd20_get( &trees[0], &br );
		if( d<0 ) break;

		if( d > 255 ) {
			// a match: the literal code carries the length, the
			// offset code carries the distance
			long ilen, ct;
			if( d > 263 ) {
				int dd = d - 264;
				int nb = (dd>>2) + 1;
				long v = br.bits(nb);
				if( v<0 ) break;
				ilen = v + (1L << (nb+2)) + (long)((dd&3) << nb);
			} else {
				ilen = d - 256;
			}
			ct = asd20_get( &trees[1], &br );
			if( ct<0 ) break;
			if( ct < 4 ) {
				long v = br.bits(2);
				if( v<0 ) break;
				pos = v;
			} else {
				int nb = (int)((ct-2)/2);
				long v = br.bits(nb);
				if( v<0 ) break;
				pos = (1L << (ct/2))
					+ (long)((ct%2) * (1L << ((ct-2)/2))) + v;
			}
			hit = ilen + ASD20_MINHIT + 1;
			isCopy = 1;
		} else {
			c = (unsigned char)d;
		}

		for( l=0; l<hit; l++ ) {
			// the stream is continuous, so a match may span members
			while( fidx < num && fcount == ents[fidx].size ) {
				asd20_close( &o );
				fidx++;
				fcount = 0;
				if( fidx >= num ) { done = true; break; }
				asd20_open( this, &ents[fidx], &o );
			}
			if( done ) break;
			if( isCopy )
				c = win[(bpos + winSize - pos - 3) % winSize];
			o.blk[o.blkn++] = c;
			if( o.blkn == sizeof(o.blk) ) asd20_flush( &o );
			win[bpos] = c;
			bpos = (bpos+1) % winSize;
			fcount++;
			decoded++;
		}
	}

	asd20_close( &o );

	// the whole member set must have been produced and every
	// member CRC must match
	if( decoded != total ) goto cleanup;
	if( o.crcBad ) goto cleanup;
	ret = true;

cleanup:
	if( o.out ) { fclose(o.out); o.out = NULL; }
	if( win ) free(win);
	if( trees ) free(trees);
	if( ents ) free(ents);
	return ret;
}

void CAsdTool::init_expander( int x )
{
	// LZSSルーチン初期化
	wptr = xptr = 0;
	memset( CTool::common_buf, 0, 4096 );
	verylong = (x&0xff);
}


bool CAsdTool::expand_data( FILE* in, FILE* out, unsigned long len )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	while( len )
	{
		// バッファに残ってる物を出力
		while( len && wptr != xptr )
		{
			if( out )
				fputc( buf[wptr], out );
			incr( wptr ), len--;
		}
		if( len==0 )
			return false;
		if( feof(in) )
			return true;

		// 展開。辞書サイズ4096bytes の LZSS。
		unsigned char flag = fgetc(in);
		for( int sft=8; sft; sft-- )
		{
			if( flag & (1<<(sft-1)) )
			{
				// bit-ON＝符号化されたデータ
				unsigned short code = (fgetc(in)<<8)+fgetc(in);
				int fptr = (xptr-1-(code&0xfff))&0xfff;
				int ln = (code>>12)+3;
				if( ln==18 )
					ln += verylong;

				while( ln-- )
				{
					buf[xptr] = buf[fptr];
					incr(xptr), incr(fptr);
				}
			}
			else
			{
				// bit-OFF＝素のまま
				buf[xptr] = fgetc(in);
				incr(xptr);
			}
		}
	}

	return false;
}
#undef incr
