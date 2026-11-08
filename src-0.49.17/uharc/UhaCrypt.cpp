/* UHARC header + table ciphers. Clean-room from FORMAT.md section 6. */

#include <string.h>
#include "UhaCrypt.h"

static const char kDefaultPwd[] = "UHARC Default Password";

/* The shared 32-round schedule: for each 8-byte offset, CRC the whole
 * 256-byte S-box and XOR back crc / ~crc. It runs over the partially
 * built S-box exactly as the original does (init state visible).
 */
static unsigned long sbox_crc(const unsigned char* d, unsigned n)
{
	unsigned long crc = 0xFFFFFFFFUL;
	unsigned i;
	static unsigned long tab[256];
	static int tab_ready = 0;
	if( !tab_ready )
	{
		unsigned k, j;
		for( k = 0; k < 256; k++ )
		{
			unsigned long c = k;
			for( j = 0; j < 8; j++ )
				c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
			tab[k] = c;
		}
		tab_ready = 1;
	}
	for( i = 0; i < n; i++ )
		crc = tab[(unsigned char)(crc ^ d[i])] ^ (crc >> 8);
	return crc;
}

void UhaHeaderDecrypt(unsigned char* raw, unsigned len, unsigned char key)
{
	unsigned i;
	for( i = 0; i < len; i++ )
		raw[4 + i] ^= (unsigned char)((unsigned)key + i * i);
}

void UhaSBoxInit(UhaSBox* b, const char* pwd, int is02)
{
	unsigned k, off;
	unsigned long crc;
	const char* p = pwd ? pwd : kDefaultPwd;
	unsigned plen = (unsigned)strlen(p);
	if( plen == 0 )
		plen = 1;

	if( is02 )
	{
		for( k = 0; k < 256; k++ )
			b->s[k] = (unsigned char)p[k % plen];
	}
	else
	{
		b->s[0] = (unsigned char)plen;
		for( k = 0; k < 255; k++ )
			b->s[1 + k] = (unsigned char)p[k % plen];
	}

	for( off = 0; off < 256; off += 8 )
	{
		crc = sbox_crc(b->s, 256);
		for( k = 0; k < 4; k++ )
		{
			b->s[off + k] ^= (unsigned char)((crc >> (8 * k)) & 0xFF);
			b->s[off + 4 + k] ^= (unsigned char)((~crc >> (8 * k)) & 0xFF);
		}
	}
	b->pos = 0;
	b->mix = 0xFE;
	b->is02 = is02;
}

void UhaSBoxUpdate(UhaSBox* b, unsigned char* buf, unsigned long len)
{
	unsigned long i;
	for( i = 0; i < len; i++ )
	{
		unsigned char in = buf[i];
		if( b->is02 )
		{
			unsigned char a = b->s[b->pos];
			b->pos = (b->pos + 1) & 255;
			b->s[b->pos] = (unsigned char)(b->s[b->pos] + a);
			buf[i] = (unsigned char)(b->s[b->pos] ^ in);
		}
		else
		{
			unsigned char a = b->s[b->pos];
			unsigned char b2;
			b->pos = (b->pos + 1) & 255;
			b2 = b->s[b->pos];
			b->mix = (b->mix + 1) & 255;
			b->s[b->pos] = (unsigned char)(b2 + a);
			buf[i] = (unsigned char)(in ^ (unsigned char)(b2 + a));
			b->s[b->mix] = (unsigned char)(b->s[b->mix] - b2);
		}
	}
}
