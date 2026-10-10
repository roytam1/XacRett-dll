#ifndef AFX_KIUTIL_H__D2E1F380_468E_11D3_8D94_ECF8CA9E4339__INCLUDED_
#define AFX_KIUTIL_H__D2E1F380_468E_11D3_8D94_ECF8CA9E4339__INCLUDED_

// ちょっと便利かも知れない関数群「kiutil」
//
// VC++ のツリーでひとまとめで表示させるためにクラス化
// しただけで、実は全部 public & static だったりする。(^^;

class kiutil
{
public:

// タイムスタンプ

	// WIN( 0.1 millisec from 1601.1.1 )
	static void timeSet( const char* fname, FILETIME* pft );
	// DOS( 0-4:day 5-8:month 9-15:year+1980, 0-4:sec/2 5-10:min 11-15:hour )
	static void timeSet( const char* fname, WORD date, WORD time );
	// UNIX( sec from 1970.1.1 )
	static void timeSet( const char* fname, DWORD sec );

// パス文字列

	// ↓最初に必ずコレを呼んでおくこと！
	static void pathInit();

	// 相対パスを与えると [不正文字除去],[複数階層makeDir],[..の__への変換] を行う
	static char* pathMake( char* path);
	static bool  m_nodir;//pathMakeで使用する
							//ディレクトリを無視するフラグ

	// 絶対パスを与えると [複数階層makeDir]
	static void pathMakeAbs_no_dtv_check( char* path );
	// yに最後の\, dに最後の. の位置を入れる
	static void pathSplit( const char* path, int* y, int* d );
	// 拡張子
	static const char* pathExt( const char* path );
	// ファイル名
	static const char* pathName( const char* path );

// window
	
	// 強制的に前面へ。( kazubon氏の TClock のソースより。感謝! )
	static void wndFront( HWND wnd );

// その他

	// fgets改訂版(CR/LF/CRLFを改行と認定。改行コードは含めない文字列を返す)
	static char* getline( char* str, int size, FILE* fp );
	// 拡張子変換
	static void getOriginalName( char* nw, char const * od,char* ext );

	//パスが有効なディレクトリであるか判断します。
	static bool pathIsDirectory( LPCTSTR lpPath );
	//ファイルが存在するか
	static bool pathExist( LPCTSTR lpFileName );
	//最後に\を付ける
	static LPTSTR pathAddBackslash( LPTSTR lpFileName );

	// ワイルドカードにマッチしているか
	// 第１引数にワイルドカードを含む文字列を指定する。
	static int StrMatch( const char *ptn, const char *str );

	static DWORD getfilesize(LPCSTR file,DWORD* hight=NULL);
// 変数

	static char lb[256];
};

#endif
