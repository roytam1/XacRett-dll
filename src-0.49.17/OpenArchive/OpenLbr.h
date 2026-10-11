#ifndef AFX_OPENLBR_H__7C2A9D41_3E5F_4B8A_9C1D_5E6F708192A3__INCLUDED_
#define AFX_OPENLBR_H__7C2A9D41_3E5F_4B8A_9C1D_5E6F708192A3__INCLUDED_


#include "OpenArchive.h"

// CP/M LBR (Gary Novosielski) 32-byte directory entry.
// All multi-byte integers are little-endian. Sector = 128 bytes.
struct LbrHead
{
	unsigned char status;// 0x00 active, 0xFE deleted, 0xFF unused
	char name[9];        // 8 chars, space padded
	char ext[4];         // 3 chars, space padded
	unsigned short index;// first sector of member
	unsigned short length;// member size in sectors
	unsigned short crc;  // CCITT/XMODEM over member bytes
	unsigned short cdate; // creation date, CP/M days (0 if unused)
	unsigned short ctime; // creation time, DOS format
	unsigned short mdate; // last-change date, CP/M days
	unsigned short mtime; // last-change time, DOS format
	unsigned char pad;   // pad bytes (0x1A) in last sector
};

class COpenLbr : public COpenArchive
{
public: //-- COpenArchive interface -----------

	COpenLbr() : COpenArchive( )
	{COpenLbr::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
	bool ReadEntry(int entryNo, LbrHead& hdr);
	int m_entryNo;
	int m_dirEntries;
	long m_fileSize;
};

#endif
