#include "stdafx.h"
#include "OpenImp.h"

// IMP_DIR_ENTRY flags
#define DFLAG_DIR     1
#define DFLAG_E8SHORT 2
#define DFLAG_E8LONG  4

// IMP_HEADER flags
#define HFLAG_MULTIVOL 1
#define HFLAG_LASTVOL  2
#define HFLAG_BLOWFISH 4

// methods
#define METHOD_STORE   0
#define METHOD_LZ77    1
#define METHOD_BWT     2
#define METHOD_LZ77_MM 3

// LZ77
#define NLLCODES 288 /* Number of literal-length codes */
#define NDCODES 42   /* Number of distance codes */
#define ND2CODES 14  /* Number of distance codes for length==2 */
#define ND3CODES 28  /* Number of distance codes for length==3 */
#define MIN_LZ_CODES (NLLCODES+NDCODES+ND2CODES)
#define LONGMATCHBITS 15  /*extra bits for long match*/
#define LONGMATCHMASK ((1<<LONGMATCHBITS)-1)
#define LONGMATCHCODE (NLLCODES-2)
#define MMCODE (NLLCODES-1) /*multimedia code*/
#define LLTTBITS 9
#define DTTBITS 7
#define LLTTSIZE (1<<LLTTBITS)
#define DTTSIZE (1<<DTTBITS)
#define MAX_MM_CODES 1024

// BWT
#define NCODES 258
#define WINDOW 50
#define BWTABLES 6
#define BWTTBITS 9
#define BWTTSIZE (1<<BWTTBITS)

#define MAX_NCODES NLLCODES
#define MAXCODELEN 16
#define NLENCODES (MAXCODELEN+5) /* no. of code length codes */
#define ZRUN (MAXCODELEN+1) /* run of zeros */
#define DZRUN (MAXCODELEN+3)/* run of delta-zeros*/
#define HC_MCL 14    /* max code length for storing Huffman codes */
#define HCTTBITS 6
#define HCTTSIZE (1<<HCTTBITS)

bool COpenImp::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	imp.imp = fp;
	if( imp.imp==NULL )return false;

	if( !imp.ReadImpHead() )
		{fclose(imp.imp);return false;}
	if(imp.ih.flags&HFLAG_MULTIVOL)
		{fclose(imp.imp);return false;}
	if(imp.ih.flags&HFLAG_BLOWFISH)
		{fclose(imp.imp);return false;}
	
	imp.g_blockbuffer=0;imp.g_prevfpos=0xffffffff;
	imp.dirbufsrc = imp.dirbufend = imp.dirbuf;
	ent=0;
	
	return Find_Sub(lpSubInfo);
}

bool COpenImp::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_Sub(lpSubInfo);
}

bool COpenImp::Find_Sub(INDIVIDUALINFO *lpSubInfo)
{
	char outname[MAX_PATH];
	IMP_DIR_ENTRY dirent;

	if(ent==imp.ih.direntries) return false;
	ent++;


	if( !imp.get_dir_entry(&dirent,outname) )
		return false;
	//name = kiutil::pathMake( outname );
	if( dirent.flags&DFLAG_DIR )return Find_Sub(lpSubInfo);

	//î•ñŠi”[
	
	lstrcpy(lpSubInfo->szFileName,outname);
	lpSubInfo->dwCRC=dirent.crc;
	lpSubInfo->dwOriginalSize=dirent.esize;
	lpSubInfo->dwCompressedSize=0;//imp.ih.csize;
	lpSubInfo->wRatio = (lpSubInfo->dwOriginalSize + 1) * 100 / (lpSubInfo->dwCompressedSize + 1);
	lpSubInfo->wDate=dirent.mdate;
	lpSubInfo->wTime=dirent.mtime;
	{
        lstrcpy(lpSubInfo->szAttribute,"-----");
        if (dirent.attrib & _A_ARCH)
        {
            lpSubInfo->szAttribute[0] = 'A';
        }
        if (dirent.attrib & _A_HIDDEN)
        {
            lpSubInfo->szAttribute[1] = 'H';
        }
        if (dirent.attrib & _A_RDONLY)
        {
            lpSubInfo->szAttribute[2] = 'D';
        }
        if (dirent.attrib & _A_SUBDIR)
        {
            lpSubInfo->szAttribute[3] = 'R';
        }
        if (dirent.attrib & _A_SYSTEM)
        {
            lpSubInfo->szAttribute[4] = 'S';
        }
    }
	return true;
}