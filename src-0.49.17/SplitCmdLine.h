#ifndef AFX_SplitCmdLine_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_SplitCmdLine_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_

#include <vector>
#include <string>

class CSplitCmdLine  
{
public:
	char m_lpBaseDirectory[MAX_PATH];
	char m_lpArcFileName[MAX_PATH];
	bool m_show;//ダイアログの表示
	int m_write;//上書き確認
				//0 する
				//1 すべて"はい"としてしない
				//2 すべて"いいえ"としてしない
	bool m_nodir;//ディレクトリ無視

	std::vector<std::string> m_wildcard;
	BOOL Split(LPCTSTR lpCmdLine);
	CSplitCmdLine();
	virtual ~CSplitCmdLine();

protected:
	void Analysis(std::vector<LPCSTR>* arg);
private:
	BOOL IsBaseDir(LPCTSTR lpOption);
	BOOL FileExist(LPCTSTR lpOption);
	void ReadResponse(LPCSTR lpResponseName ,std::vector<std::string>* wildcard);
};

#endif