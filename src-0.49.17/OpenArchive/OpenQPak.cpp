#include "stdafx.h"
#include "OpenQPak.h"

bool COpenQPak::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	Open_QPak();
	return Find_sub(lpSubInfo);
}

bool COpenQPak::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}
bool COpenQPak::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	PakIndex entry;
	static DWORD nowpos=-1;
	if(++nowpos==count)
	{nowpos=-1;return false;}
	memcpy(&entry,&COpenQPak::common_buf[nowpos*sizeof(PakIndex)],sizeof(PakIndex));

	lstrcpy(lpSubInfo->szFileName,entry.fname);
	for(int i = 0;lpSubInfo->szFileName[i]!=0x00;i++);

	lpSubInfo->dwCompressedSize=(DWORD)(entry.size+i+8);
	lpSubInfo->dwOriginalSize=(DWORD)entry.size;
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	//lstrcpy(lpSubInfo->szMode,"");

	return true;
}

bool COpenQPak::Open_QPak()
{
	PakIndex *entry;
	char dummy[8];
	DWORD sign;
	if( 1!=fread( &sign,4,1,COpenQPak::fp ) )
		return false;

	switch( sign )
	{
	// Quake1, Quake2, Half-Life, SiN( PACK/SPAK )
	case 0x4b434150:
	case 0x4b415053:
		{
			DWORD offset,contents;
			if( 1!=fread( &offset,4,1,COpenQPak::fp )
			 || 1!=fread( &contents,4,1,COpenQPak::fp ) )
			return false;

			fseek( COpenQPak::fp,0,SEEK_END );
			if( (unsigned)ftell(COpenQPak::fp) < offset+contents )
				return false;

			count = contents / ((sign==0x4b434150?0x38:0x78)+8);

			fseek( COpenQPak::fp,offset,SEEK_SET );
			entry = new PakIndex[count];
			for( DWORD i=0; i!=count; i++ )
			{
				fread( entry[i].fname, 1,
					sign==0x4b434150 ? 0x38 : 0x78, COpenQPak::fp );
				fread( &entry[i].offset, 4, 1, COpenQPak::fp );
				fread( &entry[i].size, 4, 1, COpenQPak::fp );
			}
		}break;

	// Quake, Half-life( WAD2/WAD3 )
	case 0x32444157:
	case 0x33444157:
		{
			DWORD offset;
			if( 1!=fread( &count,4,1,COpenQPak::fp )
			 || 1!=fread( &offset,4,1,COpenQPak::fp ) )
				return false;

			fseek( COpenQPak::fp,0,SEEK_END );
			if( (unsigned)ftell(COpenQPak::fp) < offset+count*0x20 )
				return false;

			fseek( COpenQPak::fp,offset,SEEK_SET );
			entry = new PakIndex[count];
			for( DWORD i=0; i!=count; i++ )
			{
				fread( &entry[i].offset, 4, 1, COpenQPak::fp);
				fread( &entry[i].size, 4, 1, COpenQPak::fp );
				fread( dummy, 1, 8, COpenQPak::fp );
				fread( entry[i].fname, 1, 0x10, COpenQPak::fp );
			}
		}break;

	// Doom, Half-Life( IWAD/PWAD )
	case 0x44415749:
	case 0x44415750:
		{
			DWORD offset;
			if( 1!=fread( &count,4,1,COpenQPak::fp )
			 || 1!=fread( &offset,4,1,COpenQPak::fp ) )
				return false;

			fseek( COpenQPak::fp,0,SEEK_END );
			if( (unsigned)ftell(COpenQPak::fp) < offset+count*0x10 )
				return false;

			fseek( COpenQPak::fp,offset,SEEK_SET );
			entry = new PakIndex[count];
			for( DWORD i=0; i!=count; i++ )
			{
				fread( &entry[i].offset, 4, 1, COpenQPak::fp );
				fread( &entry[i].size, 4, 1, COpenQPak::fp );
				fread( entry[i].fname, 1, 0x08, COpenQPak::fp );
			}
		}break;

	// ELSE
	default:
		if( 1!=fread( &sign,4,1,COpenQPak::fp ) )
			return false;
		// Delta Force ( PFF )
		if( (sign&0xffffff)!=0x464650 )
			return false;
		{
			DWORD recsize,offset;
			if( 1!=fread( &count,4,1,COpenQPak::fp )
			 || 1!=fread( &recsize,4,1,COpenQPak::fp )
			 || 1!=fread( &offset,4,1,COpenQPak::fp ) )
				return false;

			fseek( COpenQPak::fp,0,SEEK_END );
			if( (unsigned)ftell(COpenQPak::fp) < offset+count*recsize )
				return false;

			fseek(COpenQPak::fp,offset,SEEK_SET );
			entry = new PakIndex[count];
			for( DWORD i=0; i!=count; i++ )
			{
				fread( dummy, 1, 4, COpenQPak::fp );
				fread( &entry[i].offset, 4, 1, COpenQPak::fp );
				fread( &entry[i].size, 4, 1, COpenQPak::fp );
				fread( dummy, 1, 4, COpenQPak::fp );// date
				fread( entry[i].fname, 1, 0x10, COpenQPak::fp );
			}
		}break;
	}
	memcpy(COpenQPak::common_buf,entry,sizeof(PakIndex)*COpenQPak::count);
	delete [] entry;
	return true;
}