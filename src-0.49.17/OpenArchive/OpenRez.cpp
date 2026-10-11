#include "stdafx.h"
#include "OpenRez.h"

//この場合オリジナルの情報を返す
bool COpenRez::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	fseek(COpenRez::fp,4,SEEK_SET);

	if (1!=fread(&DevFileNum,4,1,COpenRez::fp))//分割数
		return false;

	char CommentLen[2];//コメントは飛ばす
	if (2 !=fread(CommentLen,1,2,COpenRez::fp))
		return false;
	fseek(COpenRez::fp,ftell(COpenRez::fp)+CommentLen[0]+CommentLen[1]*0xFF,SEEK_SET);

	char FileNameLen[2];
	if (2 !=fread(FileNameLen,1,2,COpenRez::fp))
		return false;
	if (FileNameLen[0] + FileNameLen[1]*0xFF !=fread(lpSubInfo->szFileName,1,FileNameLen[0] + FileNameLen[1]*0xFF,COpenRez::fp))
		return false;
	char FileSize[6];
	if ( 6 != fread(FileSize,1,6,COpenRez::fp))
		return false;

	if (!FileSize[5] || !FileSize[4]){//オーバーフローする可能性があるのでFileSize[4,5]は無視^^;
		lpSubInfo->dwOriginalSize = FileSize[0]+FileSize[1]*0xFF+FileSize[2]*0xFFFF+FileSize[3]*0xFFFFFF;
	}else{
		lpSubInfo->dwOriginalSize =0xFFFFFFFF;
	}
	//lstrcpy(lpSubInfo->szMode,"");

	return true;
}
//分割ファイルの情報を返す
bool COpenRez::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	static int i=0;

	if (++i==COpenRez::DevFileNum)
		i=0;
		return false;

	char FileNameLen[2];
	if (2!=fread(FileNameLen,1,2,COpenRez::fp))
		i=0;
		return false;
	if (FileNameLen[0] + FileNameLen[1]*0xFF !=fread(lpSubInfo->szFileName,1,FileNameLen[0] + FileNameLen[1]*0xFF ,COpenRez::fp))
		i=0;
		return false;

	lpSubInfo->szFileName[FileNameLen[0] + FileNameLen[1]*0xFF]=0;
	if (1 !=fread(&lpSubInfo->dwCompressedSize,4,1,COpenRez::fp))
		i=0;
		return false;
	WORD crc16=0;
	if (1 !=fread(&crc16,2,1,COpenRez::fp))
		i=0;
		return false;
	lpSubInfo->dwCRC = (DWORD)crc16;

	WIN32_FIND_DATA find;
	char FileName[512+1];

	for(int j = 0;COpenRez::szArcFileName[j]!=0;j++);
	for(;COpenRez::szArcFileName[j]!='\\';j--);
	lstrcpy(FileName,COpenRez::szArcFileName);
	for(int k = 0;lpSubInfo->szFileName[k]!=0;FileName[++j]=lpSubInfo->szFileName[k],k++);
	FileName[j+1]=0;
	
	HANDLE h = FindFirstFile((LPCSTR)FileName,&find );
	if( h==INVALID_HANDLE_VALUE )
	{
		lpSubInfo->dwOriginalSize = 0;
		lstrcpy(lpSubInfo->szMode,"  ×  ");
	}else{
		lpSubInfo->dwOriginalSize = find.nFileSizeLow;
		if (lpSubInfo->dwOriginalSize==lpSubInfo->dwCompressedSize)
			lstrcpy(lpSubInfo->szMode,"  ○  ");
		else
			lstrcpy(lpSubInfo->szMode,"  △  ");
	}
	FindClose( h );
	
	return true;
}