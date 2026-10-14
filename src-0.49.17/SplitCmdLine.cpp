#include "stdafx.h"
#include "SplitCmdLine.h"

CSplitCmdLine::CSplitCmdLine()
:m_show(true)
,m_write(0),m_nodir(false)
{
	m_lpBaseDirectory[0] = NULL;
	m_lpArcFileName[0] = NULL;
	m_password[0] = 0;
}

CSplitCmdLine::~CSplitCmdLine()
{
}
BOOL CSplitCmdLine::Split(LPCTSTR _lpCmdLine)
{
	// コマンドライン分割
	char* lpCmdLine=new char[lstrlen(_lpCmdLine)+1];
	lstrcpy(lpCmdLine,_lpCmdLine);

	char* p=lpCmdLine;
	char endc;
	bool first = true;
	std::vector<LPCSTR> arg;

	while( *p!='\0' )
	{
		// 余分な空白はスキップ
		while( *p==' ' ) //|| *p=='\t' || *p=='\r' || *p=='\n' )
			p++;

		// " だったら、その旨記録してさらに一個進める
		if( *p=='"' )
			endc='"', p++;
		else
			endc=' ';

		// end-of-text なら終了
		if( *p=='\0' )
			break;

		if( first)
		{
			first = false;
		}
		else
		{
			// 引数を保存
			arg.push_back( p );
		}

		// 引数の終わりへ…
		while( *p!=endc && *p!='\0' )
			p++;

		// 終わりは'\0'にすることによって、引数を区切る
		if( *p!='\0' )
			*(p++) = '\0';
	}
	Analysis(&arg);
	delete[] lpCmdLine;
	return TRUE;
}
// A token is a switch when it starts with '-', or with '/' followed by
// one of our option letters. Only those: a bare '/' must stay a path, or
// absolute paths like /tmp/x would be swallowed as an unknown switch.
static BOOL IsSwitchToken(LPCSTR s)
{
	if( s[0] == '-' )
		return TRUE;
	if( s[0] == '/' )
		{
		const char* q = "nNoOjJpP";
		for( ; *q; q++ )
			if( s[1] == *q ) return TRUE;
		}
	return FALSE;
}

void CSplitCmdLine::Analysis(std::vector<LPCSTR>* arg)
{
	std::vector<LPCSTR>::iterator it;
	for (it = arg->begin(); it!=arg->end(); it++)
	{
		if( IsSwitchToken(*it) )
		{
			switch((*it)[1])
			{
			case 'n':
			case 'N':
				if((*it)[2]=='0')
					m_show=true;
				else
					m_show=false;
				break;
			case 'o':
			case 'O':
				if((*it)[2]=='1')
					m_write=1;
				else if((*it)[2]=='2')
					m_write=2;
				else
					m_write=0;
				
				break;
			case 'j':
			case 'J':
				m_nodir=true;
				break;
			case 'p':
			case 'P':
				// Non-interactive password. Anything after the option letter
				// is the password, so -psecret and -p secret both work
				// (the value only has to start with the same letter).
				{
				LPCSTR v = (*it) + 2;
				lstrcpyn( m_password, *v ? v : "", sizeof(m_password) );
				break;
				}
			}
		}
		else if (FileExist((*it)) && !m_lpArcFileName[0])
		{
			//書庫ファイル設定
			lstrcpy(m_lpArcFileName,(*it));
		}
		else if (IsBaseDir((*it)) && !m_lpBaseDirectory[0])
		{
			// 基準ディレクトリ設定
			lstrcpy(m_lpBaseDirectory,(*it));
		}
		else//wildcard
		{
			if((*it)[0]=='@')
				ReadResponse(&(*it)[1],&m_wildcard);
			else
				m_wildcard.push_back((*it));
		}
	}
	if(!m_wildcard.size())
		m_wildcard.push_back("*");
}
// 基準ディレクトリか判定
BOOL CSplitCmdLine::IsBaseDir(LPCTSTR lpOption)
{
	if (*lpOption == '-' || *lpOption == '/')
		return FALSE;

	while (*lpOption)
		++lpOption;
	--lpOption;
	if (*lpOption == '\\')
		return TRUE;

	return FALSE;
}

BOOL CSplitCmdLine::FileExist(LPCTSTR lpOption)
{
	// LongFileName取得
	WIN32_FIND_DATA find;
	HANDLE h = FindFirstFile( lpOption,&find );
	if( h==INVALID_HANDLE_VALUE )
	{
		return false;
	}
	FindClose( h );
	return true;
}
//レスポンスファイルを読み込んでwildcardにpush_backする
void CSplitCmdLine::ReadResponse(LPCSTR lpResponseName ,std::vector<std::string>* wildcard)
{
	HANDLE hFile=::CreateFile(lpResponseName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
	if (hFile==INVALID_HANDLE_VALUE)
		return;

	DWORD dwrdSize,dwSize=GetFileSize(hFile,NULL);
	if (!dwSize)
	{
		::CloseHandle(hFile);
		return;
	}
	char*buff=new char[dwSize+2];
	memset(buff,0,dwSize+1);
	std::string str;
	std::vector<LPCSTR> vec;
	if (::ReadFile(hFile,buff,dwSize,&dwrdSize,NULL)&&(dwSize==dwrdSize))
	{
		//まずvecに一時的に格納
		char endc;
		char* p=buff;
		while( *p!='\0' )
		{
			// 余分な空白はスキップ
			while( *p==' ' || *p=='\r' || *p=='\n' )
				p++;

			// " だったら、その旨記録してさらに一個進める
			if( *p=='"' )
				endc='"', p++;
			else
				endc=' ';

			// end-of-text なら終了
			if( *p=='\0' )
				break;

			vec.push_back(p);

			// 引数の終わりへ…
			while( *p!=endc && *p!='\0' && *p!='\r' && *p!='\n')
				p++;

			// 終わりは'\0'にすることによって、引数を区切る
			if( *p!='\0' )
				*(p++) = '\0';
		}
		//vecからwildcardに移す
		for(std::vector<LPCSTR>::iterator it=vec.begin();it!=vec.end();it++)
		{
			str=*it;
			wildcard->push_back(str);
		}
	}
	delete[] buff;
	::CloseHandle(hFile);
}