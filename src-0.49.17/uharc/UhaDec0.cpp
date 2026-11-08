/* Cores 0 STORE + 1 RLE + shared core selector. Clean-room. */

#include <stddef.h>
#include "UhaDec0.h"

int UhaCoreOf(int method, int ver)
{
	if( method < 0 )
		return -1;
	if( ver == 2 )
	{
		if( method > 43 )
			return -1;
		if( method <= 1 )
			return method;
		if( method <= 7 )
			return 2;
		return 3;
	}
	if( ver == 3 )
	{
		if( method == 80 )
			return 0;
		if( method > 79 )
			return -1;
		if( method <= 1 )
			return method;
		if( method <= 7 )
			return 2;
		if( method <= 21 )
			return 3;
		if( method <= 35 )
			return 4;
		return 5;
	}
	if( method > 87 )
		return -1;
	if( method <= 1 )
		return method;
	if( method <= 7 )
		return 2;
	if( method <= 23 )
		return 3;
	if( method <= 39 )
		return 4;
	return 5;
}

unsigned long UhaDictSize(int method, int ver)
{
	int base, span;
	if( ver == 2 )
	{
		if( method < 8 || method > 43 )
			return 0;
		base = 8; span = 12;
	}
	else if( ver == 3 )
	{
		if( method >= 8 && method <= 21 )
		{
			base = 8; span = 14;
		}
		else if( method >= 22 && method <= 35 )
		{
			base = 22; span = 14;
		}
		else if( method >= 36 && method <= 79 )
		{
			base = 36; span = 14;
		}
		else
			return 0;
	}
	else
	{
		if( method >= 8 && method <= 23 )
		{
			base = 8; span = 16;
		}
		else if( method >= 24 && method <= 39 )
		{
			base = 24; span = 16;
		}
		else if( method >= 40 && method <= 87 )
		{
			base = 40; span = 16;
		}
		else
			return 0;
	}
	return 1UL << (unsigned)(((method - base) % span) + 10);
}

int UhaDecStore(UhaReadCb rcb, void* rctx, UhaWriteCb wcb, void* wctx,
	unsigned long out_len)
{
	unsigned char buf[8192];
	unsigned long left = out_len;
	if( !rcb || !wcb )
		return 1;
	while( left )
	{
		unsigned want = left > sizeof(buf) ? sizeof(buf) : (unsigned)left;
		int n = rcb(rctx, buf, want);
		if( n <= 0 )
			return 3;
		if( wcb(wctx, buf, (unsigned)n) != 0 )
			return 4;
		left -= (unsigned long)n;
	}
	return 0;
}

/* Core 1 (RLE) is oracle-driven: it will be completed in Phase 2
 * against 02-R.UHA once the byte format is reproduced. Until then it
 * refuses loudly so the provider never emits garbage.
 */
int UhaDecRle(UhaReadCb rcb, void* rctx, UhaWriteCb wcb, void* wctx,
	unsigned long out_len)
{
	(void)rcb; (void)rctx; (void)wcb; (void)wctx; (void)out_len;
	return 3;
}
