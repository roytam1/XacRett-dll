#ifndef AFX_TOOL_H__57AAB1DD_886F_4D25_860C_2AA917B02AC1__INCLUDED_
#define AFX_TOOL_H__57AAB1DD_886F_4D25_860C_2AA917B02AC1__INCLUDED_

#include <vector>
#include <string>

#include "kiutil.h"
#define XACR_BUFSIZE (0x100000)
// ↑ must be bigger than 0x100000 !! ( for RAR )

typedef BOOL (CALLBACK *ARCHIVERPROC)(HWND _hwnd, UINT _uMsg,UINT _nState, LPARAM _lpEis);

class CTool
{
public:

////////////////////////////////////////////////
// 派生クラス作成についてMEMO
//
// GetPassword
// 　パスワードが必要になったらこれを呼ぶこと
// 　呼ぶ度にダイアログを出してユーザに入力させる
// common_buf
// 　作業用に自由に使ってよし。
// 　XACR_BUFSIZE byte 確保されているはず。
// 　
////////////////////////////////////////////////
//
// コンストラクタ
// 　CTool( ルーチン名 )を必ず呼び出すこと
//
// デストラクタ
// 　自由に
//
// IsType
// 　拡張子から対応書庫かどうか判定。ただし、拡張子
// 　以外のマトモな判定手段を持たないルーチンは
// 　ここでは必ず false を返さなくてはならない
//
// Check
// 　ファイル名, ファイルサイズ と、common_buf に入っている
// 　ファイル先頭の min(XACR_BUFSIZE,fsize) bytes を使って厳密に判定
// 　common_buf はこの時書き換え不可！
//
// Extract
// 　書庫"fname"をディレクトリ"ddir"(=カレント)に展開
//
////////////////////////////////////////////////

// 仮想関数 ////////////////

	virtual bool IsType( const char* ext ){return false;}
	virtual bool Check( const char* fname, unsigned long fsize ) = 0;
	virtual bool Extract( const char* fname, const char* ddir ) = 0;

// コンストラクタ＆デストラクタ ////

protected:
	CTool( const char* name )
		: m_Name(name), m_pNext(NULL), m_pPass(NULL){}
public:
	virtual ~CTool()
		{delete m_pNext;	delete [] m_pPass;}

// 作業用バッファ ///////

public:
	static void AllocWorkSpace()
	{
		FreeWorkSpace();
		common_buf = new unsigned char[XACR_BUFSIZE];
	}
	static void FreeWorkSpace()
	{
		if(common_buf)
			delete [] common_buf;
		common_buf=NULL;
	}

	static unsigned char* common_buf;

// ルーチン名 ///////////

public:
	const char* GetRoutineName()
		{return m_Name;}
private:
	const char* m_Name;

// 簡易リスト ///////////

public:
	CTool* GetNext()
		{return m_pNext;}
	CTool* AddTail( CTool* p )
		{
			if( !m_pNext )
				m_pNext=p;
			else
				m_pNext->AddTail(p);
			return p;
		}
private:
	CTool* m_pNext;

// パスワード ///////////
public:
	HINSTANCE InsTance;//追加By Bonty
protected:
	char* GetPassword();
private:
	char* m_pPass;
	static BOOL CALLBACK PassProc( HWND dlg,UINT msg,WPARAM wp,LPARAM lp );

//SetOwnerWindow用 ///////////追加By Bonty
public:
	static ARCHIVERPROC arcproc;
	static HWND			hWnd;
	static UINT			Msg;
	static bool			m_Endflg;
// 部分解凍用 ///////////追加By Bonty

public:
	//ワイルドカード可能
	//部分解凍用の文字列をセット
	void StrSet( std::vector<std::string>* w)
	{wildcard=w;}
	
protected:
	//char** wildcard;
	std::vector<std::string>* wildcard;

public:
	//StrMatchを使ってそれに合っているかどうかチェック
	//これ使うのExtract内で各ファイルに一回なので
	//SetOwnerWindowに使っちゃいます。
	//再利用性皆無になるけど面倒なので＾＾；
	bool CheckStr(const char *str ,bool c=true);
protected:
	//上書き確認用にオーバーライド
	static FILE* fopen(const char* file,const char* ac);
	//fpにNULLが渡ると面倒＆進歩状況にも使えるかも
	static size_t fwrite( void* buffer, size_t size, size_t count, FILE* fp );
	static size_t fwrite( const unsigned char * buffer, size_t size, size_t count, FILE* fp );
	static int fclose( FILE* fp );
};

#endif
