#include "stdafx.h"
#include "OpenBlz.h"

#if CHAR_BIT == 8
# define octet(v) ((byte) (v))
#else
# define octet(v) ((v) & 0x00ffu)
#endif

bool COpenBlz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	// LongFileNameŽæ“¾
	WIN32_FIND_DATA find;
	HANDLE h = FindFirstFile( COpenBlz::szArcFileName,&find );
	if( h==INVALID_HANDLE_VALUE )
		return false;
	
	FindClose( h );
	kiutil::getOriginalName( lpSubInfo->szFileName,find.cFileName,
			0==stricmp(kiutil::pathExt(find.cFileName),"blz") ? NULL : "unblz" );
	lstrcpy(lpSubInfo->szAttribute ,"A---");//‰½‚©“ä^^;

	unsigned char* buf=COpenBlz::common_buf;
	fread( buf,1,30,fp);

	lpSubInfo->dwCompressedSize=get_uint32(buf + 2*4);
	lpSubInfo->dwOriginalSize=get_uint32(buf + 4*4);
	lpSubInfo->dwCRC=get_uint32(buf + 4*5);

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	lstrcpy(lpSubInfo->szMode,"BLZ");

	return true;
}

bool COpenBlz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}

unsigned long COpenBlz::get_uint32(const byte *p)
{
    return ((unsigned long) octet(p[0]) << 24) |
           ((unsigned long) octet(p[1]) << 16) |
           ((unsigned long) octet(p[2]) <<  8) |
           ((unsigned long) octet(p[3])      );
}