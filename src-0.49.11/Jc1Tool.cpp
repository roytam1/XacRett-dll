#include "stdafx.h"
#include "Jc1Tool.h"
#ifndef _DEBUG
//Jcalg32.dllからもろパクリ。(^^;;;

bool CJc1Tool::Check( const char* fname, unsigned long fsize )
{
	MemmapFile mf;
	if( !mf.Open(fname) )
		return false;
	const BYTE* beg = mf.base();
	unsigned int nFilesize;
	
	nFilesize=JCALG1_GetUncompressedSizeOfCompressedBlock((void *)beg);
	mf.Close();
	if(nFilesize)
		return true;
	else
		return false;
}

bool CJc1Tool::Extract( const char* packedname, const char* ddir )
{
	char szOutFile[MAX_PATH];

	char *pData;
	char *pBuffer;
	unsigned int nFilesize;
	unsigned long nBytesRead;
	unsigned int nCompressedSize;
	//unsigned int nWindowSize = 64*1024;
	//int iTest;
	DWORD dwFlags=FILE_ATTRIBUTE_NORMAL;

	// 入力ファイルを開く。
	FILE* hInputFile=fopen(packedname,"rb");
	
	if(hInputFile==NULL){
		return false;//ERROR_NOT_FIND_FILE;
	}

	kiutil::getOriginalName( szOutFile,packedname,
		!strcmp(kiutil::pathExt(packedname),"jcalg1") ? NULL
			: !strcmp(kiutil::pathExt(packedname),"jc") ? NULL
			: !strcmp(kiutil::pathExt(packedname),"jc1") ? NULL: "unjcalg1" );

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(szOutFile))//追加 By Bonty
	{
		return true;
	}
	kiutil::pathMake(szOutFile);
	// 出力ファイルを開く。
	FILE* hOutputFile=fopen(szOutFile,"wb");

	if(hOutputFile==NULL) {
		fclose(hInputFile);
		return false;//ERROR_ARC_FILE_OPEN;
	}
	nFilesize=filelength(fileno(hInputFile));
	pData=new char[nFilesize];

	// 展開処理開始。
	nCompressedSize=nFilesize;
	nBytesRead=fread(pData,1,nCompressedSize,hInputFile);
	//ReadFile(hInputFile,(void *)pData,nCompressedSize,&nBytesRead,0);
	nFilesize=JCALG1_GetUncompressedSizeOfCompressedBlock((void *)pData);
	if(!nFilesize){
		delete[] pData;
		fclose(hInputFile);
		fclose(hOutputFile);
		return false;//ERROR_ARC_FILE_OPEN;
	}
	pBuffer=new char[nFilesize];		
	if(!nBytesRead){
		delete[] pBuffer;
		delete[] pData;
		fclose(hOutputFile);
		fclose(hInputFile);
		return false;//ERROR_EMPTY_INPUT;
	}
	nFilesize=JCALG1_Decompress_Fast((void *)pData,pBuffer);
	fwrite(pBuffer,nFilesize,1,hOutputFile);
	//WriteFile(hOutputFile,pBuffer,nFilesize,&nBytesRead,0);
	// 展開処理終了。

	delete[] pBuffer;
	delete[] pData;
	fclose(hInputFile);
	fclose(hOutputFile);

	return true; //OK
}
#endif
