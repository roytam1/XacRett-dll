#ifndef __yz32lib_h__
#define __yz32lib_h__

// time_t
#if !defined(_USE_32BIT_TIME_T)
	#error "Please define _USE_32BIT_TIME_T"
#endif
#include <sys/types.h>
typedef time_t YZtime_t;

// STL
#ifdef WIN32
#pragma warning( disable : 4996 ) // _s
#pragma warning( disable : 4786 )
#pragma warning( disable : 4251 ) // string and vector<string> are not exported !
                                  // so, this static link version must be used
                                  // by the same version of compiler and library
                                  // with the DLL.
#endif // WIN32

#include <vector>
#include <string>
using std::string;
using std::vector;

typedef std::string              FileName ;  // ファイル名
typedef std::vector< FileName >  FileTbl  ;  // ファイル一覧

#ifndef YZ1DLLEXP
	#ifdef YZ1_EXPORTS
		#define YZ1DLLEXP __declspec(dllexport) 
	#else
		#define YZ1DLLEXP __declspec(dllimport) 
	#endif
#endif

//-------------------------------------------------------------------
// yzDecHead クラス

class yzFileEv;

class YZ1DLLEXP yzDecHead
{
    public: long        m_file_count ;           // ファイル数
    public: long *      m_file_size_tbl ;        // ファイルサイズ（複数）
    public: YZtime_t *  m_file_mtime_tbl ;       // 更新時刻（複数）
    public: FileTbl     m_file_name_tbl ;        // ファイル名（複数）
    public: int         m_file_name_max_length ; // ファイル名最大長
    public: long        m_file_size_max ;        // ファイルサイズ最大

    private: yzFileEv * yzm ;              // メッセージ出力先

    //-----------------------
    //  コンストラクタ
    public:  yzDecHead(
        const unsigned char *  in_area,      // in : ファイル一覧エリア
        const long &           in_file_cnt,  // in : ファイル数
        yzFileEv *             yzmev,        // in : メッセージ出力先
        const long &           in_size,      // kinaba :: in : in_areaのサイズ(バイト数)
	bool*                  success       // kinaba :: out : デコード成功した？
    );

    //-----------------------
    //  ディレクトリの内容とメモリの内容と比較
    public: int                             // out: 1:中断, 0:不一致あり(続行), -1:不一致無し(続行)
    MemChk(
        const char *           in_dir_name, // in : ディレクトリ名
        const int &            in_n,        // in : n 番目
        const unsigned char *  in_mem       // in : 比較メモリ
    );

    //-----------------------
    //  ディレクトリの内容を表示
    public: void  Print();

    //-----------------------
    //  メモリ内容をファイルとして出力
    int                                     // out: 0:OK, !0:失敗
    FileWrite(                              // ---:
        const char *           in_dir_name, // in : カレントディレクトリ
        const int &            in_n,        // in : 番号
        const unsigned char *  in_data      // in : 出力データ（展開出データ）
    );
};


//-------------------------------------------------------------------
// yzFileStat クラス

class YZ1DLLEXP yzFileStat
{
    public: string  m_file_name ;        // ファイル名
    public: long    m_file_size ;        // ファイルサイズ
    public: time_t  m_file_mtime ;       // 更新時刻

    public: yzFileStat(
        string  in_file_name,        // ファイル名
        long    in_file_size,        // ファイルサイズ
        time_t  in_file_mtime        // 更新時刻
    );
};


//-------------------------------------------------------------------
// yzDecodeHeader クラス

class YZ1DLLEXP yzDecodeHeader
{
    private : yzDecHead &  m_yz_dec_head ;

    // コンストラクタ
    public : yzDecodeHeader(yzDecHead & in_yz_dec_head );

    public : vector< yzFileStat >  filestat_Get();
};


//-------------------------------------------------------------------
//  yzFileEv クラス（ yzFileEncode/yzFileDecode からのイベントクラス）

class YZ1DLLEXP yzFileEv
{
    //-----------------------
    //  [!]メッセージ出力
    public: virtual void           // out: 無し
    MessEX(                        // ---: --------------
        const char *  in_str,      // in : 出力メッセージ
        const int &   in_msg_no    // in : メッセージ識別番号
    ) = 0 ;

    //-----------------------
    //  [i]メッセージ出力
    public: virtual void           // out: 無し
    MessINF(                       // ---: --------------
        const char *  in_str,      // in : 出力メッセージ
        const int &   in_msg_no    // in : メッセージ識別番号
    ) = 0 ;

    //-----------------------
    //  [?]メッセージ出力
    public: virtual int            // out: 1:no, 0:yes
    MessYN(                        // ---: --------------
        const char *  in_str,      // in : 出力メッセージ
        const int &   in_msg_no    // in : メッセージ識別番号
    ) = 0 ;

    //-----------------------
    //  [INPUT]メッセージ出力
    public: virtual char *         // out: 入力文字列
    MessINPUT(                     // ---: --------------
        const char *  in_str,      // in : 出力メッセージ
        const int &   in_msg_no    // in : メッセージ識別番号
     ) = 0 ;

    //-----------------------
    //  Gauge 初期化
    public: virtual int                     // out: 0:続行, !0:中断
    gauge_EncodeInit(                       // ---: --------------
        const char *   in_input_file_name,  // in : 入力ファイル（ディレクトリ）名（フルパス）
        FileName &     output_name,         // in/out: 出力ファイル名（置き換え可能）
        FileTbl &      fullpath_list,       // in/out: フルパスファイル名一覧（置き換え可能）
        FileTbl &      file_list            // in/out: 相対ファイル名一覧（置き換え可能）
    ) = 0 ;

    //-----------------------
    //  Gauge 初期化
    public: virtual int                        // out: 0:続行, !0:中断
    gauge_DecodeInit(                          // ---: --------------
        const char *            in_file_name,  // in : 解凍するファイル名
        const yzDecodeHeader &  in_yz_dec_head // in : 展開情報
    ) = 0 ;

    //-----------------------
    //  Gauge ゲージの文字列表示
    public: virtual int            // out: 0:続行, !0:中断
    gauge_String(                  // ---: --------------
        const char *  in_str       // in : メッセージ
    ) = 0 ;

    //-----------------------
    //  Gauge ゲージの値表示
    public: virtual int            // out: 0:続行, !0:中断
    gauge_Value(                   // ---: --------------
        const int &  in_per,       // in : 経過割合 パーセント 0-1000
        const int &  in_end_time   // in : 終了予想時刻 1/100 秒後
    ) = 0 ;

    //-----------------------
    //  Gauge ファイル出力直前情報
    public: virtual int                 // out: 0:出力続行, 1:skip(出力せず続行), 2:中断
    gauge_WriteInfo(                    // ---: --------------
        const yzFileStat &  in_yzfstat, // in : ファイル詳細情報
        const int &         in_no,      // in : 処理中のファイル番号（0～）
        const int &         in_max      // in : 全ファイル数
    ) = 0 ;
};


//-------------------------------------------------------------------

class YZ1DLLEXP YzMessage
{
    //-----------------------
    // メッセージ言語（日本語/英語）
    public: enum language_type
    {
        language_japanese = 1, // 日本語
        language_english  = 2, // 英語
    } ;

    public: static language_type  language ;
};

#include "YZ_MsgTbl.h"


//-------------------------------------------------------------------

enum YZ_MAIN_ENCODE_PASSWD_MODE    // パスワードの使用
{
    YZ_MAIN_ENCODE_PASSWD_OFF = 0, // パスワードＯＦＦ
    YZ_MAIN_ENCODE_PASSWD_ON  = 1, // パスワードＯＮ
    YZ_MAIN_ENCODE_PASSWD_PUB = 2  // 公開形式パスワードＯＮ
} ;


//-------------------------------------------------------------------
//  yzFileEncode クラス

class YZ1DLLEXP yzFileEncode
{
    public:  FileName          m_output_name ;  // 圧縮後の出力ファイル名

    private: yzFileEv *        yzm ;            // メッセージ出力先
    private: const char *      dirName ;        // 圧縮ファイル出力ディレクトリ
    private: const char *      SelfEncodeFile ; // 自己解凍ファイル作成
    private: const enum YZ_MAIN_ENCODE_PASSWD_MODE  passwd_mode ; // パスワードの使用

    //-----------------------
    //  コンストラクタ
    public: yzFileEncode(
        yzFileEv *    in_yzm,                     // in : メッセージ出力先
        const char *  in_dir_name         = NULL, // in : 圧縮ファイル出力ディレクトリ
        const char *  in_self_encode_file = NULL, // in : 自己解凍ファイル作成
        const enum YZ_MAIN_ENCODE_PASSWD_MODE  in_passwd_mode = // in : パスワードの使用
             YZ_MAIN_ENCODE_PASSWD_OFF                          //      パスワードＯＦＦ
    );

    //-------------------------------------------------------------------
    //  「圧縮」  -1:キャンセル, 0以上:正常終了
    public:    int                   // out: -1:キャンセル, 0以上:正常終了
    EncodeFile(
        const char *  in_file_name   // in : 圧縮するファイル名
    );
};


//-------------------------------------------------------------------
//  yzFileDecode クラス

class MyBitGetEv ;  // 圧縮データ読み込み
class MemAlloc ;    // 圧縮データのヘッダ読み込みエリア
class yzDecode ;    // 圧縮データの解凍

class YZ1DLLEXP yzFileDecode
{
friend class yzDLLArcHandler;		// 勝手に改造 by K.INABA
friend class yzDLLArcHeaderHandler;	// 勝手に改造 by K.INABA

    public:  char *       yzfiledecode_output_dir_name ; // 伸長先、ディレクトリ名
    public:  yzDecHead *  yzfiledecode_p_yz_dec_head ;   // 圧縮データのヘッダ情報

    private: yzFileEv *   yzm ;        // メッセージ出力先

    private: MyBitGetEv *  p_bit_get ;  // 圧縮データ読み込み
    private: MemAlloc *    p_head ;     // 圧縮データのヘッダ読み込みエリア
    private: yzDecode *    p_yz_decode ;// 圧縮データの解凍
    private: long  dec_all_size   ; // 2: AllSize(4)
    private: long  dec_head_size  ; // 3: HeaderSize(4)
    private: long  dec_file_count ; // 4: FileCount(4)
    private: long  dec_data_size  ;


	//-------------------------------------------------------------------
	//  コンストラクタ
	public: yzFileDecode(
	    yzFileEv *    in_yzm,        // in : メッセージ出力先
	    const char *  in_dir_name    // in : 伸長先、ディレクトリ名
	);

	//-------------------------------------------------------------------
	//  デストラクタ
	// デストラクタが必要な class はコピーも禁止する。
	private: yzFileDecode            (const yzFileDecode&) ; // オブジェクトコピー禁止
	private: yzFileDecode & operator=(const yzFileDecode&) ; // オブジェクトコピー禁止
	public: ~yzFileDecode();

	//-------------------------------------------------------------------
	//  YZファイルのヘッダ情報のみ解凍
	public: int                              // out:-1:キャンセル, 0以上:正常終了
	YzFile_DecodeHeader(
	    const char *  in_file_name   // in : 伸長するファイル名
	);

	//-------------------------------------------------------------------
	//  YZファイルの内容表示
	public: int                      // out: -1:キャンセル, 0以上:正常終了
	Print(                           // ---: ----------------------------
	    const char *  fileName       // in : 伸長するファイル名
	);

	//-------------------------------------------------------------------
	//  YZファイルの解凍
	public: int                      // out: -1:キャンセル, 0以上:正常終了
	DecodeFile(                      // ---: ----------------------------
	    const char *  fileName,      // in : 伸長するファイル名
	    const int &   in_decode_mode // in : 伸長モード 0:書き込む、1:書き込まない（解凍テスト）
	);
};

#ifdef WIN32
#pragma warning( default : 4786 )
#pragma warning( default : 4251 )
#endif // WIN32
#endif // __yz32lib_h__
