#include "stdafx.h"
#include "OpenYz1.h"

bool COpenYz1::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	CYzEv ev;
	InEv	in(COpenYz1::szArcFileName);
	yzFileDecode dec(in, &ev, "" );

	/* The listing constructor only remembers the archive path - it does
	   NOT open the file or read the header, so dec_file_count is still 0.
	   yzFileDecode only fills dec_file_count in YzFile_DecodeHeader, which is
	   what CYz1Tool::Extract ends up running via DecodeFile; without it here
	   the archive lists as empty (the official binary lists the members). */
	if( 0 > dec.YzFile_DecodeHeader( COpenYz1::szArcFileName ) )
		{
		COpenYz1::File_Count = 0;
		COpenYz1::lpInfo = NULL;
		return false;
		}

	COpenYz1::File_Count = dec.dec_file_count;
	if( COpenYz1::File_Count <= 0 )
		{
		COpenYz1::lpInfo = NULL;
		return false;
		}
	COpenYz1::lpInfo = new INDIVIDUALINFO[COpenYz1::File_Count];

	//memcpy(COpenYz1::common_buf, dec.p_yz_dec_head,944+dec.dec_head_size);

	/*FILE *fpy=fopen("C:\\data.ddd","wb");
    fwrite(&COpenYz1::common_buf[944] ,dec.dec_head_size,1 ,fpy);
	fclose(fpy);*/
	COpenYz1::FindHeader(&dec);

	return COpenYz1::FindSub(lpSubInfo);
}

bool COpenYz1::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return COpenYz1::FindSub(lpSubInfo);
}

bool COpenYz1::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	static int s =-1;

	if (++s == COpenYz1::File_Count)
	{
		s=-1;
		return false;
	}
	memcpy(lpSubInfo,&COpenYz1::lpInfo[s],sizeof(INDIVIDUALINFO));
	return true;
}
void COpenYz1::FindHeader(yzFileDecode* dec)
{
	for (int i =0; i !=COpenYz1::File_Count;i++){
		COpenYz1::lpInfo[i].dwOriginalSize=dec->p_yz_dec_head->m_file_size_tbl[i];
		struct tm* time=localtime(&dec->p_yz_dec_head->m_file_mtime_tbl[i]);
		if( time!=NULL )
		{
			FILETIME ft;
			SYSTEMTIME sys;
			sys.wYear =      time->tm_year+1900;
			sys.wMonth =     time->tm_mon+1;
			sys.wDayOfWeek = time->tm_wday;
			sys.wDay =       time->tm_mday;
			sys.wHour =      time->tm_hour;
			sys.wMinute =    time->tm_min;
			sys.wSecond =    time->tm_sec;
			sys.wMilliseconds = 0;
			SystemTimeToFileTime(&sys,&ft);
			FileTimeToDosDateTime(&ft,&COpenYz1::lpInfo[i].wDate,&COpenYz1::lpInfo[i].wTime);
		}
		LPCSTR p=dec->p_yz_dec_head->m_file_name_tbl[i].c_str();
		lstrcpy(COpenYz1::lpInfo[i].szFileName,p);

		lstrcpy(COpenYz1::lpInfo[i].szAttribute ,"A---W");
		lstrcpy(COpenYz1::lpInfo[i].szMode,"YZ1");
		COpenYz1::lpInfo[i].dwCompressedSize=0;
		COpenYz1::lpInfo[i].dwCRC=0;
		COpenYz1::lpInfo[i].uFlag=0;
		COpenYz1::lpInfo[i].uOSType=0;
		COpenYz1::lpInfo[i].wRatio=0;
	}
	/*int n = 944;
	int tmp=0;
	DWORD h_Update;

	for (int i =0; i !=COpenYz1::File_Count;i++,n+=4){
		memcpy(&COpenYz1::lpInfo[i].dwOriginalSize ,&COpenYz1::common_buf[n],4);
		lstrcpy(COpenYz1::lpInfo[i].szAttribute ,"A---W");
		lstrcpy(COpenYz1::lpInfo[i].szMode,"");
		COpenYz1::lpInfo[i].dwCompressedSize=0;
		COpenYz1::lpInfo[i].dwCRC=0;
		COpenYz1::lpInfo[i].uFlag=0;
		COpenYz1::lpInfo[i].uOSType=0;
		COpenYz1::lpInfo[i].wRatio=0;
	}
	for (i =0; i !=COpenYz1::File_Count;i++,n+=4){
		memcpy(&h_Update,&COpenYz1::common_buf[n],4);
		struct tm* time=localtime((long*)&h_Update);
		if( time!=NULL )
		{
			FILETIME ft;
			SYSTEMTIME sys;

			sys.wYear =      time->tm_year+1900;
			sys.wMonth =     time->tm_mon+1;
			sys.wDayOfWeek = time->tm_wday;
			sys.wDay =       time->tm_mday;
			sys.wHour =      time->tm_hour;
			sys.wMinute =    time->tm_min;
			sys.wSecond =    time->tm_sec;
			sys.wMilliseconds = 0;
			SystemTimeToFileTime(&sys,&ft);
			FileTimeToDosDateTime(&ft,&COpenYz1::lpInfo[i].wDate,&COpenYz1::lpInfo[i].wTime);
		}
	}
	for (i =0; i !=COpenYz1::File_Count;i++){
		tmp=n;
		for (;COpenYz1::common_buf[n] !=0;n++);
		n++;
		memcpy(&COpenYz1::lpInfo[i].szFileName ,&COpenYz1::common_buf[tmp],n-tmp);
	}
	memcpy(COpenYz1::common_buf,lpInfo,COpenYz1::File_Count*sizeof(INDIVIDUALINFO));
	delete []lpInfo;
	return;*/
}