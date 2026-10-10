#ifndef AFX_OPENIs_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENIs_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenIs : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenIs() : COpenArchive( )
	{COpenIs::fp=NULL;m_FileList=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	int i;
	// ヘッダファイルの情報読み取り
	bool readHeader();
	// 書庫名保持
	void setupNamePattern( const char* fname );
	char m_arcNamePat[MAX_PATH*2];

	// ファイルハンドル
	bool openArc( int vol, const char* ext );
	void closeArc();

	// 格納ファイル情報
	struct fileInf
	{
		char    name[MAX_PATH]; //名前(パス付き)
		__int64 cbExpanded;     //解凍後のサイズ
		__int64 cbCompressed;   //圧縮後のサイズ
		__int64 ofsData;        //データ位置
		DWORD   attr;           //属性
		WORD    date, time;     //日時
		bool    cmprs, crypt;   //圧縮?暗号化?
	};
	fileInf* m_FileList;
	int m_FileNum;

};

#endif