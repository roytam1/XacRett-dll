#include "stdafx.h"
#include "OpenXz.h"

// Multibyte integer (LE128), bounds-checked against end.
static DWORD xzVliM( const BYTE *b, DWORD end, DWORD *q )
{
	unsigned __int64 v = 0;
	DWORD s = 0;
	for(;;)
		{
		BYTE c;
		if( *q >= end ) return 0xFFFFFFFF;
		c = b[(*q)++];
		v |= (unsigned __int64)(c & 0x7F) << s;
		s += 7;
		if( !(c & 0x80) ) break;
		if( s >= 64 ) return 0xFFFFFFFF;
		}
	if( v > 0x7FFFFFFF ) return 0xFFFFFFFF;
	return (DWORD)v;
}

bool COpenXz::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	BYTE magic[6];
	BYTE foot[12];
	char outname[MAX_PATH];
	long fsize, idxstart;
	DWORD back, nrec, i, q, end;
	unsigned __int64 usum;
	BYTE *idx;
	bool ok;

	fseek( COpenXz::fp, 0, SEEK_END );
	fsize = ftell( COpenXz::fp );
	if( fsize < 32 )
		return false;
	fseek( COpenXz::fp, 0, SEEK_SET );
	if( 6 != fread(magic,1,6,COpenXz::fp) )
		return false;
	if( magic[0]!=0xFD || magic[1]!=0x37 || magic[2]!=0x7A ||
	    magic[3]!=0x58 || magic[4]!=0x5A || magic[5]!=0x00 )
		return false;
	// index via footer backward size (no full decode for listing)
	ok = false;
	idx = NULL;
	fseek( COpenXz::fp, fsize - 12, SEEK_SET );
	if( 12 == fread(foot,1,12,COpenXz::fp) &&
	    foot[10]==0x59 && foot[11]==0x5A )
		{
		back = (DWORD)(foot[4]|(foot[5]<<8)|(foot[6]<<16)|(foot[7]<<24));
		idxstart = fsize - 12 - (long)((back + 1) * 4);
		if( idxstart >= 12 && idxstart < fsize - 12 )
			{
			DWORD idxlen = (DWORD)(fsize - 12 - idxstart);
			idx = new BYTE[idxlen];
			fseek( COpenXz::fp, idxstart, SEEK_SET );
			if( idxlen == fread(idx,1,idxlen,COpenXz::fp) && idx[0]==0 )
				{
				q = 1; end = idxlen;
				nrec = xzVliM(idx, end, &q);
				if( nrec != 0xFFFFFFFF )
					{
					usum = 0;
					ok = true;
					for( i=0; i<nrec; i++ )
						{
						DWORD ua = xzVliM(idx, end, &q);
						DWORD ub = xzVliM(idx, end, &q);
						if( ua==0xFFFFFFFF || ub==0xFFFFFFFF ) { ok = false; break; }
						usum += (unsigned __int64)ub;
						}
					if( ok )
						lpSubInfo->dwOriginalSize = usum > 0xFFFFFFFF ? 0xFFFFFFFF : (DWORD)usum;
					}
				}
			delete [] idx;
			}
		}
	if( !ok )
		return false;
	kiutil::getOriginalName( outname,COpenXz::szArcFileName,
		0==stricmp(kiutil::pathExt(COpenXz::szArcFileName),"xz") ? NULL : "unxz" );
	lstrcpy(lpSubInfo->szFileName,outname);
	lpSubInfo->dwCompressedSize = (DWORD)fsize;
	lpSubInfo->dwCRC = 0;
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lstrcpy(lpSubInfo->szMode,"xz");
	lpSubInfo->wDate = 0;
	lpSubInfo->wTime = 0;
	return true;
}

bool COpenXz::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	(void)lpSubInfo;
	return false;
}
