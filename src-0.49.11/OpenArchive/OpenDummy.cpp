#include "stdafx.h"
#include "OpenDummy.h"

bool COpenDummy::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	//COpenDummy::WildName = szWildName;
	return false;
}

bool COpenDummy::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}


//無駄な処理をしないようにオーバーロードしてる
bool COpenDummy::OpenArchive(LPCSTR szFileName)
{
	return false;
}