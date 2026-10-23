#ifndef AFX_ALZDECODE_H__ALZ_ENGINE_INCLUDED__
#define AFX_ALZDECODE_H__ALZ_ENGINE_INCLUDED__

#include "Tool.h"
#include <stdio.h>

// The shared ALZ container walker, this tree's rewrite of the
// reference implementation in alzgg/src/cli_alz.c (HAZO unalz).
//
//   .alz (main, volume 0) + .a00, .a01, ... (continuations)
//
// Every volume is
//   [0..7]     "ALZ\x01" + 4 ignored bytes
//   [8..fsiz-17]  the data region
//   [fsiz-16..fsiz-1]  the END record ("CLZ\x01" + 8 zero bytes +
//                      "CLZ\x02" on the last volume, "CLZ\x03" before)
//
// The walk never needs ftell/fseek on the caller side: AlzNextMember()
// leaves the cursor right at the start of the next item.

#define ALZ_MAXVOLS   128
#define ALZ_HDRSZ     8
#define ALZ_ENDSZ     16
#define ALZ_RANDHED   12
#define ALZ_NAME_MAX  4096

// fd bits
#define ALZ_FD_ENC    0x01
#define ALZ_FD_NB     0xF0   // (fd>>4): the width of each size field

// methods
#define ALZ_M_STORE   0
#define ALZ_M_DEFLATE 2

class AlzChain
{
public:
	AlzChain()
		{for( int i=0; i!=ALZ_MAXVOLS; i++ ) vol[i]=NULL; nv=0; v=0; vpos=0;}
	~AlzChain()
		{Close();}

	bool Open( const char* main );      // the main .alz; finds .a00 and up
	void Close();

	// member-Data reading: bounded by the data region and transparent
	// over the volume boundaries (like the reference span_read)
	int      read( void* buf, int len );        // short at the end of a volume
	LONGLONG skip( LONGLONG len );              // bytes actually skipped
	bool     magic4( unsigned long* out );      // the 4 magic bytes at the walk edge
	bool     readhead( void* buf, int len );    // Header bytes, one volume only

	bool getpos( int* ov, long* op )
		{*ov=v; *op=vpos; return v<nv;}
	bool setpos( int av, long p );
	bool Valid() const    // true while the walk can continue
		{return v < nv && vol[v] != NULL;}

protected:
	bool nextvol();

	FILE* vol[ALZ_MAXVOLS];
	int   nv;                   // number of volumes, 1..ALZ_MAXVOLS
	int   v;                    // the current volume
	long  vpos;                 // the (raw) offset inside the current volume
	long  fsiz[ALZ_MAXVOLS];    // the raw size of every volume
};

struct AlzMember
{
	unsigned short name_len;
	unsigned char  attr;    // 0x01 RO, 0x02 hidden, 0x04 system,
							// 0x10 Directory, 0x20 Archive
	unsigned short time;    // DOS time
	unsigned short date;    // DOS date
	unsigned char  fd;      // ALZ_FD_ENC + ALZ_FD_NB
	unsigned char  method;  // 0 = store, 2 = deflate (1/3 unsupported)
	unsigned long  crc;     // CRC-32 of the UNcompressed bytes
	LONGLONG       csize;   // excludes the 12-byte verifier when encrypted
	LONGLONG       usize;

	void Zero()
		{name_len=0; attr=0; time=0; date=0; fd=0; method=0;
		 crc=0; csize=0; usize=0;}
};

// returns  1 = the member is parsed, the cursor sits at the Data
//          0 = END record (the cursor sits past it, the walk is done)
//         -1 = structural error (bad magic, oversized header, ...)
int  AlzNextMember( AlzChain* chain, AlzMember* m, char* name );

// skip a member's data span, when it carries one
void AlzSkipMember( AlzChain* chain, const AlzMember* m );

#endif
