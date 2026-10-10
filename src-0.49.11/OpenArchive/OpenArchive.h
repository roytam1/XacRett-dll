#ifndef AFX_OPENARCHIVE_H__57AAB1DD_886F_4D25_860C_2AA917B02AC1__INCLUDED_
#define AFX_OPENARCHIVE_H__57AAB1DD_886F_4D25_860C_2AA917B02AC1__INCLUDED_

#include "..\kiutil.h"

#ifndef FNAME_MAX32
#define FNAME_MAX32 512
#endif

typedef	HGLOBAL	HARC;

#ifndef XACR_BUFSIZE
#define XACR_BUFSIZE (0x100000)
#endif

struct INDIVIDUALINFO {
	DWORD	dwOriginalSize;
	DWORD	dwCompressedSize;
	DWORD	dwCRC;
	UINT	uFlag;
	UINT	uOSType;
	WORD	wRatio;
	WORD	wDate;
	WORD	wTime;
	char	szFileName[FNAME_MAX32 + 1];
	char	dummy1[3];
	char	szAttribute[8];
	char	szMode[8];
};

class COpenArchive
{
public:
// 仮想関数 ////////////////

	virtual bool OpenArchive(LPCSTR szFileName);
	virtual bool FindFirst(INDIVIDUALINFO *lpSubInfo) = 0;
	virtual bool FindNext(INDIVIDUALINFO *lpSubInfo) = 0;
    bool CloseArchive();
// コンストラクタ＆デストラクタ ////

protected:
	COpenArchive()
		:m_pNext(NULL),fp(NULL){}
public:
	~COpenArchive()
	{
		if(m_pNext) delete m_pNext;
		if (fp)//開いたままだったら閉じる
			fclose(fp);
	}

// 作業用バッファ ///////
public:
	static unsigned char* common_buf;
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

	char szArcFileName[MAX_PATH];
protected:
	
    FILE *fp;//ファイルハンドル
	static char m_WildName[MAX_PATH];
public:
	static void SetWildName(LPCSTR s){lstrcpyn(m_WildName,s,MAX_PATH);}
	static LPCSTR GetWildName(){return m_WildName;}

// 簡易リスト ///////////

public:
	COpenArchive* GetNext()
		{return m_pNext;}
	COpenArchive* AddTail( COpenArchive* p )
		{
			if( !m_pNext )
				m_pNext=p;
			else
				m_pNext->AddTail(p);
			return p;
		}
private:
	COpenArchive* m_pNext;

};

#endif
