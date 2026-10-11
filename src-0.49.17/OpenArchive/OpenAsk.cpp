#include "stdafx.h"
#include "OpenAsk.h"

bool COpenAsk::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	m_info.clear();
	LPCSTR ext=kiutil::pathExt(szArcFileName);
	INDIVIDUALINFO info={0};
	if( 0==stricmp( ext,"ask" ) )
	{
		// LongFileName取得
		WIN32_FIND_DATA find;
		HANDLE h = FindFirstFile( szArcFileName,&find );
		if( h==INVALID_HANDLE_VALUE )
			return false;
		FindClose( h );
		kiutil::getOriginalName( info.szFileName,find.cFileName,"org");
		info.dwCompressedSize = (DWORD)filelength(fileno(fp));
		lstrcpy(info.szAttribute ,"A--W-");//何か謎^^;
		m_info.push_back(info);
	}else if( 0==stricmp( ext,"apk" ) )
	{
		unsigned long int file_length,p=0;
		unsigned char *root;
		unsigned char num;
		typedef struct{
			char name[30];
			unsigned long int size;
		}_fd_;
		_fd_ *fd_;

		/*ファイルのサイズを得る*/
		file_length = filelength(fileno(fp));

		if((root = (unsigned char*)malloc(file_length))==NULL){
			return false;
		}

		fread(root,1,file_length,fp);
		fclose(fp);

		num = root[p];
		p++;

		bool ret=true;

		if((fd_ = (_fd_*)malloc(sizeof(_fd_)*num))==NULL){
			ret=false;
		}else{
			for(unsigned char i=0;i<num;i++)
			{
				memset(fd_[i].name,'\0',sizeof(fd_[i].name));
				sscanf((char*)(root+p),"%[^\n]",fd_[i].name);
				p+=strlen(fd_[i].name)+1;

				fd_[i].size = (*(root+p)<<24)+(*(root+p+1)<<16)+(*(root+p+2)<<8)+(*(root+p+3));
				p+=4;

				lstrcpy(info.szFileName,fd_[i].name);
				info.dwCompressedSize = fd_[i].size;
				lstrcpy(info.szAttribute ,"A--W-");//何か謎^^;
				m_info.push_back(info);
			}
		}
		free(root);
	}else
		return false;

	m_cur_info=m_info.begin();
	return FindNext(lpSubInfo);
}

bool COpenAsk::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	if(m_cur_info==m_info.end())
		return false;
	memcpy(lpSubInfo,&(*m_cur_info),sizeof(INDIVIDUALINFO));
	m_cur_info++;
	return true;
}