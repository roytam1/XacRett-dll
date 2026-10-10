#ifndef AFX_ISTOOL_H__DC72C0F0_7D3A_4DFB_91E7_7C54A63A7FA1__INCLUDED_
#define AFX_ISTOOL_H__DC72C0F0_7D3A_4DFB_91E7_7C54A63A7FA1__INCLUDED_

#include "Tool.h"


class CIsTool : public CTool
{
//
// original source is
//
// i6comp v0.20 ( Dec-27-2000 )
//   ftp://ftp.externet.hu/pub/mirror/sac/pack/i6comp02.zip
// i5comp v2.01 ( Aug-24-1999 )
//   ftp://ftp.externet.hu/pub/mirror/sac/pack/i5comp21.rar
// 
// -] fOSSiL [-
// -] Morlac [-
//

public:

	CIsTool() : CTool( "InstallShield-Cab 展開" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"hdr");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:

	// ヘッダファイルの情報読み取り
	bool readHeader();
	// 抽出
	bool extractAll();
	void directCopy( bool crypt );
	void decompress();

	// マルチボリュームからの読み出しをカプセル化
	int m_nextVol;
	int m_sizeLimit;
	bool readInit();
	int readArc( void* buf, int siz );

	// 書庫名保持
	void setupNamePattern( const char* fname );
	char m_arcNamePat[MAX_PATH*2];

	// ファイルハンドル
	bool openArc( int vol, const char* ext );
	void closeArc();
	FILE *fp, *wp;

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

	static int __stdcall _r_(void* b, DWORD* s, CIsTool* p);
	static int __stdcall _w_(void* b, DWORD* s, CIsTool* p);
};

#endif
