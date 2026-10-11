#ifndef AFX_RARTOOL_H__2AE8B602_4430_11D4_8D96_9277B4F74C35__INCLUDED_
#define AFX_RARTOOL_H__2AE8B602_4430_11D4_8D96_9277B4F74C35__INCLUDED_

#include "Tool.h"

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

#define NC 298
#define DC 48
#define RC 28
#define BC 19
#define MC 257

struct LitDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[NC];
};

struct DistDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[DC];
};

struct RepDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[RC];
};

struct MultDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[MC];
};

struct BitDecode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[BC];
};

struct Decode
{
  unsigned int MaxNum;
  unsigned int DecodeLen[16];
  unsigned int DecodePos[16];
  unsigned int DecodeNum[2];
};

struct AudioVariables
{
  int K1,K2,K3,K4,K5;
  int D1,D2,D3,D4;
  int LastDelta;
  unsigned int Dif[11];
  unsigned int ByteCount;
  int LastChar;
};

class CRarTool : public CTool
{
//
// original source is
//
// Unrar
//   http://www.rarsoft.com
// by Eugene Roshal
//
public:
	CRarTool() : CTool( "Rar “WŠJ" )
		{
			InitCRC();
			MDPtr[0]=&MD[0],MDPtr[1]=&MD[1],MDPtr[2]=&MD[2],MDPtr[3]=&MD[3];
		}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"rar");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	bool IsArchive(bool bStrict);
	int ReadHeader(int BlockType);
	int ReadBlock(int BlockType);
	bool GetNextVolume();
	DWORD UnpRead(BYTE *Addr,DWORD Count);
	DWORD UnpWrite(BYTE *Addr,DWORD Count);
	void Unstore();

	// Unpacker(OLD) ////////////////////
	void OldUnpack(bool bSolid);
	void ShortLZ();
	void LongLZ();
	void HuffDecode();
	void GetFlagsBuf();
	void OldUnpInitData(bool bSolid);
	void InitHuff(void);
	void CorrHuff(unsigned int *CharSet,unsigned int *NumToPlace);
	void OldCopyString(unsigned int Distance,unsigned int Length);
	unsigned int DecodeNum(int Num,unsigned int StartPos,unsigned int *DecTab,unsigned int *PosTab);

	unsigned int FlagBuf;
	int LCount;
	int FlagsCnt;
	unsigned int ChSet[256];
	unsigned int Place[256];
	unsigned int NToPl[256];
	unsigned int ChSetA[256];
	unsigned int PlaceA[256];
	unsigned int ChSetB[256];
	unsigned int PlaceB[256];
	unsigned int NToPlB[256];
	unsigned int ChSetC[256];
	unsigned int PlaceC[256];
	unsigned int NToPlC[256];
	unsigned int AvrPlc;
	unsigned int AvrPlcB;
	unsigned int AvrLn1;
	unsigned int AvrLn2;
	unsigned int AvrLn3;
	int NumHuf;
	int StMode;
	unsigned int Nhfb,Nlzb;
	unsigned int MaxDist3;
	int Buf60;

	// Unpacker(NEW) ////////////////////
	void Unpack(bool bSolid);
	void UnpReadBuf(int FirstBuf);
	void UnpWriteBuf();
	void ReadTables();
	void ReadLastTables();
	void MakeDecodeTables(unsigned char *LenTab,struct Decode *Dec,int Size);
	void DecodeNumber(struct Decode *Dec);
	void CopyString();
	void UnpInitData(bool bSolid);
	BYTE DecodeAudio(int Delta);

	LitDecode  LD;
	DistDecode DD;
	RepDecode  RD;
	MultDecode MD[4];
	MultDecode *MDPtr[4];
	BitDecode  BD;
	AudioVariables AudV[4];

	unsigned int UnpPtr,WrPtr;
	unsigned int BitField;
	unsigned int Number;
	unsigned char InBuf[8192];
	unsigned char UnpOldTable[MC*4];
	unsigned int InAddr,InBit,ReadTop;
	unsigned int LastDist,LastLength;
	unsigned int Length,Distance;
	unsigned int OldDist[4],OldDistPtr;
	int UnpAudioBlock,UnpChannels,CurChannel,ChannelDelta;

	// Globals //////////////////////////
	FILE *arc,*out;
	char ArcName[MAX_PATH*2];
	BYTE* UnpMemory;

	MarkHeader MarkHead;
	OldMainHeader OldMhd;
	NewMainArchiveHeader NewMhd;
	OldFileHeader OldLhd;
	NewFileHeader NewLhd;
	BlockHeader BlockHead;
	CommentHeader CommHead;
	ProtectHeader ProtectHead;

	int ArcFormat;
	int Encryption;
	bool bSolid,bNeedNextVol;
	DWORD HeaderCRC;
	char ExtrName[MAX_PATH*2];
	int NextBlockPos,MainHeadSize;
	int UnpPackedSize,DestUnpSize;
	DWORD CurUnpRead,CurUnpWrite;

	// CRC ///////////////////////////////
	DWORD UnpFileCRC,PackedCRC;//,PackFileCRC
	DWORD crcTable[256];
	DWORD Key[4];
	WORD OldKey[4];
	BYTE PN1,PN2,PN3;
	BYTE SubstTable[256];
	void InitCRC();
	DWORD UpdateChecker( DWORD StartVal,BYTE *ptr,DWORD Size,bool b32Bit );
	void EncryptBlock(BYTE *Buf);
	void DecryptBlock(BYTE *Buf);
	void Encode13(BYTE *Data,DWORD Count);
	void Crypt(BYTE *Data,DWORD Count,int Method);
	void Decode13(BYTE *Data,DWORD Count);
	void Crypt15(BYTE *Data,DWORD Count);
	void UpdKeys(BYTE *Buf);
	void Swap(BYTE *Ch1,BYTE *Ch2);
	void SetCryptKeys(char *Password);
	void SetOldKeys(char *Password);
};

#endif
