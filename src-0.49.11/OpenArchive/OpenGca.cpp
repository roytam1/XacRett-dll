#include "stdafx.h"
#include "OpenGca.h"

//sing namespace std;
//using namespace nsGcaSDK;

bool COpenGca::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	gca.SetArcFilePath(COpenGca::szArcFileName);
	COpenGca::FileNum = gca.GetNumFiles();

	return COpenGca::set_Find(lpSubInfo);
}

bool COpenGca::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenGca::set_Find(lpSubInfo);
}

bool COpenGca::set_Find(INDIVIDUALINFO *lpSubInfo)
{
	static int i=-1;
	string filename;

	if (++i==COpenGca::FileNum)
	{
		i = -1;
		return false;
	}

	filename = gca.GetFileName(i);
	lpSubInfo->dwCompressedSize = gca.GetDataSize(i);
	lpSubInfo->dwCRC = gca.GetFileCRC(i);
	lpSubInfo->dwOriginalSize = gca.GetFileSize(i);
	lstrcpy(lpSubInfo->szFileName,filename.c_str());

	DWORD dAttributes=gca.GetFileAttributes(i);

	lstrcpy(lpSubInfo->szAttribute ,"----W");
	if (dAttributes & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[4] = 'R';
	if (dAttributes & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[2] = 'H';
	if (dAttributes & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[3] = 'S';
	if (dAttributes & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[1] = 'D';
	if (dAttributes & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[0] = 'A';

	FILETIME ftm;
	ftm = gca.GetFileTime(i);
	FileTimeToDosDateTime(&ftm,&lpSubInfo->wDate,&lpSubInfo->wTime);

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	//lstrcpy(lpSubInfo->szMode,"");
	return true;

}