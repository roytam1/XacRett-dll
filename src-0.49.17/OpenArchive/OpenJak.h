#ifndef AFX_OPENJAK_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENJAK_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_

#include "OpenArchive.h"

class COpenJak : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenJak() : COpenArchive( )
	{COpenJak::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	struct JakHeader
	{
		char Magic[18];		// This_Is_Jack_File\0
		char dummy1[2];
		DWORD No;			// 切り裂きファイルの連番( 1～ )
		DWORD FileNameLen;	// ファイル名の長さ
		DWORD FileSize;		// 切り裂く前のファイルサイズ
		FILETIME FileTime;	// ファイルの更新日付
		char dummy2[3];
	//	char* OrigFileName;	// ファイル名 ( NULL 終端なし )
	};
	char basedir[FNAME_MAX32 + 1];
	char bodyname[MAX_PATH+1];
	char tmp[18];
	DWORD OriginalSize;
	int FindHeader( const BYTE* hdr);
};
#endif