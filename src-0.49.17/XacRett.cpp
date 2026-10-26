// XacRett.DLL 
// K.INABA氏の作成したXacRettをDLL化しました。


// Tool.cpp,hをいじってあります。注意!!
//CLzhTool::FindHeaderもいじってあります。

// 必須ヘッダ
#include "stdafx.h"
#include "kiutil.h"
#include "resource.h"
#include <windows.h>
#include "XacRett.h"

#include "SplitCmdLine.h"//コマンドライン解析用
#include <map>  //ヘッダファイルを追加
#include <functional>
using namespace std;   //こいつを入れないと使えません。

#define RAR_3X 1
// 復元ルーチン
#include "Tool.h"

#include "LzhTool.h"
#include "ZipTool.h"
#include "ArjTool.h"
#ifndef RAR_3X
	# include "RarTool.h"
#else
	# include "Rar3Tool.h"
#endif
#include "AceTool.h"
#include "TarTool.h"
#include "GzTool.h"
#include "LzmaTool.h"
#include "XzTool.h"
#include "SevenZTool.h"
#include "ZstdTool.h"
#include "LzTool.h"
#include "Bz2Tool.h"
#include "CompressZTool.h"
#include "BgaTool.h"
#include "JakTool.h"
#include "IshTool.h"
#include "MimeTool.h"
#include "UnixDecoderTool.h"
#include "HqxTool.h"
#include "MSCompressTool.h"
#include "ZooTool.h"
#include "ArcTool.h"
#include "FrzTool.h"
#include "PackzTool.h"
#include "CompactCTool.h"
#include "BelTool.h"
#include "ArgTool.h"
#include "CptTool.h"
#include "ImpTool.h"
#include "PitTool.h"
#include "QPakTool.h"
#include "SplTool.h"
#include "BooTool.h"
#include "AtoBTool.h"
#include "SharTool.h"
#include "FishTool.h"
#include "RezTool.h"
#include "JamTool.h"
#include "AsdTool.h"
#include "ArTool.h"
#include "LbrTool.h"
#include "AinTool.h"
#include "CpioTool.h"
#include "ZacTool.h"
#include "MacBinTool.h"
#include "DzTool.h"
//#include "PmdTool.h"
#include "YncTool.h"
#include "DmgTool.h"

#include "IsoTool.h"//追加By Bonty
#include "BlzTool.h"//追加By Bonty
#include "AlzTool.h"//追加By Bonty
#include "EggTool.h"
#include "LzxTool.h"//追加By Bonty
#include "askTool.h"//追加By Bonty

#include "Yz1Tool.h"
#include "CabTool.h"
#include "GcaTool.h"
#include "IsTool.h"
#include "aPLibTool.h"//追加By Bonty
#include "SqxTool.h"//追加By Bonty(thanks for HAL)

//Open～系ルーチン  追加By Bonty
#include "OpenArchive\OpenArchive.h"

#include "OpenArchive\OpenZip.h"
#include "OpenArchive\OpenBel.h"
#include "OpenArchive\OpenCab.h"
#include "OpenArchive\OpenBga.h"
#include "OpenArchive\OpenMSCompress.h"
#include "OpenArchive\OpenZoo.h"
#include "OpenArchive\OpenGca.h"
#include "OpenArchive\OpenDz.h"
#include "OpenArchive\OpenArj.h"
#include "OpenArchive\OpenLzh.h"
#include "OpenArchive\OpenYz1.h"
#include "OpenArchive\OpenTar.h"
#include "OpenArchive\OpenBz2.h"
#include "OpenArchive\OpenGz.h"
#include "OpenArchive\OpenLzma.h"
#include "OpenArchive\OpenXz.h"
#include "OpenArchive\OpenSevenZ.h"
#include "OpenArchive\OpenZstd.h"
#include "OpenArchive\OpenLz.h"
#include "OpenArchive\OpenRez.h"
#include "OpenArchive\OpenJak.h"
#include "OpenArchive\OpenRar.h"
#include "OpenArchive\OpenAce.h"
#include "OpenArchive\OpenArc.h"
#include "OpenArchive\OpenArg.h"
#include "OpenArchive\OpenAsd.h"
#include "OpenArchive\OpenFish.h"
#include "OpenArchive\OpenFrz.h"
#include "OpenArchive\OpenJam.h"
#include "OpenArchive\OpenIsh.h"
#include "OpenArchive\OpenSpl.h"
#include "OpenArchive\OpenZac.h"
#include "OpenArchive\OpenHqx.h"
#include "OpenArchive\OpenCpt.h"
#include "OpenArchive\OpenMacBin.h"
#include "OpenArchive\OpenCpio.h"
#include "OpenArchive\OpenShar.h"
#include "OpenArchive\OpenAtoB.h"
#include "OpenArchive\OpenBoo.h"
#include "OpenArchive\OpenYnc.h"
#include "OpenArchive\OpenUnixDecoder.h"
#include "OpenArchive\OpenQPak.h"
#include "OpenArchive\OpenMime.h"
#include "OpenArchive\OpenCompactC.h"
#include "OpenArchive\OpenCompressZ.h"
#include "OpenArchive\OpenPackz.h"
#include "OpenArchive\OpenImp.h"
#include "OpenArchive\OpenAr.h"
#include "OpenArchive\OpenLbr.h"
#include "OpenArchive\OpenAin.h"
#include "OpenArchive\OpenIs.h"
#include "OpenArchive\OpenBlz.h"
#include "OpenArchive\OpenAlz.h"
#include "OpenArchive\OpenEgg.h"
#include "OpenArchive\OpenDmg.h"
#include "OpenArchive\OpenIso.h"
#include "OpenArchive\OpenLzx.h"
#include "OpenArchive\OpenaPLib.h"
#include "OpenArchive\OpenSqx.h"
#include "OpenArchive\OpenAsk.h"
#include "DgcTool.h"
#include "OpenArchive\OpenDgc.h"
#include "DgdTool.h"
#include "OpenArchive\OpenDgd.h"
#include "OpenArchive\OpenDummy.h"//未対応用のダミー

#ifndef _DEBUG
#include "Jc1Tool.h"//追加By Bonty
#include "OpenArchive\OpenJc1.h"
#endif

#if !defined(_MSC_VER) || (_MSC_VER >= 1200)
#include <shlwapi.h>
#endif

// グローバル変数
static CTool* g_pToolList = NULL;
static COpenArchive* g_pOpenArchiveList = NULL;//追加By Bonty
static std::map<CTool*,COpenArchive*,std::less<CTool*> > ArchiveListMap;

static int    g_nError[3] = {0,0,0};
enum { NOT_A_FILE, UNSUPPORTED, FAILED };


#define  WM_MYSTOP (WM_APP + 1)
#define DATALOADSIZE (65536*4)

static unsigned char* g_pWorkSpace = NULL;
static  HINSTANCE Instance = 0;//DLLハンドル
static bRunning = false;//動作中のフラグ

int ArcNameToNum(LPCSTR arckind );
int MeltArchive(LPCSTR lpArchiveName , LPCSTR lpBaseDir ,std::vector<std::string>* wildcard,CSplitCmdLine* scd);

void DllEnd(void);//後始末
void DllSub(void);

// shlwapi.dll is absent on stock Win95/NT4: load PathSetDlgItemPathA
// at runtime, fall back to the raw path text.
static void SetDlgItemPathCompat(HWND dlg, int id, LPCSTR path)
{
	typedef void (WINAPI *PSDPATHFUNC)(HWND, int, LPCSTR);
	HMODULE hSh = GetModuleHandleA("shlwapi.dll");
	if (hSh == NULL) {
		UINT oldErrMode = SetErrorMode(SEM_NOOPENFILEERRORBOX);
		hSh = LoadLibraryA("shlwapi.dll");
		SetErrorMode(oldErrMode);
	}
	PSDPATHFUNC pF = hSh == NULL ? NULL : (PSDPATHFUNC)GetProcAddress(hSh, "PathSetDlgItemPathA");
	if (pF != NULL) pF(dlg, id, path);
	else SetDlgItemTextA(dlg, id, path);
}

class CInfoDlg //状況表示ダイアログ
{
public:

	CInfoDlg( const char* fname, CTool* p )
		{
			// 状況表示ダイアログ用データ初期化
			m_bFlagEnd = true;
			m_Han = NULL;
			strcpy( m_FName, fname );
			strcpy( m_RName,"XacRett: ");
			strcat( m_RName, p->GetRoutineName() );
			p->InsTance = Instance;
			//m_Msg=RegisterWindowMessage(WM_ARCEXTRACT);
		}

	void Start()
		{
			// 終了フラグをオフ
			m_bFlagEnd = false;
			hWnd=0;

			// スレッド作成
			DWORD id;
			m_Han = CreateThread( NULL, 0, Thread, this,
									CREATE_SUSPENDED, &id);
			if( m_Han )
			{
				// 作成に成功したら、優先度を IDLE にしてスタート
				SetThreadPriority( m_Han, THREAD_PRIORITY_IDLE );
				ResumeThread( m_Han );
			}
			else
				// 失敗したら、終了フラグを立てておく
				m_bFlagEnd=true;

			//ウィンドウが作られるのを待つ
			while(!hWnd)
				Sleep(50);
		}

	~CInfoDlg()
		{
			// 終了フラグが立っていない場合
			if( !m_bFlagEnd )
			{
				// フラグを立てて
				m_bFlagEnd = true;

				// スレッド終了まで待機
				for( DWORD ec=STILL_ACTIVE; ec==STILL_ACTIVE; )
				{
					if( !GetExitCodeThread( m_Han, &ec ) )
						break;
					Sleep(50);
				}

				// ハンドル解放
				CloseHandle( m_Han );
			}
		}

private:
	char m_FName[300], m_RName[300];
	volatile bool m_bFlagEnd;
	volatile HANDLE m_Han;
public:
	static UINT	m_Msg;
	static HWND hWnd;
	static int	m_write;
private:
	static DWORD WINAPI Thread( void* param )
		{
			//window作成
			hWnd = CreateDialogParam( Instance,
										  MAKEINTRESOURCE(IDD_INFDLG),
										  NULL, (DLGPROC)Proc, (LPARAM)param );

			ShowWindow(hWnd, SW_SHOW);
			UpdateWindow(hWnd);

			// 終了フラグが立っていない間メッセージループ
			for( MSG msg;
				 !((CInfoDlg*)param)->m_bFlagEnd;
				 PeekMessage(&msg,0,0,0,PM_REMOVE) )
			{
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}

			// 終了フラグが立ったらスレッド終了
			DestroyWindow( hWnd );
			ExitThread( 0 );
			return 0;
		}
public:
	static BOOL CALLBACK Proc( HWND dlg, UINT msg, WPARAM wp, LPARAM lp )
		{
			static BOOL flgEND=FALSE;
			if( msg==WM_INITDIALOG )
			{
				flgEND=FALSE;
				CInfoDlg* p = (CInfoDlg*)lp;
				SetWindowLong( dlg,GWL_USERDATA,lp );
				SetWindowText( dlg,p->m_RName );
				//SetWindowText( GetDlgItem(dlg,IDC_FNAME), p->m_FName );
				SetDlgItemPathCompat(dlg,IDC_FNAME,p->m_FName);
				ShowWindow( dlg,SW_SHOW );
				UpdateWindow( dlg );
			}
			else if( msg==m_Msg )
			{
				EXTRACTINGINFO*	info=(EXTRACTINGINFO*)lp;
				switch (wp)
				{
					case ARCEXTRACT_OPEN:
					{
						SetDlgItemPathCompat(hWnd,IDC_ENAME,info->szSourceFileName);
						UpdateWindow( dlg );
						return flgEND;
					}
					case ARCEXTRACT_WRITEFILE_OPEN:
					{
						if(kiutil::pathExist(info->szSourceFileName))
						{
							if(m_write==1)//すべてハイ
								return 0;
							else if(m_write==2)//すべていいえ
								return 1;
							else
							{
								char msg[MAX_PATH*2];
								char fmt[MAX_PATH*2];
								if(0==LoadString(Instance,IDS_OVERWRITE_MSG,fmt,sizeof(fmt)))
									lstrcpy(fmt,"%s");
								wsprintf(msg,fmt,info->szSourceFileName);
								EnableWindow(hWnd,FALSE);
								char cap[MAX_PATH];
								if(0==LoadString(Instance,IDS_OVERWRITE_CAPTION,cap,sizeof(cap)))
									lstrcpy(cap,"XacRett");
								bool ret=MessageBox(hWnd,msg,cap,MB_YESNO)!=IDYES;
								EnableWindow(hWnd,TRUE);
								BringWindowToTop(hWnd);
								if(ret)
									return 1;
							}
						}
						return 0;
					}
				}

			}
			else if( (msg==WM_SYSCOMMAND && (wp==SC_CLOSE||wp==SC_DEFAULT))
					|| (msg==WM_COMMAND && LOWORD(wp)==IDCANCEL) )
			{
				flgEND=TRUE;
				return TRUE;
			}
			return FALSE;
		}
};
UINT CInfoDlg::m_Msg=NULL;
HWND CInfoDlg::hWnd=NULL;
int	CInfoDlg::m_write=NULL;

BOOL APIENTRY DllMain( HINSTANCE hInstance, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call){
	case DLL_PROCESS_ATTACH:
			bRunning = false;
			DllSub();
			Instance = hInstance;
			break;
	case DLL_PROCESS_DETACH:
			DllEnd();
			break;
	}
	return TRUE;
}

void DllSub(void)//ロードする
{
// 初期化
	kiutil::pathInit();
	CTool::AllocWorkSpace();

// 復元ルーチンロード
	CTool* t;//GetNext()をやってると効率悪いので
	COpenArchive* o;

	// 非圧縮系
	t=g_pToolList =         new CTarTool;
	o=g_pOpenArchiveList =  new COpenTar;
	ArchiveListMap[t]=o;

	t=t->AddTail( new CJakTool );
	o=o->AddTail( new COpenJak);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCpioTool );
	o=o->AddTail( new COpenCpio);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CQPakTool );
	o=o->AddTail( new COpenQPak);
	ArchiveListMap[t]=o;

	// わりとマトモ系( チェックが速いもの )
 	t=t->AddTail( new CYz1Tool );
	o=o->AddTail( new COpenYz1);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CTgzTool );
	o=o->AddTail( new COpenTgz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CTbzTool );
	o=o->AddTail( new COpenTbz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CTaZTool );
	o=o->AddTail( new COpenTaZ);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CTazTool );
	o=o->AddTail( new COpenTaz);
	ArchiveListMap[t]=o;

  	t=t->AddTail( new CIsTool );
	o=o->AddTail( new COpenIs);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CMSCompressTool );
	o=o->AddTail( new COpenMSCompress);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCpioGzTool );
	o=o->AddTail( new COpenCpioGz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCpioBzTool );
	o=o->AddTail( new COpenCpioBz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCpioZTool );
	o=o->AddTail( new COpenCpioZ);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CMacBinTool );
	o=o->AddTail( new COpenMacBin);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CDzTool );
	o=o->AddTail( new COpenDz);
	ArchiveListMap[t]=o;
	// わりとマトモ系( チェックが遅いもの )
	t=t->AddTail( new CBgaTool );
	o=o->AddTail( new COpenBga);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CLzhTool );
	o=o->AddTail( new COpenLzh);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CZipTool );
	o=o->AddTail( new COpenZip);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCabTool );
	o=o->AddTail( new COpenCab);
	ArchiveListMap[t]=o;

#ifndef RAR_3X
	t=t->AddTail( new CRarTool );
#else
	t=t->AddTail( new CRar3Tool );
#endif
	o=o->AddTail( new COpenRar);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CAceTool );
	o=o->AddTail( new COpenAce);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CGcaTool );
	o=o->AddTail( new COpenGca);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CHqxTool );
	o=o->AddTail( new COpenHqx);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CArjTool );
	o=o->AddTail( new COpenArj);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CIshTool );
	o=o->AddTail( new COpenIsh);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CImpTool );
	o=o->AddTail( new COpenImp);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CZooTool );
	o=o->AddTail( new COpenZoo);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCptTool );
	o=o->AddTail( new COpenCpt);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CPitTool );
	o=o->AddTail( new COpenDummy);//PIT
	ArchiveListMap[t]=o;

	t=t->AddTail( new CSqxTool );
	o=o->AddTail( new COpenSqx);
	ArchiveListMap[t]=o;

	// てきと～系
	t=t->AddTail( new CDmgTool );
	o=o->AddTail( new COpenDmg);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CIsoTool );
	o=o->AddTail( new COpenIso);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CGzTool );
	o=o->AddTail( new COpenGz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CTxzTool );
	o=o->AddTail( new COpenTxz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCpioXzTool );
	o=o->AddTail( new COpenCpioXz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CTzstTool );
	o=o->AddTail( new COpenTzst);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCpioZstTool );
	o=o->AddTail( new COpenCpioZst);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CZstdTool );
	o=o->AddTail( new COpenZstd);
	ArchiveListMap[t]=o;

	t=t->AddTail( new C7zTool );
	o=o->AddTail( new COpenSevenZ);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CLzTarTool );
	o=o->AddTail( new COpenTlz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CTlzmaTarTool );
	o=o->AddTail( new COpenTlzma);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCpioLzmaTool );
	o=o->AddTail( new COpenCpioLzma);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CLzTool );
	o=o->AddTail( new COpenLz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CBz2Tool );
	o=o->AddTail( new COpenBz2);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CLzmaTool );
	o=o->AddTail( new COpenLzma);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CXzTool );
	o=o->AddTail( new COpenXz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCompressZTool );
	o=o->AddTail( new COpenCompressZ);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CRezTool );
	o=o->AddTail( new COpenRez);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CArcTool );
	o=o->AddTail( new COpenArc);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CZacTool );
	o=o->AddTail( new COpenZac);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CAsdTool );
	o=o->AddTail( new COpenAsd);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CArgTool );
	o=o->AddTail( new COpenArg);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CPackzTool );
	o=o->AddTail( new COpenPackz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CFrzTool );
	o=o->AddTail( new COpenFrz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CCompactCTool );
	o=o->AddTail( new COpenCompactC);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CArTool );
	o=o->AddTail( new COpenAr);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CLbrTool );
	o=o->AddTail( new COpenLbr);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CAinTool );
	o=o->AddTail( new COpenAin);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CSplTool );
	o=o->AddTail( new COpenSpl);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CBlzTool );
	o=o->AddTail( new COpenBlz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CAlzTool );
	o=o->AddTail( new COpenAlz);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CEggTool );
	o=o->AddTail( new COpenEgg);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CLzxTool );
	o=o->AddTail( new COpenLzx);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CaPLibTool );
	o=o->AddTail( new COpenaPLib);
	ArchiveListMap[t]=o;

#ifndef _DEBUG
	t=t->AddTail( new CJc1Tool );
	o=o->AddTail( new COpenJc1);
	ArchiveListMap[t]=o;
#endif
//	t=t->AddTail( new CPmdTool );
	// ほぼ拡張子Only系
	t=t->AddTail( new CYncTool );
	o=o->AddTail( new COpenYnc);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CMimeTool );
	o=o->AddTail( new COpenMime);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CUueTool );
	o=o->AddTail( new COpenUue);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CXxeTool );
	o=o->AddTail( new COpenXxe);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CSharTool );
	o=o->AddTail( new COpenShar);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CAtoBTool );
	o=o->AddTail( new COpenAtoB);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CFishTool );
	o=o->AddTail( new COpenFish);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CBelTool );
	o=o->AddTail( new COpenBel);
	ArchiveListMap[t]=o;
	t=t->AddTail( new CJamTool );
	o=o->AddTail( new COpenJam);
	ArchiveListMap[t]=o;
	t=t->AddTail( new CBooTool );
	o=o->AddTail( new COpenBoo);
	ArchiveListMap[t]=o;
	t=t->AddTail( new CAskTool );
	o=o->AddTail( new COpenAsk);
	ArchiveListMap[t]=o;
	t=t->AddTail( new CDgcTool );
	o=o->AddTail( new COpenDgc);
	ArchiveListMap[t]=o;

	t=t->AddTail( new CDgdTool );
	o=o->AddTail( new COpenDgd);
	ArchiveListMap[t]=o;

	g_pToolList->Msg=RegisterWindowMessage(WM_ARCEXTRACT);
}
void DllEnd(void)//後始末
{
	delete g_pToolList;
	CTool::FreeWorkSpace();
	delete g_pOpenArchiveList;
	COpenArchive::FreeWorkSpace();
}

//統合アーカイバ仕様のAPI
extern "C" int __stdcall  XacRett(const HWND hWnd,LPCSTR szCmdLine,
								  LPSTR szOutput,const DWORD dwSize)
{
	bRunning = true;
	EnableWindow(hWnd,false);

	CSplitCmdLine scd;

	scd.Split(szCmdLine);

	// Optional non-interactive password (-p<password>). When absent
	// every provider still falls back to the modal dialog.
	CTool::SetPasswordArg( scd.m_password );

	int a = MeltArchive(scd.m_lpArcFileName,scd.m_lpBaseDirectory,&scd.m_wildcard,&scd);
	EnableWindow(hWnd,true);
	//BringWindowToTop(hWnd);
	bRunning = false;
	return a;
}

//バージョンを返します。
extern "C" WORD __stdcall XacRettGetVersion(VOID)
{
	return DLLVERSION;
}
extern "C" WORD __stdcall XacRettGetSubVersion(VOID)
{
	return SUBDLLVERSION;
}
//DLLが現在使用中かどうかを返します。
extern "C" BOOL __stdcall XacRettGetRunning(VOID)
{
	return bRunning;
}
//指定したアーカイブファイルの形式を得ます。
extern "C" int __stdcall  XacRettGetArchiveType(LPCSTR lpFileName)
{
	bRunning = true;
	//DllSub();
	//ここでチェック
	CTool* p = NULL;

	// ヘッダっぽいあたりを読込
	FILE* fp=fopen( lpFileName,"rb" );

	if( !fp )
	{
		g_nError[ NOT_A_FILE ]++;
		//DllEnd();
		bRunning = false;
		return 0;
	}

	int read = fread( CTool::common_buf,1,XACR_BUFSIZE,fp );
	fseek( fp,0,SEEK_END );
	int fsize = ftell( fp );
	fclose( fp );

	if( fsize>0 )
	{
		// まず拡張子で絞って書庫タイプを決定
		const char* ext=kiutil::pathExt( lpFileName );

		CTool *e=NULL;
		if( stricmp(ext,"exe") )
		{
			for( e=g_pToolList; e!=NULL; e=e->GetNext() )
			{
				if( e->IsType(ext) )
				{
					if( e->Check( lpFileName, fsize ) )
						p=e;
					break;
				}
			}
		}

		// それでだめなら全検索
		if( p==NULL )
		{
			for( p=g_pToolList; p!=NULL; p=p->GetNext() )
				if( p!=e && p->Check( lpFileName,fsize ) )
					break;
		}
	}

	if( p ){
		int tmp = ArcNameToNum(p->GetRoutineName());
		bRunning = false;
		return tmp;
	}
	else{
		bRunning = false;
		return 0;
	}
}

//指定ファイルがサポートしているアーカイブファイルとして
//正しいかどうかを返します。
extern "C" BOOL __stdcall XacRettCheckArchive(LPCSTR _szFileName, const int _iMode)
{
	if (XacRettGetArchiveType(_szFileName))
		return true;
	else
		return false;
}

extern "C" BOOL  __stdcall XacRettQueryFunctionList(const int _iFunction)
{
	switch (_iFunction)
	{
	case ISARC:
	case ISARC_GET_VERSION:
//	case ISARC_GET_CURSOR_INTERVAL:
//	case ISARC_SET_CURSOR_INTERVAL:
//	case ISARC_GET_BACK_GROUND_MODE:
//	case ISARC_SET_BACK_GROUND_MODE:
//	case ISARC_GET_CURSOR_MODE:
//	case ISARC_SET_CURSOR_MODE:
	case ISARC_GET_RUNNING:
		
	case ISARC_CHECK_ARCHIVE:
//	case ISARC_CONFIG_DIALOG:
//	case ISARC_GET_FILE_COUNT:
	case ISARC_QUERY_FUNCTION_LIST:
//	case ISARC_HOUT:
//	case ISARC_STRUCTOUT:
//	case ISARC_GET_ARC_FILE_INFO:
		
  	case ISARC_OPEN_ARCHIVE:
 	case ISARC_CLOSE_ARCHIVE:
 	case ISARC_FIND_FIRST:
 	case ISARC_FIND_NEXT:
//	case ISARC_EXTRACT:
//	case ISARC_ADD:
//	case ISARC_MOVE:
//	case ISARC_DELETE:
//	case ISARC_SETOWNERWINDOW:
//	case ISARC_CLEAROWNERWINDOW:
//	case ISARC_SETOWNERWINDOWEX:
//	case ISARC_KILLOWNERWINDOWEX:
		
//	case ISARC_GET_ARC_FILE_NAME:
//	case ISARC_GET_ARC_FILE_SIZE:
//	case ISARC_GET_ARC_ORIGINAL_SIZE:
//	case ISARC_GET_ARC_COMPRESSED_SIZE:
//	case ISARC_GET_ARC_RATIO:
//	case ISARC_GET_ARC_DATE:
//	case ISARC_GET_ARC_TIME:
//	case ISARC_GET_ARC_OS_TYPE:
//	case ISARC_GET_ARC_IS_SFX_FILE:
//	case ISARC_GET_ARC_WRITE_TIME_EX:
//	case ISARC_GET_ARC_CREATE_TIME_EX:
//	case ISARC_GET_ARC_ACCESS_TIME_EX:
//	case ISARC_GET_ARC_CREATE_TIME_EX2:
//	case ISARC_GET_ARC_WRITE_TIME_EX2:
//	case ISARC_GET_FILE_NAME:
//	case ISARC_GET_ORIGINAL_SIZE:
//	case ISARC_GET_COMPRESSED_SIZE:
//	case ISARC_GET_RATIO:
//	case ISARC_GET_DATE:
//	case ISARC_GET_TIME:
//	case ISARC_GET_CRC:
//	case ISARC_GET_ATTRIBUTE:
//	case ISARC_GET_OS_TYPE:
//	case ISARC_GET_METHOD:
//	case ISARC_GET_WRITE_TIME:
//	case ISARC_GET_CREATE_TIME:
//	case ISARC_GET_ACCESS_TIME:
//	case ISARC_GET_WRITE_TIME_EX:
//	case ISARC_GET_CREATE_TIME_EX:
//	case ISARC_GET_ACCESS_TIME_EX:
//	case ISARC_SET_ENUM_MEMBERS_PROC:
//	case ISARC_CLEAR_ENUM_MEMBERS_PROC:
//	case ISARC_GET_ARC_FILE_SIZE_EX:
//	case ISARC_GET_ARC_ORIGINAL_SIZE_EX:
//	case ISARC_GET_ARC_COMPRESSED_SIZE_EX:
//	case ISARC_GET_ORIGINAL_SIZE_EX:
//	case ISARC_GET_COMPRESSED_SIZE_EX:
//	case ISARC_SETOWNERWINDOWEX64:
//	case ISARC_KILLOWNERWINDOWEX64:
//	case ISARC_SET_ENUM_MEMBERS_PROC64:
//	case ISARC_CLEAR_ENUM_MEMBERS_PROC64:
//	case ISARC_OPEN_ARCHIVE2:
//	case ISARC_GET_ARC_READ_SIZE:
//	case ISARC_GET_ARC_READ_SIZE_EX:
		return TRUE;
	default:
		break;
	}
	return FALSE;
}

extern "C" HARC __stdcall  XacRettOpenArchive(const HWND _hwnd, LPCSTR _szFileName,const DWORD _dwMode)
{
	bRunning = true;
	//ここでチェック
	CTool* p = NULL;
	// ヘッダっぽいあたりを読込
	FILE* fp=fopen( _szFileName,"rb" );
	if( !fp )
	{
		bRunning = false;
		return 0;
	}
	int read = fread( CTool::common_buf,1,XACR_BUFSIZE,fp );
	fseek( fp,0,SEEK_END );
	int fsize = ftell( fp );
	fclose( fp );
	if( fsize>0 )
	{
		// まず拡張子で絞って書庫タイプを決定
		const char* ext=kiutil::pathExt( _szFileName );
		CTool *e=NULL;
		if( stricmp(ext,"exe") )
		{
			for( e=g_pToolList; e!=NULL; e=e->GetNext() )
			{
				if( e->IsType(ext) )
				{
					if( e->Check( _szFileName, fsize ) )
						p=e;
					break;
				}
			}
		}
		// それでだめなら全検索
		if( p==NULL )
		{
			for( p=g_pToolList; p!=NULL; p=p->GetNext() )
				if( p!=e && p->Check( _szFileName,fsize ) )
					break;
		}
	}
	COpenArchive::AllocWorkSpace();
	COpenArchive *e=ArchiveListMap[p];
	if(!e || !e->OpenArchive(_szFileName))
	{
		COpenArchive::FreeWorkSpace();
		bRunning = false;
		return NULL;
	}

	bRunning = false;
	return (HARC) p;

/*	int kind = 0;
	COpenArchive *e=NULL;

	bRunning = true;

	e=g_pOpenArchiveList;

	kind = XacRettGetArchiveType(_szFileName);
	if (kind == 0 || kind > NUM_ARCHIVETYPE)
	{//対応していない
		bRunning = false;
		return NULL;
	}
	COpenArchive::AllocWorkSpace();

	for ( int i = 0 ; i != kind-1 ;++i)
		e=e->GetNext();
	
	if(!e->OpenArchive(_szFileName))
	{
		bRunning = false;
		return NULL;
	}
	bRunning = false;
	return (HARC) e;//複数のハンドルに対応しないからこれでいいか^^;
	*/
}

extern "C" int  __stdcall  XacRettCloseArchive(HARC _harc)
{
	COpenArchive *e=ArchiveListMap[(CTool *)_harc];
	if(!e)return -1;
	bRunning = true;

	if(e->szArcFileName==NULL)
	{
		bRunning = false;
		return -1;
	}
	if (!e->CloseArchive())
	{
		bRunning = false;
		return -1;
	}
	COpenArchive::FreeWorkSpace();
	bRunning = false;
	return 0;
}

extern "C" int  __stdcall  XacRettFindNext(HARC _harc, INDIVIDUALINFO *_lpSubInfo)
{
	COpenArchive *e=ArchiveListMap[(CTool *)_harc];
	if(!e)return -1;
	bRunning = true;
	if(e->szArcFileName==NULL)
	{
		bRunning = false;
		return -1;
	}
	::memset(_lpSubInfo,0,sizeof(INDIVIDUALINFO));
	if(!e->FindNext(_lpSubInfo))
	{
		bRunning = false;
		return -1;
	}
	bRunning = false;
	if(kiutil::StrMatch(e->GetWildName(),_lpSubInfo->szFileName))
		return 0;
	else
		return XacRettFindNext(_harc,_lpSubInfo);
}
extern "C" int  __stdcall  XacRettFindFirst(HARC _harc, LPCSTR _szWildName,INDIVIDUALINFO *_lpSubInfo)
{
	COpenArchive *e=ArchiveListMap[(CTool *)_harc];
	if(!e)return -1;

	bRunning = true;
	if(e->szArcFileName==NULL)
	{
		bRunning = false;
		return -1;
	}
	::memset(_lpSubInfo,0,sizeof(INDIVIDUALINFO));
	e->SetWildName(_szWildName);
	if(!e->FindFirst(_lpSubInfo))
	{
		bRunning = false;
		return -1;
	}
	bRunning = false;
	if(kiutil::StrMatch(e->GetWildName(),_lpSubInfo->szFileName))
		return 0;
	else
		return XacRettFindNext(_harc,_lpSubInfo);
}
//解凍用ルーチン
int MeltArchive(LPCSTR lpArchiveName , LPCSTR lpBaseDir ,std::vector<std::string>* wildcard,CSplitCmdLine* scd)
{
	//DllSub();
	CTool* p = NULL;

	//ファイル名不指定
	if(!lpArchiveName || lstrlen(lpArchiveName)==0)
		return ERROR_CANNOT_READ;

	// ヘッダっぽいあたりを読込
	FILE* fp=fopen( lpArchiveName,"rb" );
	if( !fp )
	{
		g_nError[ NOT_A_FILE ]++;
		//ファイルが読み込めません
		return ERROR_CANNOT_READ;
	}

	int read = fread( CTool::common_buf,1,XACR_BUFSIZE,fp );
	fseek( fp,0,SEEK_END );
	int fsize = ftell( fp );
	fclose( fp );

	if( fsize>0 )
	{
		// まず拡張子で絞って書庫タイプを決定
		const char* ext=kiutil::pathExt( lpArchiveName );

		CTool *e=NULL;
		if( stricmp(ext,"exe") )
		{
			for( e=g_pToolList; e!=NULL; e=e->GetNext() )
			{
				if( e->IsType(ext) )
				{
					if( e->Check( lpArchiveName, fsize ) )
						p=e;
					break;
				}
			}
		}

		// それでだめなら全検索
		if( p==NULL )
		{
			for( p=g_pToolList; p!=NULL; p=p->GetNext() )
				if( p!=e && p->Check( lpArchiveName,fsize ) )
					break;
		}
	}

	if( p==NULL )
	{
		//対応形式ではない
		return ERROR_FILE_STYLE;
	}

	//終了フラグをOFFにしとく
	p->m_Endflg=false;

	char lpCurrentDirectory[MAX_PATH];
	::GetCurrentDirectory(MAX_PATH, lpCurrentDirectory);

	if(lpBaseDir && lstrlen(lpBaseDir))
		::SetCurrentDirectory(lpBaseDir);

	p->StrSet(wildcard);//ワイルドカード文字列をセット

// LongFileName取得
	WIN32_FIND_DATA find;
	HANDLE h = FindFirstFile( lpArchiveName,&find );
	if( h==INVALID_HANDLE_VALUE )
	{
		g_nError[ NOT_A_FILE ]++;
		//ファイルが読み込めません
		return ERROR_CANNOT_READ;
	}
	FindClose( h );
	// 状況表示ダイアログ準備
	CInfoDlg inf( find.cFileName, p );
	p->arcproc=inf.Proc;
	inf.m_write=scd->m_write;
	inf.m_Msg=p->Msg;
	if(scd->m_show)
	{
		inf.Start();
		p->hWnd=inf.hWnd;
	}
	kiutil::m_nodir=scd->m_nodir;
	//ここから解凍開始
	if( !p->Extract( lpArchiveName, lpBaseDir ) )
	{
		::SetCurrentDirectory(lpCurrentDirectory);
		//書き込みエラー
		//と言うことにしとく＾＾；
		//実際には壊れてたりもするはず。
		kiutil::m_nodir=false;
		return ERROR_CANNOT_WRITE;
	}
	::SetCurrentDirectory(lpCurrentDirectory);
	//終了フラグをOFFにしとく(念のため
	//p->m_Endflg=false;
	kiutil::m_nodir=false;
	return p->m_Endflg?ERROR_USER_CANCEL:0;
}

int ArcNameToNum(LPCSTR buffer )
{
	if ( lstrcmp(buffer,"Ace 展開")==0)
		return ARCHIVETYPE_ACE;
	else if ( lstrcmp(buffer,"Arc 展開")==0)
		return ARCHIVETYPE_ARC;
	else if ( lstrcmp(buffer,"Arg 展開")==0)
		return ARCHIVETYPE_ARG;
	else if ( lstrcmp(buffer,"Arj 展開")==0)
		return ARCHIVETYPE_ARJ;
	else if ( lstrcmp(buffer,"Deb/Ar 展開")==0)
		return ARCHIVETYPE_AR;
	else if ( lstrcmp(buffer,"Asd 展開")==0)
		return ARCHIVETYPE_ASD;
	else if ( lstrcmp(buffer,"Ascii to Binary 変換")==0)
		return ARCHIVETYPE_ATOB;
	else if ( lstrcmp(buffer,"Belon 展開")==0)
		return ARCHIVETYPE_BEL;
	else if ( lstrcmp(buffer,"Bza/Gza 展開")==0)
		return ARCHIVETYPE_BGA;
	else if ( lstrcmp(buffer,"Boo Decode")==0)
		return ARCHIVETYPE_BOO;
	else if ( lstrcmp(buffer,"Bzip2 展開")==0)
		return ARCHIVETYPE_BZ2;
	else if ( lstrcmp(buffer,"Compact 展開")==0)
		return ARCHIVETYPE_COMPACT;
	else if ( lstrcmp(buffer,"Compress 展開")==0)
		return ARCHIVETYPE_COMPRESSZ;
	else if ( lstrcmp(buffer,"Cpio 展開" )==0)
		return ARCHIVETYPE_CPIO;
	else if ( lstrcmp(buffer,"Cpio.gz/Rpm 展開" )==0)
		return ARCHIVETYPE_CPIOGZRPM;
	else if ( lstrcmp(buffer,"Cpio.Z 展開" )==0)
		return ARCHIVETYPE_CPIOZ;
	else if ( lstrcmp(buffer,"Cpio.bz2 展開")==0)
		return ARCHIVETYPE_CPIOBZ2;
	else if ( lstrcmp(buffer,"Cpio+LZMA 展開")==0)
		return ARCHIVETYPE_CPIOLZMA;
	else if ( lstrcmp(buffer,"Cpio+XZ 展開")==0)
		return ARCHIVETYPE_CPIOXZ;
	else if ( lstrcmp(buffer,"Cpio+ZSTD 展開")==0)
		return ARCHIVETYPE_CPIOZST;
	else if ( lstrcmp(buffer,"Cpt 展開")==0)
		return ARCHIVETYPE_CPT;
	else if ( lstrcmp(buffer,"Dzip 展開")==0)
		return ARCHIVETYPE_DZ;
	else if ( lstrcmp(buffer,"Fish decode")==0)
		return ARCHIVETYPE_FISH;
	else if ( lstrcmp(buffer,"Freeze 展開")==0)
		return ARCHIVETYPE_FRZ;
	else if ( lstrcmp(buffer,"Gzip 展開")==0)
		return ARCHIVETYPE_GZ;
	else if ( lstrcmp(buffer,"Tar+XZ 展開")==0)
		return ARCHIVETYPE_TXZ;
	else if ( lstrcmp(buffer,"Tar+LZIP 展開")==0)
		return ARCHIVETYPE_TLZ;
	else if ( lstrcmp(buffer,"Tar+LZMA 展開")==0)
		return ARCHIVETYPE_TLZMA;
	else if ( lstrcmp(buffer,"LZIP 展開")==0)
		return ARCHIVETYPE_LZIP;
	else if ( lstrcmp(buffer,"LZMA 展開")==0)
		return ARCHIVETYPE_LZMA;
	else if ( lstrcmp(buffer,"XZ 展開")==0)
		return ARCHIVETYPE_XZ;
	else if ( lstrcmp(buffer,"7z 展開")==0)
		return ARCHIVETYPE_7Z;
	else if ( lstrcmp(buffer,"Tar+ZSTD 展開")==0)
		return ARCHIVETYPE_TZST;
	else if ( lstrcmp(buffer,"ZSTD 展開")==0)
		return ARCHIVETYPE_ZSTD;
	else if ( lstrcmp(buffer,"BinHex Decode")==0)
		return ARCHIVETYPE_HQX;
	else if ( lstrcmp(buffer,"Imp 展開")==0)
		return ARCHIVETYPE_IMP;
	else if ( lstrcmp(buffer,"Ish Decode")==0)
		return ARCHIVETYPE_ISH;
	else if ( lstrcmp(buffer,"Jack 結合")==0)
		return ARCHIVETYPE_JAK;
	else if ( lstrcmp(buffer,"Jam May/96 展開")==0)
		return ARCHIVETYPE_JAM;
	else if ( lstrcmp(buffer,"Lzh 解凍")==0)
		return ARCHIVETYPE_LZH;
	else if ( lstrcmp(buffer,"MacBinary decode")==0)
		return ARCHIVETYPE_MACBIN;
	else if ( lstrcmp(buffer,"Base64 Decode")==0)
		return ARCHIVETYPE_MIME;
	else if ( lstrcmp(buffer,"MS-Compress 展開")==0)
		return ARCHIVETYPE_MSCOMPRESS;
	else if ( lstrcmp(buffer,"Pack 展開")==0)
		return ARCHIVETYPE_PACKZ;
	else if ( lstrcmp(buffer,"Pit 展開")==0)
		return ARCHIVETYPE_PIT;
	else if ( lstrcmp(buffer,"QuakePak/WAD/Pff 展開")==0)
		return ARCHIVETYPE_QPAC;
	else if ( lstrcmp(buffer,"Rar 展開")==0)
		return ARCHIVETYPE_RAR;
	else if ( lstrcmp(buffer,"Rez 結合")==0)
		return ARCHIVETYPE_REZ;
	else if ( lstrcmp(buffer,"Shar 展開")==0)
		return ARCHIVETYPE_SHAR;
	else if ( lstrcmp(buffer,"Splint 展開")==0)
		return ARCHIVETYPE_SPL;
	else if ( lstrcmp(buffer,"Tar 展開" )==0)
		return ARCHIVETYPE_TAR;
	else if ( lstrcmp(buffer,"Tar+Bzip2 展開" )==0)
		return ARCHIVETYPE_TBZ;
	else if ( lstrcmp(buffer,"Tar+Gzip 展開" )==0)
		return ARCHIVETYPE_TGZ;
	else if ( lstrcmp(buffer,"Tar+Compress 展開" )==0)
		return ARCHIVETYPE_TAZ;
	else if ( lstrcmp(buffer,"Tar+Pack 展開")==0)
		return ARCHIVETYPE_TAz;
	else if ( lstrcmp(buffer,"UUDecode")==0)
		return ARCHIVETYPE_UUDECODE;
	else if ( lstrcmp(buffer,"XXDecode")==0)
		return ARCHIVETYPE_XXDECODE;
	else if ( lstrcmp(buffer,"yDecode")==0)
		return ARCHIVETYPE_YNC;
	else if ( lstrcmp(buffer,"Zac 展開")==0)
		return ARCHIVETYPE_ZAC;
	else if ( lstrcmp(buffer,"Zip 展開")==0)
		return ARCHIVETYPE_ZIP;
	else if ( lstrcmp(buffer,"Zoo 展開")==0)
		return ARCHIVETYPE_ZOO;
	else if ( lstrcmp(buffer,"Cab 展開")==0)
		return ARCHIVETYPE_CAB;
	else if ( lstrcmp(buffer,"Gca 展開")==0)
		return ARCHIVETYPE_GCA;
	else if ( lstrcmp(buffer,"InstallShield-Cab 展開")==0)
		return ARCHIVETYPE_IS;
	else if ( lstrcmp(buffer,"Yz1 展開")==0)
		return ARCHIVETYPE_YZ1;
	else if (lstrcmp(buffer,"BriefLZ 展開")==0)
		return ARCHIVETYPE_BLZ;
	else if (lstrcmp(buffer,"Alz 展開")==0)
		return ARCHIVETYPE_ALZ;
	else if (lstrcmp(buffer,"Egg \x93W\x8aJ")==0)
		return ARCHIVETYPE_EGG;
	else if (lstrcmp(buffer,"Dmg 展開")==0)
		return ARCHIVETYPE_DMG;
	else if (lstrcmp(buffer,"Iso 展開")==0)
		return ARCHIVETYPE_ISO;
	else if (lstrcmp(buffer,"Lzx 展開")==0)
		return ARCHIVETYPE_LZX;
	else if (lstrcmp(buffer,"Jcalg1 展開")==0)
		return ARCHIVETYPE_JC1;
	else if (lstrcmp(buffer,"aPLib 展開")==0)
		return ARCHIVETYPE_APK;
	else if (lstrcmp(buffer,"Sqx 展開")==0)
		return ARCHIVETYPE_SQX;
	else if (lstrcmp(buffer,"Neo_ASK 展開")==0)
		return ARCHIVETYPE_ASK;
	else if (lstrcmp(buffer,"Ain 展開")==0)
		return ARCHIVETYPE_AIN;
	else if (lstrcmp(buffer,"Lbr 展開")==0)
		return ARCHIVETYPE_LBR;
	else if (lstrcmp(buffer,"Dgc 展開")==0)
		return ARCHIVETYPE_DGC;
	else if (lstrcmp(buffer,"Dgd 展開")==0)
		return ARCHIVETYPE_DGD;
	else
		return ARCHIVETYPE_UNKNOWN ;
}
