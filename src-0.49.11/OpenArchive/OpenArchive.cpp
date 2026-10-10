#include "stdafx.h"
#include "OpenArchive.h"

unsigned char* COpenArchive::common_buf=NULL;
char COpenArchive::m_WildName[MAX_PATH]={0};

bool COpenArchive::OpenArchive(LPCSTR szFileName)
{
	if (COpenArchive::fp){//他のファイルが開かれているときにはエラー
		return false;
	}
	COpenArchive::fp = fopen(szFileName, "rb");
	if (!COpenArchive::fp)
		return false;//開くの失敗
	lstrcpy(COpenArchive::szArcFileName,szFileName);
	size_t n = fread(COpenArchive::common_buf,1,XACR_BUFSIZE,COpenArchive::fp);
	fseek( COpenArchive::fp,0,SEEK_SET );

	return true;
}

bool COpenArchive::CloseArchive()
{
	if (COpenArchive::fp)
	{//閉じる
		bool ret=(fclose(COpenArchive::fp)==EOF);
		COpenArchive::fp=NULL;
		return ret;
	}else{
		return false;
	}
}