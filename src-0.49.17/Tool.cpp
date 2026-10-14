// tool.cpp
// 　復元ルーチンの共通インターフェイスクラス。

/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/

#include "stdafx.h"
#include "resource.h"
#include "Tool.h"
#include "Windows.h"
#include "XacRett.h"

unsigned char* CTool::common_buf;
bool m_bFlagEnd = false;
ARCHIVERPROC CTool::arcproc=NULL;
char* CTool::m_pPassArg=NULL;
HWND			CTool::hWnd=NULL;
UINT			CTool::Msg=0;
bool			CTool::m_Endflg=false;

void CTool::SetPasswordArg(const char* pw)
{
	delete [] m_pPassArg;
	m_pPassArg = NULL;
	if( pw && *pw )
		{
		m_pPassArg = new char[lstrlen(pw) + 1];
		lstrcpy( m_pPassArg, pw );
		}
}

char* CTool::GetPassword()
{
	if( m_pPassArg && *m_pPassArg )
		{
		// -p was given: use it and never show the dialog.
		delete [] m_pPass;
		m_pPass = new char[lstrlen(m_pPassArg) + 1];
		lstrcpy( m_pPass, m_pPassArg );
		return m_pPass;
		}

	delete [] m_pPass;

	DialogBoxParam( InsTance, MAKEINTRESOURCE(IDD_PASS),
					NULL,(DLGPROC)PassProc, (LPARAM)(&m_pPass) );

	return m_pPass;


}

BOOL CALLBACK CTool::PassProc( HWND dlg,UINT msg,WPARAM wp,LPARAM lp )
{
	if( msg==WM_INITDIALOG )
	{
		SetWindowLong( dlg, GWL_USERDATA, lp );
		SendDlgItemMessage( dlg, IDC_MASK, BM_SETCHECK, BST_CHECKED, 0 );
		SetFocus( GetDlgItem(dlg,IDC_PASSWORD) );
		kiutil::wndFront( dlg );
		return TRUE;
	}
	else if( msg==WM_COMMAND )
	{
		if( LOWORD(wp)==IDOK )
		{
			int len = SendDlgItemMessage( dlg,IDC_PASSWORD,
										WM_GETTEXTLENGTH,0,0);

			char** ppPass=(char**)GetWindowLong(dlg,GWL_USERDATA);
			*ppPass=new char[1+len];

			SendDlgItemMessage(dlg,IDC_PASSWORD,WM_GETTEXT,
									len+1,(LPARAM)*ppPass);
			EndDialog(dlg,IDOK);
			return TRUE;
		}
		else if( LOWORD(wp)==IDC_MASK )
		{
			if( BST_CHECKED==
					SendDlgItemMessage( dlg,IDC_MASK,
										BM_GETCHECK,0,0 ) )
				SendDlgItemMessage( dlg,IDC_PASSWORD,
									EM_SETPASSWORDCHAR,'*',0 );
			else
				SendDlgItemMessage( dlg,IDC_PASSWORD,
									EM_SETPASSWORDCHAR,0,0 );
			InvalidateRect( GetDlgItem(dlg,IDC_PASSWORD),
												NULL,TRUE );
			return TRUE;
		}
	}
	return FALSE;
}

//StrMatchを使ってそれに合っているかどうかチェック
//これ使うのExtract内で各ファイルに一回なので
//SetOwnerWindowに使っちゃいます。
//再利用性皆無になるけど面倒なので＾＾；
//
//bool cはチェックするかどうか
//本来ならチェックするための関数なのだが
//SetOwnerWindowに使うためこのようなフラグを付ける事に＾＾；
//すべてのtoolで一部展開が実装されたら消えます。
bool CTool::CheckStr(const char *str ,bool c)
{
	
	
	bool result=false;

	//ここが件のc
	if(!c)
		result=true;
	else if(wildcard[0].size()==0)
		result=true;
	else
	{
		for(std::vector<std::string>::iterator it = wildcard->begin();it != wildcard->end();it++)
		{
			if(kiutil::StrMatch((*it).c_str(),str))
			{
				result=true;
				break;
			}
		}
	}

	//SetOwnerWindowのための処理
	if(result && arcproc)
	{
		static EXTRACTINGINFO	info;
		::memset(&info,0,sizeof(EXTRACTINGINFO));
		lstrcpy(info.szSourceFileName,str);
		if(arcproc(hWnd,Msg,ARCEXTRACT_OPEN,(WPARAM)&info))
			m_Endflg=true;
	}

	return result;
}

FILE* CTool::fopen(const char* file,const char* ac)
{
	if(lstrlen(file)==0)
		return NULL;
	if(kiutil::pathIsDirectory(file))
		return NULL;
	if(lstrcmp(ac,"rb")==0)//読み取りなら普通に
	{
		return ::fopen(file,ac);
	}else
	{
		//SetOwnerWindowのための処理
		if(arcproc)
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,file);
			if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
				return NULL;
		}
		return ::fopen(file,ac);
	}
}
size_t CTool::fwrite( void* buffer, size_t size, size_t s_count, FILE* fp )
{
	if(fp)
		return ::fwrite(buffer,size,s_count,fp);
	else//書き込んだ気になって^^;
		return s_count;
}
size_t CTool::fwrite( const unsigned char * buffer, size_t size, size_t s_count, FILE* fp )
{
	return fwrite((void*)buffer,size,s_count,fp);
}
int CTool::fclose( FILE* fp )
{
	return fp?::fclose(fp):EOF;
}