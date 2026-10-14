#include "stdafx.h"
#include "XzDecode.h"
extern "C" {
#include "lzma/Lzma2Dec.h"
}
#include <stdlib.h>
#include <string.h>

// ---- ISzAlloc (malloc backed) ----
static void *xzDecAlloc( ISzAllocPtr p, size_t size )
	{ (void)p; return malloc(size); }
static void xzDecFree( ISzAllocPtr p, void *addr )
	{ (void)p; free(addr); }
static ISzAlloc g_xzDecAlloc = { xzDecAlloc, xzDecFree };

// CRC64-ECMA (reflected), bitwise.
static unsigned __int64 xzCrc64( unsigned __int64 crc, const BYTE *buf, DWORD n )
{
	while( n-- )
		{
		DWORD k;
		crc ^= *buf++;
		for( k=0; k<8; k++ )
			crc = (crc & 1) ? (crc >> 1) ^ (((unsigned __int64)0xC96C5795 << 32) | 0xD7870F42) : (crc >> 1);
		}
	return crc;
}

static DWORD xzCrcTab[256] = {
0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
};

// Composable CRC32 (init/final XOR per call).
static DWORD xzCrc32( DWORD crc, const BYTE *buf, DWORD n )
{
	crc ^= 0xffffffffL;
	while( n-- )
		crc = xzCrcTab[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
	return (crc^0xffffffffL);
}

// Multibyte integer from a buffer (bounds-checked).
static DWORD xzVliBuf( const BYTE *b, DWORD end, DWORD *q, bool *ok )
{
	unsigned __int64 v = 0;
	DWORD s = 0;
	for(;;)
		{
		BYTE c;
		if( *q >= end ) { *ok = false; return 0; }
		c = b[(*q)++];
		v |= (unsigned __int64)(c & 0x7F) << s;
		s += 7;
		if( !(c & 0x80) ) break;
		if( s >= 64 ) { *ok = false; return 0; }
		}
	if( v > 0x7FFFFFFF ) { *ok = false; return 0; }
	*ok = true;
	return (DWORD)v;
}

struct XzIn
{
	BYTE *buf;
	DWORD cap, pos, len;
	XzFillFn fill;
	void *ictx;
	bool err;
	unsigned __int64 used;
	bool accOn;
	DWORD accCrc;
};

static bool xzFill( XzIn *io, DWORD n )
{
	while( io->len - io->pos < n )
		{
		DWORD want;
		int got;
		if( io->pos > 0 )
			{
			if( io->len > io->pos )
				memmove( io->buf, io->buf + io->pos, io->len - io->pos );
			io->len -= io->pos;
			io->pos = 0;
			}
		want = io->cap - io->len;
		if( want==0 )
			break;
		got = io->fill( io->ictx, io->buf + io->len, (int)want );
		if( got <= 0 )
			break;
		io->len += (DWORD)got;
		}
	return (io->len - io->pos) >= n;
}

static BYTE xzGet( XzIn *io )
{
	BYTE b;
	if( !xzFill(io, 1) ) { io->err = true; return 0; }
	b = io->buf[io->pos++];
	io->used++;
	if( io->accOn ) io->accCrc = xzCrc32(io->accCrc, &b, 1);
	return b;
}

static bool xzTake( XzIn *io, BYTE *dst, DWORD n )
{
	DWORD i;
	for( i=0; i<n; i++ )
		dst[i] = xzGet(io);
	return !io->err;
}

static DWORD xzVli( XzIn *io )
{
	unsigned __int64 v = 0;
	DWORD s = 0;
	for(;;)
		{
		BYTE b = xzGet(io);
		if( io->err ) return 0;
		v |= (unsigned __int64)(b & 0x7F) << s;
		s += 7;
		if( !(b & 0x80) ) break;
		if( s >= 64 ) { io->err = true; return 0; }
		}
	if( v > 0x7FFFFFFF ) { io->err = true; return 0; }
	return (DWORD)v;
}

bool XzDecodeStream(XzFillFn fill, void *ictx,
                    XzWriteFn write, void *octx,
                    DWORD usz, bool exact)
{
	XzIn in;
	BYTE *inbuf;
	CLzma2Dec st2;
	bool st2live;
	DWORD nout, nblocks, check, checklen;
	unsigned __int64 usum;
	BYTE fbyte[2], hb[4], foot[12], cb[8], hbuf[1024];
	DWORD i, v, hsz, q, fb, nf, fid, psz, prop, dictSize;
	DWORD csStated, usStated, dataLen, outThis, nrec, ua, ub;
	DWORD bCrc;
	unsigned __int64 idxUsed, indexLen;
	unsigned __int64 bC64;
	bool hasCs, hasUs, vok;
	ELzmaStatus stat;
	SRes r;
	SizeT slen, limit, room, remain, prevPos;
	static const BYTE magic[6] = { 0xFD,'7','z','X','Z',0x00 };

	inbuf = new BYTE[XACR_BUFSIZE];
	in.buf = inbuf; in.cap = (DWORD)XACR_BUFSIZE;
	in.pos = 0; in.len = 0;
	in.fill = fill; in.ictx = ictx; in.err = false; in.used = 0;
	in.accOn = false; in.accCrc = 0;
	st2live = false;
	nout = 0; nblocks = 0; usum = 0;
	Lzma2Dec_Construct(&st2);

	// stream header: magic + flags + CRC32(flags)
	for( i=0; i<6; i++ )
		if( xzGet(&in) != magic[i] ) goto fail;
	if( !xzTake(&in, fbyte, 2) ) goto fail;
	if( fbyte[0]!=0 ) goto fail;
	check = fbyte[1];
	if( check!=0 && check!=1 && check!=4 ) goto fail;
	checklen = check==0 ? 0 : check==1 ? 4 : 8;
	if( !xzTake(&in, hb, 4) ) goto fail;
	v = (DWORD)(hb[0]|(hb[1]<<8)|(hb[2]<<16)|(hb[3]<<24));
	if( xzCrc32(0, fbyte, 2) != v ) goto fail;

	// blocks run until the 0x00 index marker
	for(;;)
		{
		BYTE hb0;
		if( !xzFill(&in, 1) ) goto fail;
		if( in.buf[in.pos]==0 ) break;
		hb0 = xzGet(&in); if( in.err ) goto fail;
		hsz = ((DWORD)hb0 + 1) * 4;
		if( hsz < 8 || hsz > 1024 ) goto fail;
		hbuf[0] = hb0;
		if( !xzTake(&in, hbuf+1, hsz-1) ) goto fail;
		fb = hbuf[1]; q = 2;
		hasCs = (fb & 1)!=0; hasUs = (fb & 2)!=0;
		nf = (fb >> 2) + 1;
		csStated = 0; usStated = 0;
		if( hasCs ) { csStated = xzVliBuf(hbuf, hsz, &q, &vok); if(!vok) goto fail; }
		if( hasUs ) { usStated = xzVliBuf(hbuf, hsz, &q, &vok); if(!vok) goto fail; }
		if( nf != 1 ) goto fail;
		fid = xzVliBuf(hbuf, hsz, &q, &vok); if(!vok) goto fail;
		psz = xzVliBuf(hbuf, hsz, &q, &vok); if(!vok) goto fail;
		if( fid != 0x21 || psz != 1 ) goto fail;
		if( q + 1 > hsz ) goto fail;
		prop = hbuf[q++];
		while( (q & 3)!=0 )
			{ if( q >= hsz || hbuf[q]!=0 ) goto fail; q++; }
		if( q + 4 != hsz ) goto fail;
		v = (DWORD)(hbuf[q]|(hbuf[q+1]<<8)|(hbuf[q+2]<<16)|(hbuf[q+3]<<24));
		if( xzCrc32(0, hbuf, q) != v ) goto fail;
		if( prop > 40 ) goto fail;
		dictSize = (2 + (prop & 1)) << (prop / 2 + 11);
		if( dictSize < 4096 || dictSize > 0x4000000 ) goto fail;
		Lzma2Dec_Construct(&st2);
		if( Lzma2Dec_Allocate(&st2, (Byte)prop, &g_xzDecAlloc) != SZ_OK ) goto fail;
		st2live = true;
		Lzma2Dec_Init(&st2);
		dataLen = 0; outThis = 0;
		bCrc = 0; bC64 = ~(unsigned __int64)0;
		stat = LZMA_STATUS_NOT_FINISHED;
		for(;;)
			{
			SizeT dbs = st2.decoder.dicBufSize;
			if( st2.decoder.dicPos == dbs )
				{
				write( octx, st2.decoder.dic, (int)dbs );
				if( check==1 ) bCrc = xzCrc32(bCrc, st2.decoder.dic, (DWORD)dbs);
				else if( check==4 ) bC64 = xzCrc64(bC64, st2.decoder.dic, (DWORD)dbs);
				nout += (DWORD)dbs; outThis += (DWORD)dbs;
				st2.decoder.dicPos = 0;
				}
			if( exact && nout + (DWORD)st2.decoder.dicPos > usz ) goto fail;
			if( stat == LZMA_STATUS_FINISHED_WITH_MARK ) break;
			if( exact && nout + (DWORD)st2.decoder.dicPos == usz )
				{
				BYTE m;
				if( !xzFill(&in, 1) ) goto fail;
				m = in.buf[in.pos];
				if( m != 0x00 ) goto fail;
				in.pos++; in.used++; dataLen++;
				break;
				}
			if( !xzFill(&in, 1) ) goto fail;
			room = dbs - st2.decoder.dicPos;
			if( exact )
				{
				remain = (SizeT)(usz - nout - (DWORD)st2.decoder.dicPos);
				limit = st2.decoder.dicPos + (remain < room ? remain : room);
				}
			else
				limit = dbs;
			slen = (SizeT)(in.len - in.pos);
			prevPos = st2.decoder.dicPos;
			r = Lzma2Dec_DecodeToDic(&st2, limit, in.buf + in.pos, &slen, LZMA_FINISH_ANY, &stat);
			in.pos += (DWORD)slen; in.used += (DWORD)slen; dataLen += (DWORD)slen;
			if( r != SZ_OK ) goto fail;
			if( slen==0 && st2.decoder.dicPos==prevPos ) goto fail;
			}
		if( st2.decoder.dicPos > 0 )
			{
			DWORD n = (DWORD)st2.decoder.dicPos;
			write( octx, st2.decoder.dic, (int)n );
			if( check==1 ) bCrc = xzCrc32(bCrc, st2.decoder.dic, n);
			else if( check==4 ) bC64 = xzCrc64(bC64, st2.decoder.dic, n);
			nout += n; outThis += n;
			st2.decoder.dicPos = 0;
			}
		if( exact && nout > usz ) goto fail;
		if( hasCs && dataLen != csStated ) goto fail;
		if( hasUs && outThis != usStated ) goto fail;
		v = (4 - (dataLen & 3)) & 3;
		for( i=0; i<v; i++ )
			{ BYTE pb2 = xzGet(&in); if( in.err || pb2!=0 ) goto fail; }
		if( checklen > 0 )
			{
			if( !xzTake(&in, cb, checklen) ) goto fail;
			if( check==1 )
				{
				v = (DWORD)(cb[0]|(cb[1]<<8)|(cb[2]<<16)|(cb[3]<<24));
				if( bCrc != v ) goto fail;
				}
			else
				{
				unsigned __int64 e = (unsigned __int64)cb[0] | ((unsigned __int64)cb[1]<<8) |
					((unsigned __int64)cb[2]<<16) | ((unsigned __int64)cb[3]<<24) |
					((unsigned __int64)cb[4]<<32) | ((unsigned __int64)cb[5]<<40) |
					((unsigned __int64)cb[6]<<48) | ((unsigned __int64)cb[7]<<56);
				if( (bC64 ^ ~(unsigned __int64)0) != e ) goto fail;
				}
			}
		Lzma2Dec_Free(&st2, &g_xzDecAlloc);
		st2live = false;
		nblocks++;
		}
	idxUsed = in.used;
	in.accOn = true; in.accCrc = 0;
	{ BYTE mk = xzGet(&in); if( in.err || mk!=0 ) goto fail; }
	nrec = xzVli(&in); if( in.err ) goto fail;
	if( nrec != nblocks ) goto fail;
	usum = 0;
	for( i=0; i<nrec; i++ )
		{
		ua = xzVli(&in); if( in.err ) goto fail;
		ub = xzVli(&in); if( in.err ) goto fail;
		if( ua==0 ) goto fail;
		if( exact && ub > usz ) goto fail;
		usum += (unsigned __int64)ub;
		if( exact && usum > usz ) goto fail;
		}
	if( exact && usum != usz ) goto fail;
	if( !exact && usum != nout ) goto fail;
	for(;;)
		{
		DWORD usedNow = (DWORD)in.used;
		if( (usedNow & 3)==0 ) break;
		{ BYTE pb2 = xzGet(&in); if( in.err || pb2!=0 ) goto fail; }
		}
	{ BYTE icrc[4]; DWORD iv;
	  in.accOn = false;
	  if( !xzTake(&in, icrc, 4) ) goto fail;
	  iv = (DWORD)(icrc[0]|(icrc[1]<<8)|(icrc[2]<<16)|(icrc[3]<<24));
	  if( in.accCrc != iv ) goto fail; }
	indexLen = in.used - idxUsed;
	if( !xzTake(&in, foot, 12) ) goto fail;
	if( foot[10]!=0x59 || foot[11]!=0x5A ) goto fail;
	if( foot[8]!=fbyte[0] || foot[9]!=fbyte[1] ) goto fail;
	v = (DWORD)(foot[4]|(foot[5]<<8)|(foot[6]<<16)|(foot[7]<<24));
	if( (indexLen & 3)!=0 || indexLen / 4 - 1 != v ) goto fail;
	v = (DWORD)(foot[0]|(foot[1]<<8)|(foot[2]<<16)|(foot[3]<<24));
	if( xzCrc32(0, foot+4, 6) != v ) goto fail;
	if( in.pos != in.len ) goto fail;
	{
		BYTE probe;
		int pr = fill( ictx, &probe, 1 );
		if( pr != 0 ) goto fail;
	}
	if( exact && nout != usz ) goto fail;
	delete [] inbuf;
	return true;
fail:
	if( st2live ) Lzma2Dec_Free(&st2, &g_xzDecAlloc);
	if( inbuf ) delete [] inbuf;
	return false;
}
