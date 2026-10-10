// stdafx.cpp : 標準インクルードファイルを含むソース ファイル
//              XacRett.pch 生成されるプリコンパイル済ヘッダー
//              stdafx.obj 生成されるプリコンパイル済タイプ情報

#include "stdafx.h"

// TODO: STDAFX.H に含まれていて、このファイルに記述されていない
// ヘッダーファイルを追加してください。

void LastErrorMsg()
{
	LPVOID lpMsgBuf;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,GetLastError(),
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // 既定の言語
					(LPTSTR) &lpMsgBuf,0,NULL);
	// 文字列を表示する。
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION);
	// バッファを解放する。
	LocalFree(lpMsgBuf);
}

