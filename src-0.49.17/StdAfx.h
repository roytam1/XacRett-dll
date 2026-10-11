// stdafx.h : 標準のシステム インクルード ファイル、
//            または参照回数が多く、かつあまり変更されない
//            プロジェクト専用のインクルード ファイルを記述します。
//
#ifndef AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#include <windows.h>
#include <shlobj.h>

#include <stdio.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#pragma warning( disable : 4786 )

/* VC6's SDK has no basetsd.h, so the fixed-width spellings IsoTool.h and
   friends expect are undefined. Provide them only if absent, so a newer
   SDK still wins. */
#if !defined(UINT8)
typedef unsigned char  UINT8;
typedef signed char    INT8;
typedef unsigned short UINT16;
typedef unsigned int   UINT32;
#endif

void LastErrorMsg();
#include "MemmapFile.h"
#endif