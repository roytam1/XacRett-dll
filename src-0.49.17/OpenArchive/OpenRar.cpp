#include "stdafx.h"
#include "OpenRar.h"
#include "..\unrar\dll.hpp" /* UnRAR 7.x RAR5 list path */

bool COpenRar::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	if (rar5_state == 0) {
		rar5_state = 1;
		static const unsigned char RAR5SIG[8] = { 0x52,0x61,0x72,0x21,0x1a,0x07,0x01,0x00 };
		unsigned char sig[8];
		if (fp != NULL && fread(sig, 1, 8, fp) == 8 && 0 == memcmp(sig, RAR5SIG, 8))
			rar5_state = 2;
		if (fp != NULL) fseek(fp, 0, SEEK_SET);
	}
	if (rar5_state == 2)
		return FindSub5(lpSubInfo);
	return FindSub(lpSubInfo);
}

bool COpenRar::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	if (rar5_state == 2)
		return FindSub5(lpSubInfo);
	return FindSub(lpSubInfo);
}

bool COpenRar::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	static bNeedNextVol=false;
	static bool bFirstVol=true,bFirstFile=true;

	int Size=ReadBlock(FILE_HEAD|READSUBBLOCK);

	if( Size<=0 && !bNeedNextVol )
		return false;
	if( BlockHead.HeadType==SUB_HEAD )
	{fseek(COpenRar::fp,NextBlockPos,SEEK_SET);FindSub(lpSubInfo);}

	// マルチボリューム処理
	if( (NewLhd.Flags&LHD_SPLIT_BEFORE) && bSolid && bFirstVol )
		return false;

	bFirstVol=false;
	if( bNeedNextVol && Size==0 && !GetNextVolume() )
		return false;
	bNeedNextVol=(0!=(NewLhd.Flags&LHD_SPLIT_AFTER));

	// 圧縮データ位置へ飛ぶ
	//fseek(COpenRar::fp,NextBlockPos-NewLhd.PackSize,SEEK_SET);

	if( !(NewLhd.Flags&LHD_SPLIT_BEFORE) )
	{
		if( NewLhd.FileAttr&0x08 )
			FindSub(lpSubInfo);

		if( NewLhd.HostOS==UNIX ){NewLhd.FileAttr=(NewLhd.FileAttr&0x40)?0x10:0x20;}

		if( NewLhd.FileAttr&0x10 )
			FindSub(lpSubInfo);

		lstrcpy(lpSubInfo->szFileName,ExtrName);
		lpSubInfo->dwCompressedSize = NewLhd.PackSize;
		lpSubInfo->dwOriginalSize = NewLhd.UnpSize;
		lpSubInfo->dwCRC = NewLhd.FileCRC;

		lstrcpy(lpSubInfo->szAttribute ,"----W");
		if (NewLhd.FileAttr & FILE_ATTRIBUTE_READONLY)
			lpSubInfo->szAttribute[4] = 'R';
		if (NewLhd.FileAttr & FILE_ATTRIBUTE_HIDDEN)
			lpSubInfo->szAttribute[2] = 'H';
		if (NewLhd.FileAttr & FILE_ATTRIBUTE_SYSTEM)
			lpSubInfo->szAttribute[3] = 'S';
		if (NewLhd.FileAttr & FILE_ATTRIBUTE_DIRECTORY)
			lpSubInfo->szAttribute[1] = 'D';
		if (NewLhd.FileAttr & FILE_ATTRIBUTE_ARCHIVE)
			lpSubInfo->szAttribute[0] = 'A';

		//NewLhd.Method is '0'-'5' for old headers, 0-5 for new ones
		{ int m=NewLhd.Method; if(m>='0') m-='0';
		  if(m>=0&&m<=5) wsprintf(lpSubInfo->szMode,"m%d",m);
		  else lstrcpy(lpSubInfo->szMode,"?"); }

		lpSubInfo->uOSType=NewLhd.HostOS;

		double tmp1 = (double)lpSubInfo->dwOriginalSize;
		double tmp2 = (double)lpSubInfo->dwCompressedSize;
		if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
			lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
		}

		lpSubInfo->wDate=(WORD)(NewLhd.FileTime>>16);
		lpSubInfo->wTime=(WORD)NewLhd.FileTime;
	}

	if(COpenRar::fp==NULL)
		return false;

	bFirstFile=false;
	fseek(COpenRar::fp,NextBlockPos,SEEK_SET);
	return true;
}
bool COpenRar::GetNextVolume()
{
	fclose(COpenRar::fp);

	char *ChPtr;
	if( (ChPtr=strrchr(ArcName,'.'))==NULL )
		strcat(ArcName,".rar"),ChPtr=strrchr(ArcName,'.');
	else if( ChPtr[1]==0 || stricmp(ChPtr+1,"exe")==0 )
		strcpy(ChPtr+1,"rar");

	if( !('0'<=ChPtr[2] && ChPtr[2]<='9') || !('0'<=ChPtr[3] && ChPtr[3]<='9') )
		strcpy(ChPtr+2,"00");
	else
	{
		ChPtr+=3;
		while( (++(*ChPtr))=='9'+1 )
		{
			if( *(ChPtr-1)=='.' )
			{	*ChPtr='A';	break;	}
			else
			{	*ChPtr='0';	ChPtr--;}
		}
	}

	if( (COpenRar::fp=fopen(ArcName,"rb"))==NULL )
		return false;
//	if( !IsArchive(false) )
//		return false;

	fseek(COpenRar::fp,NewMhd.HeadSize-MainHeadSize,SEEK_CUR);
	ReadBlock(FILE_HEAD);
	fseek(COpenRar::fp,NextBlockPos-NewLhd.PackSize,SEEK_SET);
	bNeedNextVol=(0!=(NewLhd.Flags&LHD_SPLIT_AFTER));

	UnpPackedSize=NewLhd.PackSize;
	PackedCRC=0xFFFFFFFF;
	return true;
}

int COpenRar::ReadHeader(int BlockType)
{
	int Size;
	BYTE Header[64];

#define GetHeaderByte(N) Header[N]
#define GetHeaderWord(N) (Header[N]+((WORD)Header[N+1]<<8))
#define GetHeaderDword(N) (Header[N]+((WORD)Header[N+1]<<8)+((DWORD)Header[N+2]<<16)+((DWORD)Header[N+3]<<24))
	switch(BlockType)
	{
	case MAIN_HEAD:
		if(ArcFormat==OLD)
		{
			Size=fread(Header,1,SIZEOF_OLDMHD,COpenRar::fp);
			memcpy(OldMhd.Mark,Header,4);
			OldMhd.HeadSize=GetHeaderWord(4);
			OldMhd.Flags=GetHeaderByte(6);
		}
		else
		{
			Size=fread(Header,1,SIZEOF_NEWMHD,COpenRar::fp);
			NewMhd.HeadCRC=GetHeaderWord(0);
			NewMhd.HeadType=GetHeaderByte(2);
			NewMhd.Flags=GetHeaderWord(3);
			NewMhd.HeadSize=GetHeaderWord(5);
			NewMhd.Reserved=GetHeaderWord(7);
			NewMhd.Reserved1=GetHeaderDword(9);
			HeaderCRC=UpdateChecker(0xFFFFFFFFL,&Header[2],SIZEOF_NEWMHD-2,true);
		}
		break;
    case FILE_HEAD:
		if( ArcFormat==OLD )
		{
			Size=fread(Header,1,SIZEOF_OLDLHD,COpenRar::fp);
			OldLhd.PackSize=GetHeaderDword(0);
			OldLhd.UnpSize=GetHeaderDword(4);
			OldLhd.FileCRC=GetHeaderWord(8);
			OldLhd.HeadSize=GetHeaderWord(10);
			OldLhd.FileTime=GetHeaderDword(12);
			OldLhd.FileAttr=GetHeaderByte(16);
			OldLhd.Flags=GetHeaderByte(17);
			OldLhd.UnpVer=GetHeaderByte(18);
			OldLhd.NameSize=GetHeaderByte(19);
			OldLhd.Method=GetHeaderByte(20);
		}
		else
		{
			Size=fread(Header,1,SIZEOF_NEWLHD,COpenRar::fp);
			NewLhd.HeadCRC=GetHeaderWord(0);
			NewLhd.HeadType=GetHeaderByte(2);
			NewLhd.Flags=GetHeaderWord(3);
			NewLhd.HeadSize=GetHeaderWord(5);
			NewLhd.PackSize=GetHeaderDword(7);
			NewLhd.UnpSize=GetHeaderDword(11);
			NewLhd.HostOS=GetHeaderByte(15);
			NewLhd.FileCRC=GetHeaderDword(16);
			NewLhd.FileTime=GetHeaderDword(20);
			NewLhd.UnpVer=GetHeaderByte(24);
			NewLhd.Method=GetHeaderByte(25);
			NewLhd.NameSize=GetHeaderWord(26);
			NewLhd.FileAttr=GetHeaderDword(28);
			HeaderCRC=UpdateChecker(0xFFFFFFFFL,&Header[2],SIZEOF_NEWLHD-2,true);
		}
		break;
    case COMM_HEAD:
		Size=fread(Header,1,SIZEOF_COMMHEAD,COpenRar::fp);
		CommHead.HeadCRC=GetHeaderWord(0);
		CommHead.HeadType=GetHeaderByte(2);
		CommHead.Flags=GetHeaderWord(3);
		CommHead.HeadSize=GetHeaderWord(5);
		CommHead.UnpSize=GetHeaderWord(7);
		CommHead.UnpVer=GetHeaderByte(9);
		CommHead.Method=GetHeaderByte(10);
		CommHead.CommCRC=GetHeaderWord(11);
		HeaderCRC=UpdateChecker(0xFFFFFFFFL,&Header[2],SIZEOF_COMMHEAD-2,true);
		break;
    case PROTECT_HEAD:
		Size=fread(Header,1,SIZEOF_PROTECTHEAD,COpenRar::fp);
		ProtectHead.HeadCRC=GetHeaderWord(0);
		ProtectHead.HeadType=GetHeaderByte(2);
		ProtectHead.Flags=GetHeaderWord(3);
		ProtectHead.HeadSize=GetHeaderWord(5);
		ProtectHead.DataSize=GetHeaderDword(7);
		ProtectHead.Version=GetHeaderByte(11);
		ProtectHead.RecSectors=GetHeaderWord(12);
		ProtectHead.TotalBlocks=GetHeaderDword(14);
		memcpy(ProtectHead.Mark,&Header[18],8);
		HeaderCRC=UpdateChecker(0xFFFFFFFFL,&Header[2],SIZEOF_PROTECTHEAD-2,true);
		break;
    case ALL_HEAD:
		Size=fread(Header,1,SIZEOF_SHORTBLOCKHEAD,COpenRar::fp);
		BlockHead.HeadCRC=GetHeaderWord(0);
		BlockHead.HeadType=GetHeaderByte(2);
		BlockHead.Flags=GetHeaderWord(3);
		BlockHead.HeadSize=GetHeaderWord(5);
		if( BlockHead.Flags&LONG_BLOCK )
		{
			Size+=fread(&Header[7],1,4,COpenRar::fp);
			BlockHead.DataSize=GetHeaderDword(7);
		}
		break;
	}
	return Size;
}

int COpenRar::ReadBlock(int BlockType)
{
	NewFileHeader SaveFileHead;
	int Size,CurBlockPos=0;
	bool ReadSubBlock=(0!=(BlockType&READSUBBLOCK));
	static int LastBlock;
	memcpy(&SaveFileHead,&NewLhd,sizeof(SaveFileHead));
	BlockType &= 0xff;

	if( ArcFormat==OLD )
	{
		memset(&OldLhd,0,sizeof(OldLhd));
		CurBlockPos=ftell(COpenRar::fp);
		Size=ReadHeader(FILE_HEAD);
		if (Size != 0)
			if (OldLhd.Method > SIZEOF_SHORTBLOCKHEAD || OldLhd.NameSize==0 || OldLhd.NameSize > 80 ||
				OldLhd.UnpVer==0 || OldLhd.UnpVer > 20 || OldLhd.HeadSize <= 21 ||
				OldLhd.Flags<8 && OldLhd.HeadSize!=21+OldLhd.NameSize)
				return 0;
		NewLhd.HeadType=FILE_HEAD;
		NewLhd.HeadSize=OldLhd.HeadSize;
		NewLhd.Flags=OldLhd.Flags|LONG_BLOCK;
		NewLhd.PackSize=OldLhd.PackSize;
		NewLhd.UnpSize=OldLhd.UnpSize;
		NewLhd.FileTime=OldLhd.FileTime;
		NewLhd.UnpVer=(OldLhd.UnpVer==2) ? 13 : 10;
		NewLhd.Method=OldLhd.Method+0x30;
		NewLhd.NameSize=OldLhd.NameSize;
		NewLhd.FileAttr=OldLhd.FileAttr;
		NewLhd.FileCRC=OldLhd.FileCRC;
		if (Size!=0)
			NextBlockPos=CurBlockPos+OldLhd.HeadSize+OldLhd.PackSize;
	}
	else
	{
		while(true)
		{
			CurBlockPos=ftell(COpenRar::fp);
			Size=ReadHeader(FILE_HEAD);
			if(Size!=0)
			{
				if(NewLhd.HeadSize<SIZEOF_SHORTBLOCKHEAD)
					return 0;
				NextBlockPos=CurBlockPos+NewLhd.HeadSize;
				if( NewLhd.Flags&LONG_BLOCK )
					NextBlockPos+=NewLhd.PackSize;
				if( NextBlockPos<=CurBlockPos )
					return 0;
			}

			if( Size>0 && BlockType!=SUB_HEAD )
				LastBlock=BlockType;
			if( Size==0 || BlockType==ALL_HEAD || NewLhd.HeadType==BlockType ||
				(NewLhd.HeadType==SUB_HEAD && ReadSubBlock && LastBlock==BlockType) )
				break;
			fseek(COpenRar::fp,NextBlockPos,SEEK_SET);
		}
	}

	BlockHead.HeadCRC=NewLhd.HeadCRC;
	BlockHead.HeadType=NewLhd.HeadType;
	BlockHead.Flags=NewLhd.Flags;
	BlockHead.HeadSize=NewLhd.HeadSize;
	BlockHead.DataSize=NewLhd.PackSize;

	if( BlockType!=NewLhd.HeadType )
		BlockType=ALL_HEAD;

	if( BlockType==FILE_HEAD )
	{
		if( Size>0 )
		{
			NewLhd.NameSize=Min(NewLhd.NameSize,sizeof(ExtrName)-1);
			fread(ExtrName,1,NewLhd.NameSize,COpenRar::fp);
			ExtrName[NewLhd.NameSize]=0;
			if( ArcFormat==NEW && NewLhd.HeadCRC!=(WORD)~UpdateChecker(HeaderCRC,(BYTE*)ExtrName,NewLhd.NameSize,true) )
				return 0;
			Size+=NewLhd.NameSize;
		}
	}
	else
	{
		memcpy(&NewLhd,&SaveFileHead,sizeof(NewLhd));
		fseek(COpenRar::fp,CurBlockPos,SEEK_SET);
	}

	return Size;
}
DWORD COpenRar::UpdateChecker( DWORD StartVal,BYTE *ptr,DWORD Size,bool b32Bit )
{
	if( b32Bit )
		for(DWORD i=0;i!=Size;i++)
			StartVal = crcTable[(BYTE)StartVal^ptr[i]]^(StartVal>>8);
	else
		for(DWORD i=0;i!=Size;i++)
			StartVal+=ptr[i],StartVal=(WORD)rol(StartVal,1);
	return StartVal;
}


// --- RAR 5.0 listing via UnRAR API (added for UnRAR 7.x) ---
static int CALLBACK rar5_callback(UINT msg, LONG usr_dat, LONG p1, LONG p2)
{
	(void)usr_dat; (void)p1; (void)p2;
	if (msg == UCM_PROCESSDATA)
		return 0;
	return -1; /* refuse volumes/passwords: no interactive channel */
}

bool COpenRar::FindSub5(INDIVIDUALINFO *lpSubInfo)
{
	if (rar5_h == NULL) {
		RAROpenArchiveDataEx arc_data;
		memset(&arc_data, 0, sizeof(arc_data));
		arc_data.ArcName = szArcFileName; /* 7.x: ArcName is char*, not an array */
		arc_data.OpenMode = RAR_OM_LIST;
		rar5_h = RAROpenArchiveEx(&arc_data);
		if (rar5_h == NULL)
			return false;
		RARSetCallback(rar5_h, rar5_callback, (LPARAM)NULL);
	}
	for (;;) {
		RARHeaderDataEx hdr;
		memset(&hdr, 0, sizeof(hdr));
		if (0 != RARReadHeaderEx(rar5_h, &hdr)) {
			RARCloseArchive(rar5_h);
			rar5_h = NULL;
			return false;
		}
		if (hdr.Flags & RHDF_DIRECTORY)
			continue;
		lstrcpyn(lpSubInfo->szFileName, hdr.FileName, sizeof(lpSubInfo->szFileName));
		lpSubInfo->dwCompressedSize = hdr.PackSize;
		lpSubInfo->dwOriginalSize = hdr.UnpSize;
		lpSubInfo->dwCRC = hdr.FileCRC;
		lstrcpy(lpSubInfo->szAttribute, "----W");
		if (hdr.FileAttr & FILE_ATTRIBUTE_READONLY)
			lpSubInfo->szAttribute[4] = 'R';
		if (hdr.FileAttr & FILE_ATTRIBUTE_HIDDEN)
			lpSubInfo->szAttribute[2] = 'H';
		if (hdr.FileAttr & FILE_ATTRIBUTE_SYSTEM)
			lpSubInfo->szAttribute[3] = 'S';
		if (hdr.FileAttr & FILE_ATTRIBUTE_DIRECTORY)
			lpSubInfo->szAttribute[1] = 'D';
		if (hdr.FileAttr & FILE_ATTRIBUTE_ARCHIVE)
			lpSubInfo->szAttribute[0] = 'A';
		{ int m = (int)hdr.Method; if (m >= '0') m -= '0'; /* RAR5 reports 0x30-based like new-style RAR */
		  if (m >= 0 && m <= 5) wsprintf(lpSubInfo->szMode, "m%d", m);
		  else lstrcpy(lpSubInfo->szMode, "?"); }
		lpSubInfo->uOSType = hdr.HostOS;
		double tmp1 = (double)lpSubInfo->dwOriginalSize;
		double tmp2 = (double)lpSubInfo->dwCompressedSize;
		if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0) {
			lpSubInfo->wRatio = (WORD)((tmp2 * 1000 / tmp1));
		}
		lpSubInfo->wDate = (WORD)(hdr.FileTime >> 16);
		lpSubInfo->wTime = (WORD)hdr.FileTime;
		return true;
	}
}
