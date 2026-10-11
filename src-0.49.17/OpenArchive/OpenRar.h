#ifndef AFX_OPENRAR_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENRAR_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenRar : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenRar() : COpenArchive( )
	{
		COpenRar::fp=NULL;

		DWORD c,i,j;
		for(i=0;i!=256;i++)
		{
			for(c=i,j=0;j!=8;j++)
				c=(c&1)?(c>>1)^0xEDB88320L:(c>>1);
			crcTable[i]=c;
		}
	}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	#define  Min(x,y) (((x)<(y)) ? (x):(y))
	#define  Max(x,y) (((x)>(y)) ? (x):(y))

	#define MAXWINSIZE      0x100000
	#define MAXWINMASK      (MAXWINSIZE-1)

	#define  SIZEOF_MARKHEAD         7
	#define  SIZEOF_OLDMHD           7
	#define  SIZEOF_NEWMHD          13
	#define  SIZEOF_OLDLHD          21
	#define  SIZEOF_NEWLHD          32
	#define  SIZEOF_SHORTBLOCKHEAD   7
	#define  SIZEOF_LONGBLOCKHEAD   11
	#define  SIZEOF_COMMHEAD        13
	#define  SIZEOF_PROTECTHEAD     26

	#define  PACK_VER       20
	#define  UNP_VER        20
	#define  PROTECT_VER    20

	enum { M_DENYREAD,M_DENYWRITE,M_DENYNONE,M_DENYALL };
	enum { FILE_EMPTY,FILE_ADD,FILE_UPDATE,FILE_COPYOLD,FILE_COPYBLOCK };
	enum { ARG_RESET,ARG_NEXT,ARG_REMOVE };
	enum { ARC=1,VOL,SFX };
	enum { SUCCESS,WARNING,FATAL_ERROR,CRC_ERROR,LOCK_ERROR,WRITE_ERROR,
		   OPEN_ERROR,USER_ERROR,MEMORY_ERROR,USER_BREAK=255,IMM_ABORT=0x8000 };
	enum { EN_LOCK=1,EN_VOL=2 };
	enum { SD_MEMORY=1,SD_FILES=2 };
	enum { ASK_OVERWR=0,ALL_OVERWR=1,NO_OVERWR=2 };
	enum { ALARM_SOUND,ERROR_SOUND };
	enum { MSG_STDOUT,MSG_STDERR,MSG_NULL };
	enum { COMPARE_PATH,NOT_COMPARE_PATH };
	enum { NAMES_DONTCHANGE,NAMES_UPPERCASE,NAMES_LOWERCASE };
	enum { LOG_ARC=1,LOG_FILE=2 };
	enum { CRC16=1,CRC32=2 };
	enum { OLD_DECODE=0,OLD_ENCODE=1,NEW_CRYPT=2 };
	enum { OLD_UNPACK,NEW_UNPACK };
	enum { KEEP_TIME=1,LATEST_TIME };

	#define  MHD_MULT_VOL       1
	#define  MHD_COMMENT        2
	#define  MHD_LOCK           4
	#define  MHD_SOLID          8
	#define  MHD_PACK_COMMENT   16
	#define  MHD_AV             32
	#define  MHD_PROTECT        64

	#define  LHD_SPLIT_BEFORE   1
	#define  LHD_SPLIT_AFTER    2
	#define  LHD_PASSWORD       4
	#define  LHD_COMMENT        8
	#define  LHD_SOLID          16

	#define  LHD_WINDOWMASK     0x00e0
	#define  LHD_WINDOW64       0
	#define  LHD_WINDOW128      32
	#define  LHD_WINDOW256      64
	#define  LHD_WINDOW512      96
	#define  LHD_WINDOW1024     128
	#define  LHD_DIRECTORY      0x00e0

	#define  SKIP_IF_UNKNOWN    0x4000
	#define  LONG_BLOCK         0x8000

	#define READSUBBLOCK	0x8000

	enum { ALL_HEAD=0,MARK_HEAD=0x72,MAIN_HEAD=0x73,FILE_HEAD=0x74,
		   COMM_HEAD=0x75,AV_HEAD=0x76,SUB_HEAD=0x77,PROTECT_HEAD=0x78};
	enum { EA_HEAD=0x100 };
	enum { MS_DOS=0,OS2=1,WIN_32=2,UNIX=3 };
	enum { OLD=1,NEW=2};
	enum { EEMPTY=-1,EBREAK=1,EWRITE,EREAD,EOPEN,ECREAT,ECLOSE,ESEEK,
		   EMEMORY,EARCH };

	#define  rol(x,n)  (((x)<<(n)) | ((x)>>(8*sizeof(x)-(n))))
	#define  ror(x,n)  (((x)>>(n)) | ((x)<<(8*sizeof(x)-(n))))

	struct MarkHeader
	{
		BYTE Mark[7];
	};
	struct OldMainHeader
	{
		BYTE Mark[4];
		WORD HeadSize;
		BYTE Flags;
	};
	struct NewMainArchiveHeader
	{
		WORD HeadCRC;
		BYTE HeadType;
		WORD Flags;
		WORD HeadSize;
		WORD Reserved;
		DWORD Reserved1;
	};
	struct OldFileHeader
	{
		DWORD PackSize;
		DWORD UnpSize;
		WORD FileCRC;
		WORD HeadSize;
		DWORD FileTime;
		BYTE FileAttr;
		BYTE Flags;
		BYTE UnpVer;
		BYTE NameSize;
		BYTE Method;
	};
	struct NewFileHeader
	{
		WORD HeadCRC;
		BYTE HeadType;
		WORD Flags;
		WORD HeadSize;
		DWORD PackSize;
		DWORD UnpSize;
		BYTE HostOS;
		DWORD FileCRC;
		DWORD FileTime;
		BYTE UnpVer;
		BYTE Method;
		WORD NameSize;
		DWORD FileAttr;
	};
	struct BlockHeader
	{
		WORD HeadCRC;
		BYTE HeadType;
		WORD Flags;
		WORD HeadSize;
		DWORD DataSize;
	};
	struct SubBlockHeader
	{
		WORD HeadCRC;
		BYTE HeadType;
		WORD Flags;
		WORD HeadSize;
		DWORD DataSize;
		WORD SubType;
		BYTE Level;
	};
	struct CommentHeader
	{
		WORD HeadCRC;
		BYTE HeadType;
		WORD Flags;
		WORD HeadSize;
		WORD UnpSize;
		BYTE UnpVer;
		BYTE Method;
		WORD CommCRC;
	};
	struct ProtectHeader
	{
		WORD HeadCRC;
		BYTE HeadType;
		WORD Flags;
		WORD HeadSize;
		DWORD DataSize;
		BYTE Version;
		WORD RecSectors;
		DWORD TotalBlocks;
		BYTE Mark[8];
	};

	int ReadHeader(int BlockType);
	int ReadBlock(int BlockType);
	bool GetNextVolume();
	DWORD UpdateChecker( DWORD StartVal,BYTE *ptr,DWORD Size,bool b32Bit );
	bool FindSub(INDIVIDUALINFO *lpSubInfo);

	DWORD crcTable[256];

	MarkHeader MarkHead;
	OldMainHeader OldMhd;
	NewMainArchiveHeader NewMhd;
	OldFileHeader OldLhd;
	NewFileHeader NewLhd;
	BlockHeader BlockHead;
	CommentHeader CommHead;
	ProtectHeader ProtectHead;

	int ArcFormat;
	DWORD HeaderCRC;
	char ExtrName[MAX_PATH*2];
	int NextBlockPos,MainHeadSize;
	int UnpPackedSize;
	DWORD PackedCRC;

	char ArcName[MAX_PATH*2];

	bool bSolid,bNeedNextVol;
};

#endif