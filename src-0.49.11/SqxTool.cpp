#include "stdafx.h"
#include "SqxTool.h"

CSqxTool *pSqx; //.....

extern "C"{
int InitArchiver(void);
void DoneArchiver(void);
void ExtractFiles(void);
int FindArcMainHeader(OS_FILE OsFile,SLONG32 lMaxSearchSize,SLONG64 *l64HeadOfs,SLONG32 *lHeadSize);

int GetOwPermission(char*){return 0;}
int GetCryptKey(char* szCKey,char*,int){
	char* szKey=pSqx->GetPass();
	strncpy(szCKey,szKey,strlen(szKey)>60?60:strlen(szKey)+1);
	return 1;
}
int GetNetVolPath(char*,int){return 0;} //MVは非対応。
void SetProgress(int){}
long os_close(void*);
void* os_open(char*,char*);

int GetNextVolumeName(SLONG32 lFoceVolumeName);
void ExtractArcFile(ARC_NODE *ALNode);
void FreeDecompressor(void);
int InitDecompressor(void);
int ReadNextArcFHDR(ARC_NODE *ALNode);
int ArcNodeMatches(ARC_NODE *ALNode);
void GetOutFileName(char *szFileName,char *szOutFName);
int GetOWritePermission(ARC_NODE *ALNode,char *szFileName);
int GetFileKey(char *szFileName);
}

bool CSqxTool::Check( const char* fname, unsigned long fsize )
{
	SLONG64 l64HeadOfs;
	SLONG32 lHeadSize;
	int ret;
	strcpy(ArcStruct.szArcName, fname);
	ArcStruct.arcfile=os_open(ArcStruct.szArcName,"rb");
	if(ArcStruct.arcfile==OS_INVALID_FILE)return 0;
	ret=FindArcMainHeader(ArcStruct.arcfile,0x20000,&l64HeadOfs,&lHeadSize);
	os_close(ArcStruct.arcfile);
	return ret?true:false;
}

bool CSqxTool::Extract( const char* fname, const char* ddir )
{
	SLONG32 lError;
//--------------------------------------------------------------------//
	if(!InitArchiver()) return false;

	strcpy(ArcStruct.szArcName, fname);
	strcpy(ArcStruct.szOutPath, ddir);

	pSqx=this; //(^^;;;
	//ExtractFiles();

	ARC_NODE	AlNode;
	SLONG64		l64HeadOfs;
	SLONG32		lHeadSize;
	SLONG32		lProcessMember;
	SLONG32		lSkipFlag;
	char		szFileToSkip[CX_MAXPATH];
	char		szOutFName[CX_MAXPATH];

	if( InitDecompressor() == (int)FALSE )
		return false;

	lSkipFlag = OPTION_NONE;

	while( TRUE )
	{
		ArcStruct.arcfile = os_open(ArcStruct.szArcName,"rb");
		if( ArcStruct.arcfile == OS_INVALID_FILE )
			break;
		
		if( FindArcMainHeader(ArcStruct.arcfile,0x20000,&l64HeadOfs,&lHeadSize) == (int)FALSE )
			break;
		
		ArcStruct.l64TempArcOfs = l64HeadOfs + lHeadSize;

		while( TRUE )
		{
			memset(&AlNode,0,sizeof(ARC_NODE));
			if( ReadNextArcFHDR(&AlNode) == (int)FALSE )
				break;

			if( AlNode.lTagged == OPTION_NONE )
				continue;
			
			if( lSkipFlag == OPTION_SET )
			{
				if( strcmp(szFileToSkip,AlNode.szFName) == 0 )
					continue;
				lSkipFlag = OPTION_NONE;
			}

			//lProcessMember = ArcNodeMatches(&AlNode);
			if (m_Endflg) return true;
			//ワイルドカード処理
			lProcessMember =CheckStr(AlNode.szFName);

			if( (lProcessMember == OPTION_NONE) && (ArcStruct.iSolidFlag == OPTION_NONE) )
				continue;
			
			IoStruct.l64CompSize = AlNode.l64NewSize;
			IoStruct.l64OrigSize = AlNode.l64OrigSize;
			IoStruct.l64ToFlush = AlNode.l64OrigSize;
			IoStruct.lImageSize = (SLONG32)AlNode.l64OrigSize;
			IoStruct.u32FileCRC = CRC32_INIT;
			IoStruct.lImageLastCount = 0;
			IoStruct.usExtCompressor = 0;
			ArcStruct.uCFileCRC = AlNode.uFileCRC;

			GetOutFileName(AlNode.szFName,szOutFName);
			kiutil::pathMake(szOutFName);//-----------------------

			os_seek(ArcStruct.arcfile,AlNode.l64FileHdrStart + AlNode.lTotalBlockSize,SEEK_SET);
			ArcStruct.iIgnoreWrite = OPTION_NONE;

			if( (lProcessMember != OPTION_NONE) && (ArcStruct.iTestErrorsArc != OPTION_SET) )
			{
				switch( GetOWritePermission(&AlNode,szOutFName) )
				{
					case 1:
						if( ArcStruct.iSolidFlag == OPTION_SET )
							ArcStruct.iIgnoreWrite = OPTION_SET;
						else
						{
							strcpy(szFileToSkip,AlNode.szFName);
							lSkipFlag = OPTION_SET;
							continue;
						}
						break;
				
					case 2:
						ArcStruct.ArchiveStatus = SQX_EC_USER_ABORT;
						break;
					default: //- 0
						break;
				}
			}
			else
				ArcStruct.iIgnoreWrite = OPTION_SET;

			if( ArcStruct.iTestErrorsArc == OPTION_SET )
				ArcStruct.iIgnoreWrite = OPTION_SET;

			if( AlNode.lBlockType == SQX_DIR_HEAD_ID )
			{
				if( ArcStruct.iIgnoreWrite == OPTION_NONE )
				{
					if( os_create_full_file_path(szOutFName) == (int)FALSE )
						ArcStruct.ArchiveStatus = SQX_EC_ACCESS_DENIED;
#if !defined(__SQX_OS_LINUX__)   
					os_setfileattr(szOutFName,(AlNode.lAttr & ~OS_ATTRIBUTE_DIRECTORY));
#else
					//- TODO Linux
#endif
				}
				continue;
			}
			else
			{
				if( (ArcStruct.iIgnoreWrite == OPTION_NONE) && (ArcStruct.iFileWasSplit == OPTION_NONE) )
				{
					if( os_create_ouput_file(&ArcStruct.outfile,szOutFName) == (int)FALSE )
					{
						ArcStruct.ArchiveStatus = SQX_EC_ACCESS_DENIED;
						break;
					}
				}

				ArcStruct.iEncryption = (int)AlNode.lCryptFlag;

				if( AlNode.lCryptFlag != OPTION_NONE )
				{
					if( GetFileKey(AlNode.szFName) == (int)FALSE )
						break;
				}
				
				if( (AlNode.lFlags & SQX_NEXT_VOL_FLAG) != OPTION_NONE )
					ArcStruct.iFileWasSplit = OPTION_SET;
				else
					ArcStruct.iFileWasSplit = OPTION_NONE;
				
				ExtractArcFile(&AlNode);

				if( ArcStruct.ArchiveStatus != SQX_EC_OK )
					break;

				if( (ArcStruct.iIgnoreWrite == OPTION_NONE)  && (ArcStruct.iFileWasSplit == OPTION_NONE) )
				{
#if !defined(__SQX_OS_LINUX__)   
					os_setdosftime(ArcStruct.outfile,(AlNode.lFileTime >> 16),(AlNode.lFileTime & 0xFFFF));
					os_close(ArcStruct.outfile);
					ArcStruct.outfile = OS_INVALID_FILE;
					os_setfileattr(szOutFName,AlNode.lAttr);
#else
					//- TODO Linux
#endif
				}
			}
			
		} //- while #2
	
		if( (ArcStruct.iMVFlag == OPTION_NONE) || ((ArcStruct.iMVFlag == OPTION_SET) && (ArcStruct.iLastVolFlag == OPTION_SET)) || (ArcStruct.ArchiveStatus != SQX_EC_OK) )
			break;
		
		if( GetNextVolumeName(1) == (int)FALSE )
			break;

	}//- while #1
	
	if( ArcStruct.outfile != OS_INVALID_FILE )
		os_close(ArcStruct.outfile);
	
	if( ArcStruct.arcfile != OS_INVALID_FILE )
		os_close(ArcStruct.arcfile);

	FreeDecompressor();

	lError=ArcStruct.ArchiveStatus;
	DoneArchiver();

	return lError==SQX_EC_OK ? true : false;
}

char* CSqxTool::GetPass(){return GetPassword();}