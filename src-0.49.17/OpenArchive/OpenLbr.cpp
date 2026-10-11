#include "stdafx.h"
#include "OpenLbr.h"

#define LBR_SECTOR 128
#define LBR_ENTRY 32

// CP/M datestamp (days since 31 Dec 1977) to DOS date word. 0 on failure.
static WORD CpmDateToDosDate(unsigned short cpmdate)
{
	long days, z, era, y;
	unsigned doe, yoe, doy, mp, d, m;
	if(cpmdate==0)
		return 0;
	days = (long)cpmdate - 1;// days since 1 Jan 1978
	z = days + 2922;// days since 1 Jan 1970 (leaps 1972, 1976)
	z += 719468;// shift to civil epoch
	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = (unsigned)(z - era * 146097);
	yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
	y = (long)yoe + era * 400;
	doy = doe - (365*yoe + yoe/4 - yoe/100);
	mp = (5*doy + 2)/153;
	d = doy - (153*mp+2)/5 + 1;
	m = mp + (mp < 10 ? 3 : -9);
	y += (m <= 2);
	if(y<1980 || y>2107)
		return 0;
	return (WORD)(((y-1980)<<9)|(m<<5)|d);
}

bool COpenLbr::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	BYTE hdr[LBR_ENTRY];

	COpenLbr::m_entryNo = 1;// entry 0 describes the directory itself
	COpenLbr::m_dirEntries = 0;
	fseek(COpenLbr::fp,0,SEEK_END);
	COpenLbr::m_fileSize = ftell(COpenLbr::fp);
	fseek(COpenLbr::fp,0,SEEK_SET);
	if( LBR_ENTRY!=fread(hdr,1,LBR_ENTRY,COpenLbr::fp) )
		return false;
	// directory control entry (fatal rule): active, blank name,
	// index 0, non-zero length
	{
		int i;
		if(hdr[0]!=0)
			return false;
		for(i=1;i<=11;i++)
			if(hdr[i]!=0x20)
				return false;
		if(hdr[12]!=0 || hdr[13]!=0)
			return false;
	}
	{
		unsigned dirSectors = (hdr[14])|(hdr[15]<<8);
		if(dirSectors==0 || (long)dirSectors*LBR_SECTOR>COpenLbr::m_fileSize)
			return false;
		COpenLbr::m_dirEntries = dirSectors*4;
	}
	return FindSub(lpSubInfo);
}

bool COpenLbr::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}

bool COpenLbr::ReadEntry(int entryNo, LbrHead& hdr)
{
	BYTE rec[LBR_ENTRY];
	int i;

	fseek(COpenLbr::fp,(long)entryNo*LBR_ENTRY,SEEK_SET);
	if( LBR_ENTRY!=fread(rec,1,LBR_ENTRY,COpenLbr::fp) )
		return false;
	hdr.status = rec[0];
	for(i=0;i<8 && rec[1+i]!=0x20;i++)
		hdr.name[i] = (char)rec[1+i];
	hdr.name[i] = '\0';
	for(i=0;i<3 && rec[9+i]!=0x20;i++)
		hdr.ext[i] = (char)rec[9+i];
	hdr.ext[i] = '\0';
	hdr.index = (rec[12])|(rec[13]<<8);
	hdr.length = (rec[14])|(rec[15]<<8);
	hdr.crc = (rec[16])|(rec[17]<<8);
	hdr.cdate = (rec[18])|(rec[19]<<8);
	hdr.ctime = (rec[20])|(rec[21]<<8);
	hdr.mdate = (rec[22])|(rec[23]<<8);
	hdr.mtime = (rec[24])|(rec[25]<<8);
	hdr.pad = rec[26];
	return true;
}

bool COpenLbr::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	LbrHead hdr;
	BYTE magic[2];
	bool found = false;

	while(COpenLbr::m_entryNo<COpenLbr::m_dirEntries)
	{
		if(!ReadEntry(COpenLbr::m_entryNo,hdr))
			return false;
		if(hdr.status==0xFF)
			return false;// unused terminates the directory
		COpenLbr::m_entryNo++;
		if(hdr.status!=0)
			continue;// deleted (any other value per the spec)
		found = true;
		break;
	}
	if(!found)
		return false;

	{
		char outname[MAX_PATH];
		if(hdr.ext[0])
			wsprintf(outname,"%s.%s",hdr.name,hdr.ext);
		else
			lstrcpy(outname,hdr.name);
		lstrcpy(lpSubInfo->szFileName,outname);
	}

	{
		unsigned long datasize = (unsigned long)hdr.length*LBR_SECTOR;
		if(hdr.pad>127)
			return false;
		if(datasize<(unsigned long)hdr.pad)
			return false;
		datasize -= hdr.pad;
		if((long)hdr.index*LBR_SECTOR+(long)datasize>COpenLbr::m_fileSize)
			return false;
		// member kind: 2nd extension char flags squeeze ('q') or
		// crunch ('z'), confirmed by the member magic
		lstrcpy(lpSubInfo->szMode,"stored");
		lpSubInfo->dwCompressedSize = datasize;
		lpSubInfo->dwOriginalSize = datasize;
		fseek(COpenLbr::fp,(long)hdr.index*LBR_SECTOR,SEEK_SET);
		if( datasize>0 && 2==fread(magic,1,2,COpenLbr::fp) )
		{
			if((hdr.ext[1]=='q' || hdr.ext[1]=='Q')
				&& magic[0]==0x76 && magic[1]==0xFF)
			{
				lstrcpy(lpSubInfo->szMode,"squeezd");
				lpSubInfo->dwOriginalSize = 0;// known only after decode
			}
			else if((hdr.ext[1]=='z' || hdr.ext[1]=='Z')
				&& magic[0]==0x76 && (magic[1]==0xFE || magic[1]==0xFD))
			{
				lstrcpy(lpSubInfo->szMode,"crunch");
				lpSubInfo->dwOriginalSize = 0;// known only after decode
			}
		}
	}

	lpSubInfo->dwCRC = hdr.crc;
	lstrcpy(lpSubInfo->szAttribute ,"----W");
	lpSubInfo->uOSType = 15;// CP/M (same value LZH uses for 'C')
	lpSubInfo->wDate = CpmDateToDosDate(hdr.mdate);
	lpSubInfo->wTime = (hdr.mdate==0) ? 0 : hdr.mtime;

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	return true;
}
