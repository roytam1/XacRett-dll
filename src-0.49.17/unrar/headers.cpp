#include "rar.hpp"

void FileHeader::Reset(size_t SubDataSize)
{
  SubData.resize(SubDataSize);
  BaseBlock::Reset();
  FileHash.Init(HASH_NONE);
  mtime.Reset();
  atime.Reset();
  ctime.Reset();
  SplitBefore=false;
  SplitAfter=false;

  UnknownUnpSize=0;

  SubFlags=0; // Important for RAR 3.0 subhead.
  
  CryptMethod=CRYPT_NONE;
  Encrypted=false;
  SaltSet=false;
  UsePswCheck=false;
  UseHashKey=false;
  Lg2Count=0;

  Solid=false;
  Dir=false;
  WinSize=0;
  Inherited=false;
  SubBlock=false;
  CommentInHeader=false;
  Version=false;
  LargeFile=false;

  RedirType=FSREDIR_NONE;
  DirTarget=false;
  UnixOwnerSet=false;
}


/*
FileHeader& FileHeader::operator = (FileHeader &hd)
{
  SubData.Reset();
  memcpy(this,&hd,sizeof(*this));
  SubData.CleanData();
  SubData=hd.SubData;
  return *this;
}
*/


void MainHeader::Reset()
{
  *this=MainHeader(); /* VC6: no braced init (VC6_VALUEINIT) */
  /* VC6 does not zero POD members in T() when the struct also holds
     members with constructors (std::wstring/RarTime here), so the temp
     above leaves every POD with stack garbage. Zero the ones the header
     parse only assigns conditionally: a locator record with a zero
     quick-open offset never assigns QOpenOffset, and we would
     QOpen.Load() garbage - aida64.rar lists 0 members with ERAR_BAD_DATA
     for exactly this reason (its offset vint is 0; GPLv2-rar5.rar only
     worked because its nonzero offset overwrote the garbage). */
  HighPosAV=0;
  PosAV=0;
  CommentInHeader=false;
  PackComment=false;
  Locator=false;
  QOpenOffset=0;
  QOpenMaxSize=0;
  RROffset=0;
  RRMaxSize=0;
  MetaNameMaxSize=0;
}
