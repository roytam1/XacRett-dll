
#include "stdafx.h"
#include "RarTool.h"

bool CRarTool::Check( const char* fname, unsigned long fsize )
{
	if( (arc=fopen(fname,"rb"))==NULL )
		return false;
	bool ans=IsArchive(true);
	fclose(arc);
	return ans;
}

bool CRarTool::Extract( const char* fname,const char* ddir )
{
	// 変数初期化
	bool bFirstVol=true,bFirstFile=true;
	char *DestName;
	bNeedNextVol=false;
	char PassWord[128]={0};
	strcpy(ArcName,fname);

	// 開く
	arc=fopen(fname,"rb");
	if( arc==NULL )return false;
	if( !IsArchive(false) ){fclose(arc);return false;}
	fseek(arc,NewMhd.HeadSize-MainHeadSize,SEEK_CUR);
	
	while( true )
    {
		int Size=ReadBlock(FILE_HEAD|READSUBBLOCK);
		if( Size<=0 && !bNeedNextVol )		break;
		if( BlockHead.HeadType==SUB_HEAD )
		{fseek(arc,NextBlockPos,SEEK_SET);continue;}

		// マルチボリューム処理
		if( (NewLhd.Flags&LHD_SPLIT_BEFORE) && bSolid && bFirstVol )break;
		bFirstVol=false;
		if( bNeedNextVol && Size==0 && !GetNextVolume() )break;
		bNeedNextVol=(0!=(NewLhd.Flags&LHD_SPLIT_AFTER));

		// 圧縮データ位置へ飛ぶ
		fseek(arc,NextBlockPos-NewLhd.PackSize,SEEK_SET);
		
		if( !(NewLhd.Flags&LHD_SPLIT_BEFORE) )
		{
			// パスワード入力
			if( NewLhd.Flags&LHD_PASSWORD )
				if( *PassWord==0 )
					strncpy(PassWord,GetPassword(),sizeof(PassWord));

			// 格納されてるのがファイルだったらなんか処理。
			if( NewLhd.FileAttr&0x08 )	continue;
			if( NewLhd.HostOS==UNIX ){NewLhd.FileAttr=(NewLhd.FileAttr&0x40)?0x10:0x20;}

			DestName = kiutil::pathMake( ExtrName );
			if( NewLhd.FileAttr&0x10 )	continue;

			// 展開
			if (m_Endflg) return true;//追加 By Bonty
			//ワイルドカード処理
			if (!CheckStr(ExtrName))//追加 By Bonty
			{
				out=NULL;
			}else
				out=fopen( DestName,"wb" );
			//if( out!=NULL )
			{
				CurUnpRead=CurUnpWrite=0;
				UnpFileCRC=(ArcFormat==OLD)?0:0xFFFFFFFFL;
				PackedCRC=0xFFFFFFFFL;

				if( 0!=(Encryption=((*PassWord!=0)&&(NewLhd.Flags&LHD_PASSWORD))?NewLhd.UnpVer:0) )
					SetCryptKeys(PassWord);

				UnpPackedSize=NewLhd.PackSize;
				DestUnpSize=NewLhd.UnpSize;

				if( NewLhd.Method==0x30 )
					Unstore();
				else if( NewLhd.UnpVer<13 || NewLhd.UnpVer>UNP_VER );
				else if( NewLhd.UnpVer<=15 )
					OldUnpack(!bFirstFile&&bSolid);
				else
				    Unpack(0!=(NewLhd.Flags&LHD_SOLID));

				fclose(out);

				if( !(ArcFormat==OLD && UnpFileCRC==NewLhd.FileCRC) && !(ArcFormat==NEW && UnpFileCRC==~NewLhd.FileCRC) )
				{
					if( NewLhd.Flags&LHD_PASSWORD )	// Bad CRC
						DeleteFile(DestName);
				}
				else
				{
					kiutil::timeSet(DestName,(WORD)(NewLhd.FileTime>>16),(WORD)NewLhd.FileTime);
					SetFileAttributes(DestName,NewLhd.FileAttr);
				}
			}
		}

		if(arc==NULL)break;
		bFirstFile=false;
		fseek(arc,NextBlockPos,SEEK_SET);
    }

	fclose(arc);
	return true;
}

/**** Unpackers(Old) *****************************************************/
#define GetBits()                                                 \
        BitField = ( ( ( (DWORD)InBuf[InAddr]   << 16 ) |        \
                       ( (WORD) InBuf[InAddr+1] <<  8 ) |        \
                       (         InBuf[InAddr+2]       ) )        \
                       >> (8-InBit) ) & 0xffff;
#define AddBits(Bits)                          \
        InAddr += ( InBit + (Bits) ) >> 3;     \
        InBit  =  ( InBit + (Bits) ) &  7;

static unsigned int ShortLen1[]={1,3,4,4,5,6,7,8,8,4,4,5,6,6,4,0};
static unsigned int ShortXor1[]={0,0xa0,0xd0,0xe0,0xf0,0xf8,0xfc,0xfe,
                                 0xff,0xc0,0x80,0x90,0x98,0x9c,0xb0};
static unsigned int ShortLen2[]={2,3,3,3,4,4,5,6,6,4,4,5,6,6,4,0};
static unsigned int ShortXor2[]={0,0x40,0x60,0xa0,0xd0,0xe0,0xf0,0xf8,
                                 0xfc,0xc0,0x80,0x90,0x98,0x9c,0xb0};
#define STARTL1  2
static unsigned int DecL1[]={0x8000,0xa000,0xc000,0xd000,0xe000,0xea00,
                             0xee00,0xf000,0xf200,0xf200,0xffff};
static unsigned int PosL1[]={0,0,0,2,3,5,7,11,16,20,24,32,32};
#define STARTL2  3
static unsigned int DecL2[]={0xa000,0xc000,0xd000,0xe000,0xea00,0xee00,
                             0xf000,0xf200,0xf240,0xffff};
static unsigned int PosL2[]={0,0,0,0,5,7,9,13,18,22,26,34,36};
#define STARTHF0  4
static unsigned int DecHf0[]={0x8000,0xc000,0xe000,0xf200,0xf200,0xf200,
                              0xf200,0xf200,0xffff};
static unsigned int PosHf0[]={0,0,0,0,0,8,16,24,33,33,33,33,33};
#define STARTHF1  5
static unsigned int DecHf1[]={0x2000,0xc000,0xe000,0xf000,0xf200,0xf200,
                              0xf7e0,0xffff};
static unsigned int PosHf1[]={0,0,0,0,0,0,4,44,60,76,80,80,127};
#define STARTHF2  5
static unsigned int DecHf2[]={0x1000,0x2400,0x8000,0xc000,0xfa00,0xffff,
                              0xffff,0xffff};
static unsigned int PosHf2[]={0,0,0,0,0,0,2,7,53,117,233,0,0};
#define STARTHF3  6
static unsigned int DecHf3[]={0x800,0x2400,0xee00,0xfe80,0xffff,0xffff,
                              0xffff};
static unsigned int PosHf3[]={0,0,0,0,0,0,0,2,16,218,251,0,0};
#define STARTHF4  8
static unsigned int DecHf4[]={0xff00,0xffff,0xffff,0xffff,0xffff,0xffff};
static unsigned int PosHf4[]={0,0,0,0,0,0,0,0,0,255,0,0,0};


void CRarTool::OldUnpack(bool bSolid)
{
  int SetStMode=0;
  UnpInitData(bSolid);
  OldUnpInitData(bSolid);
  UnpReadBuf(1);
  if (!bSolid)
  {
    InitHuff();
    UnpPtr=0;
  }
  else
    UnpPtr=WrPtr;
  DestUnpSize--;

  if (SetStMode)
    StMode=1;
  else
    if (DestUnpSize>=0)
    {
      GetFlagsBuf();
      FlagsCnt=8;
    }

  while (DestUnpSize>=0)
  {
    UnpPtr&=MAXWINMASK;

    if (InAddr>sizeof(InBuf)-30)
      UnpReadBuf(0);
    if (((WrPtr-UnpPtr) & MAXWINMASK)<270 && WrPtr!=UnpPtr)
    {
      UnpWriteBuf();
    }
    if (StMode)
    {
      HuffDecode();
      continue;
    }

    if (--FlagsCnt < 0)
    {
      GetFlagsBuf();
      FlagsCnt=7;
    }

    if (FlagBuf & 0x80)
    {
      FlagBuf<<=1;
      if (Nlzb > Nhfb)
        LongLZ();
      else
        HuffDecode();
    }
    else
    {
      FlagBuf<<=1;
      if (--FlagsCnt < 0)
      {
        GetFlagsBuf();
        FlagsCnt=7;
      }
      if (FlagBuf & 0x80)
      {
        FlagBuf<<=1;
        if (Nlzb > Nhfb)
          HuffDecode();
        else
          LongLZ();
      }
      else
      {
        FlagBuf<<=1;
        ShortLZ();
      }
    }
  }
  UnpWriteBuf();
}


void CRarTool::ShortLZ()
{
  unsigned int Length,SaveLength;
  unsigned int LastDistance;
  unsigned int Distance;
  int DistancePlace;
  NumHuf=0;
  GetBits();
  if (LCount==2)
  {
    AddBits(1);
    if (BitField >= 0x8000)
    {
      OldCopyString((unsigned int)LastDist,LastLength);
      return;
    }
    BitField <<= 1;
    LCount=0;
  }

  BitField>>=8;

  ShortLen1[1]=ShortLen2[3]=Buf60+3;

  if (AvrLn1<37)
  {
    for (Length=0;;Length++)
      if (((BitField^ShortXor1[Length]) & (~(0xff>>ShortLen1[Length])))==0)
        break;
    AddBits(ShortLen1[Length]);
  }
  else
  {
    for (Length=0;;Length++)
      if (((BitField^ShortXor2[Length]) & (~(0xff>>ShortLen2[Length])))==0)
        break;
    AddBits(ShortLen2[Length]);
  }

  if (Length >= 9)
  {
    if (Length == 9)
    {
      LCount++;
      OldCopyString((unsigned int)LastDist,LastLength);
      return;
    }
    if (Length == 14)
    {
      LCount=0;
      GetBits();
      Length=DecodeNum(BitField,STARTL2,DecL2,PosL2)+5;
      GetBits();
      Distance=(BitField>>1) | 0x8000;
      AddBits(15);
      LastLength=Length;
      LastDist=Distance;
      OldCopyString(Distance,Length);
      return;
    }

    LCount=0;
    SaveLength=Length;
    Distance=OldDist[(OldDistPtr-(Length-9)) & 3];
    GetBits();
    Length=DecodeNum(BitField,STARTL1,DecL1,PosL1)+2;
    if (Length==0x101 && SaveLength==10)
    {
      Buf60 ^= 1;
      return;
    }
    if (Distance > 256)
      Length++;
    if (Distance >= MaxDist3)
      Length++;

    OldDist[OldDistPtr++]=Distance;
    OldDistPtr = OldDistPtr & 3;
    LastLength=Length;
    LastDist=Distance;
    OldCopyString(Distance,Length);
    return;
  }

  LCount=0;
  AvrLn1 += Length;
  AvrLn1 -= AvrLn1 >> 4;

  GetBits();
  DistancePlace=DecodeNum(BitField,STARTHF2,DecHf2,PosHf2);
  Distance=ChSetA[DistancePlace];
  if (--DistancePlace != -1)
  {
    PlaceA[Distance]--;
    LastDistance=ChSetA[DistancePlace];
    PlaceA[LastDistance]++;
    ChSetA[DistancePlace+1]=LastDistance;
    ChSetA[DistancePlace]=Distance;
  }
  Length+=2;
  OldDist[OldDistPtr++] = ++Distance;
  OldDistPtr = OldDistPtr & 3;
  LastLength=Length;
  LastDist=Distance;
  OldCopyString(Distance,Length);
  return;
}


void CRarTool::LongLZ()
{
  unsigned int Length;
  unsigned int Distance;
  unsigned int DistancePlace,NewDistancePlace;
  unsigned int OldAvr2,OldAvr3;

  NumHuf=0;
  Nlzb+=16;
  if (Nlzb > 0xff)
  {
    Nlzb=0x90;
    Nhfb >>= 1;
  }
  OldAvr2=AvrLn2;

  GetBits();
  if (AvrLn2 >= 122)
    Length=DecodeNum(BitField,STARTL2,DecL2,PosL2);
  else
    if (AvrLn2 >= 64)
      Length=DecodeNum(BitField,STARTL1,DecL1,PosL1);
    else
      if (BitField < 0x100)
      {
        Length=BitField;
        AddBits(16);
      }
      else
      {
        for (Length=0;((BitField<<Length)&0x8000)==0;Length++)
          ;
        AddBits(Length+1);
      }

  AvrLn2 += Length;
  AvrLn2 -= AvrLn2 >> 5;

  GetBits();
  if (AvrPlcB > 0x28ff)
    DistancePlace=DecodeNum(BitField,STARTHF2,DecHf2,PosHf2);
  else
    if (AvrPlcB > 0x6ff)
      DistancePlace=DecodeNum(BitField,STARTHF1,DecHf1,PosHf1);
    else
      DistancePlace=DecodeNum(BitField,STARTHF0,DecHf0,PosHf0);

  AvrPlcB += DistancePlace;
  AvrPlcB -= AvrPlcB >> 8;
  while (1)
  {
    Distance = ChSetB[DistancePlace];
    NewDistancePlace = NToPlB[Distance++ & 0xff]++;
    if (!(Distance & 0xff))
      CorrHuff(ChSetB,NToPlB);
    else
      break;
  }

  ChSetB[DistancePlace]=ChSetB[NewDistancePlace];
  ChSetB[NewDistancePlace]=Distance;

  GetBits();
  Distance=((WORD)((Distance & 0xff00) | (BitField >> 8))) >> 1;
  AddBits(7);

  OldAvr3=AvrLn3;
  if (Length!=1 && Length!=4)
    if (Length==0 && Distance <= MaxDist3)
    {
      AvrLn3++;
      AvrLn3 -= AvrLn3 >> 8;
    }
    else
      if (AvrLn3 > 0)
        AvrLn3--;
  Length+=3;
  if (Distance >= MaxDist3)
    Length++;
  if (Distance <= 256)
    Length+=8;
  if (OldAvr3 > 0xb0 || AvrPlc >= 0x2a00 && OldAvr2 < 0x40)
    MaxDist3=0x7f00;
  else
    MaxDist3=0x2001;
  OldDist[OldDistPtr++]=Distance;
  OldDistPtr = OldDistPtr & 3;
  LastLength=Length;
  LastDist=Distance;
  OldCopyString(Distance,Length);
}


void CRarTool::HuffDecode()
{
  unsigned int CurByte,NewBytePlace;
  unsigned int Length;
  unsigned int Distance;
  int BytePlace;

  GetBits();

  if (AvrPlc > 0x75ff)
    BytePlace=DecodeNum(BitField,STARTHF4,DecHf4,PosHf4);
  else
    if (AvrPlc > 0x5dff)
      BytePlace=DecodeNum(BitField,STARTHF3,DecHf3,PosHf3);
    else
      if (AvrPlc > 0x35ff)
        BytePlace=DecodeNum(BitField,STARTHF2,DecHf2,PosHf2);
      else
        if (AvrPlc > 0x0dff)
          BytePlace=DecodeNum(BitField,STARTHF1,DecHf1,PosHf1);
        else
          BytePlace=DecodeNum(BitField,STARTHF0,DecHf0,PosHf0);
  if (StMode)
  {
    if (BytePlace==0 && BitField > 0xfff)
      BytePlace=0x100;
    if (--BytePlace==-1)
    {
      GetBits();
      AddBits(1);
      if (BitField & 0x8000)
      {
        NumHuf=StMode=0;
        return;
      }
      else
      {
        Length = (BitField & 0x4000) ? 4 : 3;
        AddBits(1);
        GetBits();
        Distance=DecodeNum(BitField,STARTHF2,DecHf2,PosHf2);
        GetBits();
        Distance = (Distance << 5) | (BitField >> 11);
        AddBits(5);
        OldCopyString(Distance,Length);
        return;
      }
    }
  }
  else
    if (NumHuf++ >= 16 && FlagsCnt==0)
      StMode=1;
  AvrPlc += BytePlace;
  AvrPlc -= AvrPlc >> 8;
  Nhfb+=16;
  if (Nhfb > 0xff)
  {
    Nhfb=0x90;
    Nlzb >>= 1;
  }

  CTool::common_buf[UnpPtr++]=(BYTE)(ChSet[BytePlace]>>8);
  DestUnpSize--;

  while (1)
  {
    CurByte=ChSet[BytePlace];
    NewBytePlace=NToPl[CurByte++ & 0xff]++;
    if ((CurByte & 0xff) > 0xa1)
      CorrHuff(ChSet,NToPl);
    else
      break;
  }

  ChSet[BytePlace]=ChSet[NewBytePlace];
  ChSet[NewBytePlace]=CurByte;
}


void CRarTool::GetFlagsBuf()
{
  unsigned int Flags,FlagsPlace,NewFlagsPlace;

  GetBits();
  FlagsPlace=DecodeNum(BitField,STARTHF2,DecHf2,PosHf2);

  while (1)
  {
    Flags=ChSetC[FlagsPlace];
    FlagBuf=Flags>>8;
    NewFlagsPlace=NToPlC[Flags++ & 0xff]++;
    if ((Flags & 0xff) == 0)
      CorrHuff(ChSetC,NToPlC);
    else
      break;
  }

  ChSetC[FlagsPlace]=ChSetC[NewFlagsPlace];
  ChSetC[NewFlagsPlace]=Flags;
}

void CRarTool::OldUnpInitData(bool bSolid)
{
  if(!bSolid)
  {
    AvrPlcB=AvrLn1=AvrLn2=AvrLn3=NumHuf=Buf60=0;
    AvrPlc=0x3500;
    MaxDist3=0x2001;
    Nhfb=Nlzb=0x80;
  }
  FlagsCnt=0;
  FlagBuf=0;
  StMode=0;
  LCount=0;
}


void CRarTool::InitHuff()
{
  unsigned int I;
  for (I=0;I<256;I++)
  {
    Place[I]=PlaceA[I]=PlaceB[I]=I;
    PlaceC[I]=(~I+1) & 0xff;
    ChSet[I]=ChSetB[I]=I<<8;
    ChSetA[I]=I;
    ChSetC[I]=((~I+1) & 0xff)<<8;
  }
  memset(NToPl,0,sizeof(NToPl));
  memset(NToPlB,0,sizeof(NToPlB));
  memset(NToPlC,0,sizeof(NToPlC));
  CorrHuff(ChSetB,NToPlB);
}


void CRarTool::CorrHuff(unsigned int *CharSet,unsigned int *NumToPlace)
{
  int I,J;
  for (I=7;I>=0;I--)
    for (J=0;J<32;J++,CharSet++)
      *CharSet=(*CharSet & ~0xff) | I;
  memset(NumToPlace,0,sizeof(NToPl));
  for (I=6;I>=0;I--)
    NumToPlace[I]=(7-I)*32;
}


void CRarTool::OldCopyString(unsigned int Distance,unsigned int Length)
{
  DestUnpSize-=Length;
  while (Length--)
  {
    CTool::common_buf[UnpPtr]=CTool::common_buf[(UnpPtr-Distance) & MAXWINMASK];
    UnpPtr=(UnpPtr+1) & MAXWINMASK;
  }
}


unsigned int CRarTool::DecodeNum(int Num,unsigned int StartPos,unsigned int *DecTab,unsigned int *PosTab)
{
  int I;
  for (Num&=0xfff0,I=0;(int)DecTab[I]<=Num;I++)
    StartPos++;
  AddBits(StartPos);
  return(((Num-(I ? DecTab[I-1]:0))>>(16-StartPos))+PosTab[StartPos]);
}

/**** Unpackers(New) *****************************************************/

void CRarTool::Unpack(bool bSolid)
{
  static unsigned char LDecode[]={0,1,2,3,4,5,6,7,8,10,12,14,16,20,24,28,32,40,48,56,64,80,96,112,128,160,192,224};
  static unsigned char LBits[]=  {0,0,0,0,0,0,0,0,1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5};
  static int DDecode[]={0,1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,512,768,1024,1536,2048,3072,4096,6144,8192,12288,16384,24576,32768U,49152U,65536,98304,131072,196608,262144,327680,393216,458752,524288,589824,655360,720896,786432,851968,917504,983040};
  static unsigned char DBits[]=  {0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13,    14,    14,   15,   15,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16,    16};
  static unsigned char SDDecode[]={0,4,8,16,32,64,128,192};
  static unsigned char SDBits[]={2,2,3, 4, 5, 6,  6,  6};
  unsigned int Bits;

  UnpInitData(bSolid);
  UnpReadBuf(1);
  if(!bSolid)
    ReadTables();
  DestUnpSize--;

  while (DestUnpSize>=0)
  {
    UnpPtr&=MAXWINMASK;

    if (InAddr>sizeof(InBuf)-30)
      UnpReadBuf(0);
    if (((WrPtr-UnpPtr) & MAXWINMASK)<270 && WrPtr!=UnpPtr)
      UnpWriteBuf();
    if (UnpAudioBlock)
    {
      DecodeNumber((struct Decode *)MDPtr[CurChannel]);
      if (Number==256)
      {
        ReadTables();
        continue;
      }
      CTool::common_buf[UnpPtr++]=DecodeAudio(Number);
      if (++CurChannel==UnpChannels)
        CurChannel=0;
      DestUnpSize--;
      continue;
    }

    DecodeNumber((struct Decode *)&LD);
    if (Number<256)
    {
      CTool::common_buf[UnpPtr++]=(BYTE)Number;
      DestUnpSize--;
      continue;
    }
    if (Number>269)
    {
      Length=LDecode[Number-=270]+3;
      if ((Bits=LBits[Number])>0)
      {
        GetBits();
        Length+=BitField>>(16-Bits);
        AddBits(Bits);
      }

      DecodeNumber((struct Decode *)&DD);
      Distance=DDecode[Number]+1;
      if ((Bits=DBits[Number])>0)
      {
        GetBits();
        Distance+=BitField>>(16-Bits);
        AddBits(Bits);
      }

      if (Distance>=0x40000L)
        Length++;

      if (Distance>=0x2000)
        Length++;

      CopyString();
      continue;
    }
    if (Number==269)
    {
      ReadTables();
      continue;
    }
    if (Number==256)
    {
      Length=LastLength;
      Distance=LastDist;
      CopyString();
      continue;
    }
    if (Number<261)
    {
      Distance=OldDist[(OldDistPtr-(Number-256)) & 3];
      DecodeNumber((struct Decode *)&RD);
      Length=LDecode[Number]+2;
      if ((Bits=LBits[Number])>0)
      {
        GetBits();
        Length+=BitField>>(16-Bits);
        AddBits(Bits);
      }
      if (Distance>=0x40000)
        Length++;
      if (Distance>=0x2000)
        Length++;
      if (Distance>=0x101)
        Length++;
      CopyString();
      continue;
    }
    if (Number<270)
    {
      Distance=SDDecode[Number-=261]+1;
      if ((Bits=SDBits[Number])>0)
      {
        GetBits();
        Distance+=BitField>>(16-Bits);
        AddBits(Bits);
      }
      Length=2;
      CopyString();
      continue;
   }
  }
  ReadLastTables();
  UnpWriteBuf();
}


void CRarTool::UnpReadBuf(int FirstBuf)
{
  int RetCode;
  if (FirstBuf)
  {
    ReadTop=UnpRead(InBuf,sizeof(InBuf));
    InAddr=0;
  }
  else
  {
    memcpy(InBuf,&InBuf[sizeof(InBuf)-32],32);
    InAddr&=0x1f;
    RetCode=UnpRead(&InBuf[32],sizeof(InBuf)-32);
    if (RetCode>0)
      ReadTop=RetCode+32;
    else
      ReadTop=InAddr;
  }
}

void CRarTool::UnpWriteBuf()
{
  if (UnpPtr<WrPtr)
  {
// ちゃんとなってるかわからんけど一応警告回避のため...
//    UnpWrite(&CTool::common_buf[WrPtr],-WrPtr & MAXWINMASK);
    UnpWrite(&CTool::common_buf[WrPtr],-(int)WrPtr & MAXWINMASK);
    UnpWrite(CTool::common_buf,UnpPtr);
  }
  else
    UnpWrite(&CTool::common_buf[WrPtr],UnpPtr-WrPtr);
  WrPtr=UnpPtr;
}

void CRarTool::ReadTables()
{
  BYTE BitLength[BC];
  unsigned char Table[MC*4];
  int TableSize,N,I;
  if (InAddr>sizeof(InBuf)-25)
    UnpReadBuf(0);
  GetBits();
  UnpAudioBlock=(BitField & 0x8000);

  if (!(BitField & 0x4000))
    memset(UnpOldTable,0,sizeof(UnpOldTable));
  AddBits(2);

  if (UnpAudioBlock)
  {
    UnpChannels=((BitField>>12) & 3)+1;
    if (CurChannel>=UnpChannels)
      CurChannel=0;
    AddBits(2);
    TableSize=MC*UnpChannels;
  }
  else
    TableSize=NC+DC+RC;


  for (I=0;I<BC;I++)
  {
    GetBits();
    BitLength[I]=(BYTE)(BitField >> 12);
    AddBits(4);
  }
  MakeDecodeTables(BitLength,(struct Decode *)&BD,BC);
  I=0;
  while (I<TableSize)
  {
    if (InAddr>sizeof(InBuf)-5)
      UnpReadBuf(0);
    DecodeNumber((struct Decode *)&BD);
    if (Number<16)
      Table[I++]=(Number+UnpOldTable[I]) & 0xf;
    else
      if (Number==16)
      {
        GetBits();
        N=(BitField >> 14)+3;
        AddBits(2);
        while (N-- > 0 && I<TableSize)
        {
          Table[I]=Table[I-1];
          I++;
        }
      }
      else
      {
        if (Number==17)
        {
          GetBits();
          N=(BitField >> 13)+3;
          AddBits(3);
        }
        else
        {
          GetBits();
          N=(BitField >> 9)+11;
          AddBits(7);
        }
        while (N-- > 0 && I<TableSize)
          Table[I++]=0;
      }
  }
  if (UnpAudioBlock)
    for (I=0;I<UnpChannels;I++)
      MakeDecodeTables(&Table[I*MC],(struct Decode *)MDPtr[I],MC);
  else
  {
    MakeDecodeTables(&Table[0],(struct Decode *)&LD,NC);
    MakeDecodeTables(&Table[NC],(struct Decode *)&DD,DC);
    MakeDecodeTables(&Table[NC+DC],(struct Decode *)&RD,RC);
  }
  memcpy(UnpOldTable,Table,sizeof(UnpOldTable));
}

void CRarTool::ReadLastTables()
{
  if (ReadTop>=InAddr+5)
    if (UnpAudioBlock)
    {
      DecodeNumber((struct Decode *)MDPtr[CurChannel]);
      if (Number==256)
        ReadTables();
    }
    else
    {
      DecodeNumber((struct Decode *)&LD);
      if (Number==269)
        ReadTables();
    }
}

void CRarTool::MakeDecodeTables(unsigned char *LenTab,struct Decode *Dec,int Size)
{
  int LenCount[16],TmpPos[16],I;
  long M,N;
  memset(LenCount,0,sizeof(LenCount));
  for (I=0;I<Size;I++)
    LenCount[LenTab[I] & 0xF]++;

  LenCount[0]=0;
  for (TmpPos[0]=Dec->DecodePos[0]=Dec->DecodeLen[0]=0,N=0,I=1;I<16;I++)
  {
    N=2*(N+LenCount[I]);
    M=N<<(15-I);
    if (M>0xFFFF)
      M=0xFFFF;
    Dec->DecodeLen[I]=(unsigned int)M;
    TmpPos[I]=Dec->DecodePos[I]=Dec->DecodePos[I-1]+LenCount[I-1];
  }

  for (I=0;I<Size;I++)
    if (LenTab[I]!=0)
      Dec->DecodeNum[TmpPos[LenTab[I] & 0xF]++]=I;
  Dec->MaxNum=Size;
}

void CRarTool::DecodeNumber(struct Decode *Dec)
{
  unsigned int I;
  register unsigned int N;
  GetBits();
  N=BitField & 0xFFFE;
  if (N<Dec->DecodeLen[8])
    if (N<Dec->DecodeLen[4])
      if (N<Dec->DecodeLen[2])
        if (N<Dec->DecodeLen[1])
          I=1;
        else
          I=2;
      else
        if (N<Dec->DecodeLen[3])
          I=3;
        else
          I=4;
    else
      if (N<Dec->DecodeLen[6])
        if (N<Dec->DecodeLen[5])
          I=5;
        else
          I=6;
      else
        if (N<Dec->DecodeLen[7])
          I=7;
        else
          I=8;
  else
    if (N<Dec->DecodeLen[12])
      if (N<Dec->DecodeLen[10])
        if (N<Dec->DecodeLen[9])
          I=9;
        else
          I=10;
      else
        if (N<Dec->DecodeLen[11])
          I=11;
        else
          I=12;
    else
      if (N<Dec->DecodeLen[14])
        if (N<Dec->DecodeLen[13])
          I=13;
        else
          I=14;
      else
        I=15;

  AddBits(I);
  if ((N=Dec->DecodePos[I]+((N-Dec->DecodeLen[I-1])>>(16-I)))>=Dec->MaxNum)
    N=0;
  Number=Dec->DecodeNum[N];
}

void CRarTool::CopyString()
{
  LastDist=OldDist[OldDistPtr++ & 3]=Distance;
  DestUnpSize-=(LastLength=Length);
  while (Length--)
  {
    CTool::common_buf[UnpPtr]=CTool::common_buf[(UnpPtr-Distance) & MAXWINMASK];
    UnpPtr=(UnpPtr+1) & MAXWINMASK;
  }
}

void CRarTool::UnpInitData(bool bSolid)
{
  InAddr=InBit=0;
  if (!bSolid)
  {
    ChannelDelta=CurChannel=0;
    memset(AudV,0,sizeof(AudV));
    memset(OldDist,0,sizeof(OldDist));
    OldDistPtr=0;
    LastDist=LastLength=0;
    memset(CTool::common_buf,0,MAXWINSIZE);
    memset(UnpOldTable,0,sizeof(UnpOldTable));
    UnpPtr=WrPtr=0;
  }
}

BYTE CRarTool::DecodeAudio(int Delta)
{
  struct AudioVariables *V;
  unsigned int Ch;
  unsigned int NumMinDif,MinDif;
  int PCh,I;

  V=&AudV[CurChannel];
  V->ByteCount++;
  V->D4=V->D3;
  V->D3=V->D2;
  V->D2=V->LastDelta-V->D1;
  V->D1=V->LastDelta;
  PCh=8*V->LastChar+V->K1*V->D1+V->K2*V->D2+V->K3*V->D3+V->K4*V->D4+V->K5*ChannelDelta;
  PCh=(PCh>>3) & 0xFF;

  Ch=PCh-Delta;

  I=((signed char)Delta)<<3;

  V->Dif[0]+=abs(I);
  V->Dif[1]+=abs(I-V->D1);
  V->Dif[2]+=abs(I+V->D1);
  V->Dif[3]+=abs(I-V->D2);
  V->Dif[4]+=abs(I+V->D2);
  V->Dif[5]+=abs(I-V->D3);
  V->Dif[6]+=abs(I+V->D3);
  V->Dif[7]+=abs(I-V->D4);
  V->Dif[8]+=abs(I+V->D4);
  V->Dif[9]+=abs(I-ChannelDelta);
  V->Dif[10]+=abs(I+ChannelDelta);

  ChannelDelta=V->LastDelta=(signed char)(Ch-V->LastChar);
  V->LastChar=Ch;

  if ((V->ByteCount & 0x1F)==0)
  {
    MinDif=V->Dif[0];
    NumMinDif=0;
    V->Dif[0]=0;
    for (I=1;I<sizeof(V->Dif)/sizeof(V->Dif[0]);I++)
    {
      if (V->Dif[I]<MinDif)
      {
        MinDif=V->Dif[I];
        NumMinDif=I;
      }
      V->Dif[I]=0;
    }
    switch(NumMinDif)
    {
      case 1:
        if (V->K1>=-16)
          V->K1--;
        break;
      case 2:
        if (V->K1<16)
          V->K1++;
        break;
      case 3:
        if (V->K2>=-16)
          V->K2--;
        break;
      case 4:
        if (V->K2<16)
          V->K2++;
        break;
      case 5:
        if (V->K3>=-16)
          V->K3--;
        break;
      case 6:
        if (V->K3<16)
          V->K3++;
        break;
      case 7:
        if (V->K4>=-16)
          V->K4--;
        break;
      case 8:
        if (V->K4<16)
          V->K4++;
        break;
      case 9:
        if (V->K5>=-16)
          V->K5--;
        break;
      case 10:
        if (V->K5<16)
          V->K5++;
        break;
    }
  }
  return((BYTE)Ch);
}


/**** Decryption & CRC *****************************************************/
#define NROUNDS 32
#define  rol(x,n)  (((x)<<(n)) | ((x)>>(8*sizeof(x)-(n))))
#define  ror(x,n)  (((x)>>(n)) | ((x)<<(8*sizeof(x)-(n))))
#define substLong(t) ( (DWORD)SubstTable[(int)t&255] | \
           ((DWORD)SubstTable[(int)(t>> 8)&255]<< 8) | \
           ((DWORD)SubstTable[(int)(t>>16)&255]<<16) | \
           ((DWORD)SubstTable[(int)(t>>24)&255]<<24) )
static const BYTE InitSubstTable[256]={
  215, 19,149, 35, 73,197,192,205,249, 28, 16,119, 48,221,  2, 42,
  232,  1,177,233, 14, 88,219, 25,223,195,244, 90, 87,239,153,137,
  255,199,147, 70, 92, 66,246, 13,216, 40, 62, 29,217,230, 86,  6,
   71, 24,171,196,101,113,218,123, 93, 91,163,178,202, 67, 44,235,
  107,250, 75,234, 49,167,125,211, 83,114,157,144, 32,193,143, 36,
  158,124,247,187, 89,214,141, 47,121,228, 61,130,213,194,174,251,
   97,110, 54,229,115, 57,152, 94,105,243,212, 55,209,245, 63, 11,
  164,200, 31,156, 81,176,227, 21, 76, 99,139,188,127, 17,248, 51,
  207,120,189,210,  8,226, 41, 72,183,203,135,165,166, 60, 98,  7,
  122, 38,155,170, 69,172,252,238, 39,134, 59,128,236, 27,240, 80,
  131,  3, 85,206,145, 79,154,142,159,220,201,133, 74, 64, 20,129,
  224,185,138,103,173,182, 43, 34,254, 82,198,151,231,180, 58, 10,
  118, 26,102, 12, 50,132, 22,191,136,111,162,179, 45,  4,148,108,
  161, 56, 78,126,242,222, 15,175,146, 23, 33,241,181,190, 77,225,
    0, 46,169,186, 68, 95,237, 65, 53,208,253,168,  9, 18,100, 52,
  116,184,160, 96,109, 37, 30,106,140,104,150,  5,204,117,112, 84
};
void CRarTool::InitCRC()
{
	DWORD c,i,j;
	for(i=0;i!=256;i++)
	{
		for(c=i,j=0;j!=8;j++)
			c=(c&1)?(c>>1)^0xEDB88320L:(c>>1);
		crcTable[i]=c;
	}
}
DWORD CRarTool::UpdateChecker( DWORD StartVal,BYTE *ptr,DWORD Size,bool b32Bit )
{
	if( b32Bit )
		for(DWORD i=0;i!=Size;i++)
			StartVal = crcTable[(BYTE)StartVal^ptr[i]]^(StartVal>>8);
	else
		for(DWORD i=0;i!=Size;i++)
			StartVal+=ptr[i],StartVal=(WORD)rol(StartVal,1);
	return StartVal;
}
void CRarTool::EncryptBlock(BYTE *Buf)
{
	int I;
	DWORD A,B,C,D,T,TA,TB;
	DWORD *BufPtr;
	BufPtr=(DWORD *)Buf;
	A=BufPtr[0]^Key[0];
	B=BufPtr[1]^Key[1];
	C=BufPtr[2]^Key[2];
	D=BufPtr[3]^Key[3];
	for(I=0;I<NROUNDS;I++)
	{
		T=((C+rol(D,11))^Key[I&3]);
		TA=A^substLong(T);
		T=((D^rol(C,17))+Key[I&3]);
		TB=B^substLong(T);
		A=C;
		B=D;
		C=TA;
		D=TB;
	}
	BufPtr[0]=C^Key[0];
	BufPtr[1]=D^Key[1];
	BufPtr[2]=A^Key[2];
	BufPtr[3]=B^Key[3];
	UpdKeys(Buf);
}
void CRarTool::DecryptBlock(BYTE *Buf)
{
	int I;
	BYTE InBuf[16];
	DWORD A,B,C,D,T,TA,TB;
	DWORD *BufPtr;
	BufPtr=(DWORD *)Buf;
	A=BufPtr[0]^Key[0];
	B=BufPtr[1]^Key[1];
	C=BufPtr[2]^Key[2];
	D=BufPtr[3]^Key[3];
	memcpy(InBuf,Buf,sizeof(InBuf));
	for(I=NROUNDS-1;I>=0;I--)
	{
		T=((C+rol(D,11))^Key[I&3]);
		TA=A^substLong(T);
		T=((D^rol(C,17))+Key[I&3]);
		TB=B^substLong(T);
		A=C;
		B=D;
		C=TA;
		D=TB;
	}
	BufPtr[0]=C^Key[0];
	BufPtr[1]=D^Key[1];
	BufPtr[2]=A^Key[2];
	BufPtr[3]=B^Key[3];
	UpdKeys(InBuf);
}
void CRarTool::UpdKeys(BYTE *Buf)
{
	for( int i=0;i!=16;i+=4)
	{
		Key[0]^=crcTable[Buf[i]];
		Key[1]^=crcTable[Buf[i+1]];
		Key[2]^=crcTable[Buf[i+2]];
		Key[3]^=crcTable[Buf[i+3]];
	}
}
void CRarTool::Swap(BYTE *Ch1,BYTE *Ch2)
{
	BYTE Ch=*Ch1;
	*Ch1=*Ch2;
	*Ch2=Ch;
}
void CRarTool::SetCryptKeys(char *Password)
{
	unsigned int I,J,K,PswLength;
	BYTE N1,N2;
	unsigned char Psw[256];
	SetOldKeys(Password);
	Key[0]=0xD3A3B879L;
	Key[1]=0x3F6D12F7L;
	Key[2]=0x7515A235L;
	Key[3]=0xA4E7F123L;
	memset(Psw,0,sizeof(Psw));
	strcpy((char*)Psw,Password);
	PswLength=strlen(Password);
	memcpy(SubstTable,InitSubstTable,sizeof(SubstTable));
	for(J=0;J!=256;J++)
		for(I=0;I<PswLength;I+=2)
		{
			N2=(BYTE)crcTable[(Psw[I+1]+J)&0xFF];
			for(K=1,N1=(BYTE)crcTable[(Psw[I]-J)&0xFF];(N1-N2)&0xFF;N1++,K++)
				Swap(&SubstTable[N1&0xFF],&SubstTable[(N1+I+K)&0xFF]);
		}
	for(I=0;I<PswLength;I+=16)
		EncryptBlock(&Psw[I]);
}
void CRarTool::SetOldKeys(char *Password)
{
	DWORD PswCRC;
	BYTE Ch;
	PswCRC=UpdateChecker(0xFFFFFFFFL,(BYTE*)Password,strlen(Password),true);
	OldKey[0]=(WORD)PswCRC;
	OldKey[1]=(WORD)(PswCRC>>16);
	OldKey[2]=OldKey[3]=0;
	PN1=PN2=PN3=0;
	while((Ch=*Password)!=0)
	{
		PN1+=Ch;
		PN2^=Ch;
		PN3+=Ch;
		PN3=(BYTE)rol(PN3,1);
		OldKey[2]^=(WORD)(Ch^crcTable[Ch]);
		OldKey[3]+=(WORD)(Ch+(crcTable[Ch]>>16));
		Password++;
	}
}
void CRarTool::Crypt(BYTE *Data,DWORD Count,int Method)
{
	if( Method==OLD_DECODE )
		Decode13(Data,Count);
	else
		if( Method==OLD_ENCODE )
			Encode13(Data,Count);
		else
			Crypt15(Data,Count);
}
void CRarTool::Encode13(BYTE *Data,DWORD Count)
{
	while(Count--)
	{
		PN2+=PN3;
		PN1+=PN2;
		*Data+=PN1;
		Data++;
	}
}
void CRarTool::Decode13(BYTE *Data,DWORD Count)
{
	while(Count--)
	{
		PN2+=PN3;
		PN1+=PN2;
		*Data-=PN1;
		Data++;
	}
}
void CRarTool::Crypt15(BYTE *Data,DWORD Count)
{
	while(Count--)
	{
		OldKey[0]+=(WORD)0x1234;
		OldKey[1]^=(WORD)crcTable[(OldKey[0] & 0x1fe)>>1];
		OldKey[2]-=(WORD)(crcTable[(OldKey[0] & 0x1fe)>>1]>>16);
		OldKey[0]^=OldKey[2];
		OldKey[3]=(WORD)ror(OldKey[3],1)^OldKey[1];
		OldKey[3]=(WORD)ror(OldKey[3],1);
		OldKey[0]^=OldKey[3];
		*Data^=(BYTE)(OldKey[0]>>8);
		Data++;
	}
}

/******* いろいろ ***************************/

DWORD CRarTool::UnpRead(BYTE *Addr,DWORD Count)
{
	unsigned int RetCode=0,ReadSize,TotalRead=0;
	unsigned char *ReadAddr=Addr;

	while( Count>0 )
	{
		ReadSize=(unsigned int)((Count>(unsigned long)UnpPackedSize) ? UnpPackedSize : Count);
		if(arc==NULL)
			return 0;
		RetCode=fread(ReadAddr,1,ReadSize,arc);
		if( NewLhd.Flags&LHD_SPLIT_AFTER )
			PackedCRC=UpdateChecker(PackedCRC,ReadAddr,ReadSize,true);
		CurUnpRead+=RetCode;
		ReadAddr+=RetCode;
		TotalRead+=RetCode;
		Count-=RetCode;
		UnpPackedSize-=RetCode;
		if( UnpPackedSize==0 && bNeedNextVol )
			GetNextVolume();
		else
			break;
	}
	if( RetCode!=0xffffffff )
	{
		RetCode=TotalRead;
		if( Encryption )
			if( Encryption<20 )
				Crypt(Addr,RetCode,(Encryption==15)?NEW_CRYPT:OLD_DECODE);
		else
			for(DWORD I=0;I<RetCode;I+=16)
				DecryptBlock(&Addr[I]);
	}
	return RetCode;
}

DWORD CRarTool::UnpWrite(BYTE *Addr,DWORD Count)
{
	DWORD RetCode=fwrite(Addr,1,Count,out);
	CurUnpWrite+=RetCode;
	UnpFileCRC=UpdateChecker(UnpFileCRC,Addr,RetCode,ArcFormat==NEW);
	return RetCode;
}

void CRarTool::Unstore()
{
	int Code;
	BYTE* TempMemory=new BYTE[0x8000];
	while( true )
	{
		if( (Code=UnpRead(TempMemory,0x8000))<=0 )
			break;
		Code=(int)Min((unsigned long)Code,(unsigned long)DestUnpSize);
		UnpWrite(TempMemory,Code);
		if( DestUnpSize>=0 )
			DestUnpSize-=Code;
	}
	delete [] TempMemory;
}

bool CRarTool::GetNextVolume()
{
	fclose(arc);

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

	if( (arc=fopen(ArcName,"rb"))==NULL )
		return false;
	if( !IsArchive(false) )
		return false;

	fseek(arc,NewMhd.HeadSize-MainHeadSize,SEEK_CUR);
	ReadBlock(FILE_HEAD);
	fseek(arc,NextBlockPos-NewLhd.PackSize,SEEK_SET);
	bNeedNextVol=(0!=(NewLhd.Flags&LHD_SPLIT_AFTER));

	UnpPackedSize=NewLhd.PackSize;
	PackedCRC=0xFFFFFFFF;
	return true;
}

bool CRarTool::IsArchive(bool bStrict)
{
	bSolid=false;
	ArcFormat=0;

	if( fread(MarkHead.Mark,1,SIZEOF_MARKHEAD,arc)!=SIZEOF_MARKHEAD )
		return false;

	if( MarkHead.Mark[0]==0x52 && MarkHead.Mark[1]==0x45 && MarkHead.Mark[2]==0x7e && MarkHead.Mark[3]==0x5e)
	{
		ArcFormat=OLD;
		fseek(arc,0,SEEK_SET);
		ReadHeader(MAIN_HEAD);
	}
	else if( MarkHead.Mark[0]==0x52 && MarkHead.Mark[1]==0x61 &&
			 MarkHead.Mark[2]==0x72 && MarkHead.Mark[3]==0x21 &&
			 MarkHead.Mark[4]==0x1a && MarkHead.Mark[5]==0x07 &&
			 MarkHead.Mark[6]==0x00)
	{
		ArcFormat=NEW;
		if( ReadHeader(MAIN_HEAD)!=SIZEOF_NEWMHD )
			return false;
    }
    else
    {
		BYTE* TempMemory=new BYTE[0x20000];
		long CurPos=ftell(arc);
		int ReadSize=fread(TempMemory,1,0x1FFF0,arc);
		unsigned char* ChPtr=TempMemory;

		while( ArcFormat==0 && ChPtr!=NULL && ChPtr<TempMemory+ReadSize )
		{
			if( (ChPtr=(BYTE*)memchr(ChPtr,0x52,ReadSize-(int)(ChPtr-TempMemory)))!=NULL )
			{
				ChPtr++;
				if( ChPtr[0]==0x45 && ChPtr[1]==0x7e && ChPtr[2]==0x5e &&
					TempMemory[28-CurPos]==0x52 && TempMemory[29-CurPos]==0x53 &&
					TempMemory[30-CurPos]==0x46 && TempMemory[31-CurPos]==0x58)
				{
					ArcFormat=OLD;
					fseek(arc,(CurPos+ChPtr-TempMemory-1),SEEK_SET);
					ReadHeader(MAIN_HEAD);
				}
				if( ChPtr[0]==0x61 && ChPtr[1]==0x72 && ChPtr[2]==0x21 &&
					ChPtr[3]==0x1a && ChPtr[4]==0x07 && ChPtr[5]==0x00 &&
					ChPtr[8]==MAIN_HEAD)
				{
					ArcFormat=NEW;
					fseek(arc,(CurPos+ChPtr-TempMemory-1),SEEK_SET);
					fread(MarkHead.Mark,1,SIZEOF_MARKHEAD,arc);
					ReadHeader(MAIN_HEAD);
				}
			}
		}
		delete [] TempMemory;
		if( !ArcFormat )
			return false;
    }

	if( ArcFormat==OLD )
	{
		MainHeadSize=SIZEOF_OLDMHD;
		NewMhd.Flags=OldMhd.Flags & 0x3f;
		NewMhd.HeadSize=OldMhd.HeadSize;
	}
	else
	{
		MainHeadSize=SIZEOF_NEWMHD;
		if( bStrict && (WORD)~HeaderCRC!=NewMhd.HeadCRC )
			return false;
	}

	if( NewMhd.Flags & MHD_SOLID )
		bSolid=true;

	return true;
}

int CRarTool::ReadHeader(int BlockType)
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
			Size=fread(Header,1,SIZEOF_OLDMHD,arc);
			memcpy(OldMhd.Mark,Header,4);
			OldMhd.HeadSize=GetHeaderWord(4);
			OldMhd.Flags=GetHeaderByte(6);
		}
		else
		{
			Size=fread(Header,1,SIZEOF_NEWMHD,arc);
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
			Size=fread(Header,1,SIZEOF_OLDLHD,arc);
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
			Size=fread(Header,1,SIZEOF_NEWLHD,arc);
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
		Size=fread(Header,1,SIZEOF_COMMHEAD,arc);
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
		Size=fread(Header,1,SIZEOF_PROTECTHEAD,arc);
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
		Size=fread(Header,1,SIZEOF_SHORTBLOCKHEAD,arc);
		BlockHead.HeadCRC=GetHeaderWord(0);
		BlockHead.HeadType=GetHeaderByte(2);
		BlockHead.Flags=GetHeaderWord(3);
		BlockHead.HeadSize=GetHeaderWord(5);
		if( BlockHead.Flags&LONG_BLOCK )
		{
			Size+=fread(&Header[7],1,4,arc);
			BlockHead.DataSize=GetHeaderDword(7);
		}
		break;
	}
	return Size;
}

int CRarTool::ReadBlock(int BlockType)
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
		CurBlockPos=ftell(arc);
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
			CurBlockPos=ftell(arc);
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
			fseek(arc,NextBlockPos,SEEK_SET);
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
			fread(ExtrName,1,NewLhd.NameSize,arc);
			ExtrName[NewLhd.NameSize]=0;
			if( ArcFormat==NEW && NewLhd.HeadCRC!=(WORD)~UpdateChecker(HeaderCRC,(BYTE*)ExtrName,NewLhd.NameSize,true) )
				return 0;
			Size+=NewLhd.NameSize;
		}
	}
	else
	{
		memcpy(&NewLhd,&SaveFileHead,sizeof(NewLhd));
		fseek(arc,CurBlockPos,SEEK_SET);
	}

	return Size;
}
