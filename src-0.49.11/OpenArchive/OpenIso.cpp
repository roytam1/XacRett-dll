#include "stdafx.h"
#include "OpenIso.h"

bool COpenIso::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	m_info.clear();

	MemmapFile mf;
	if( !mf.Open(szArcFileName) )
		return false;
	const BYTE* beg = mf.base();
	if( mf.size() <= PSECTOR*16 )
		return false;
	
	ISO_PRIMARY_DESCRIPTOR* descriptor=(ISO_PRIMARY_DESCRIPTOR*)&beg[PSECTOR*16];
	joliet=false;
	//joliet
	{
	int block = 16;
	while( (unsigned char) descriptor->type != 0 )
	{
		if(descriptor->escape_sequences[0] == 0x25
	      && descriptor->escape_sequences[1] == 0x2f
	      && descriptor->escape_sequences[3] == 0
	      && (descriptor->escape_sequences[2] == 0x40|| descriptor->escape_sequences[2] == 0x43|| descriptor->escape_sequences[2] == 0x45) )
	    {
	      break;
	    }
		block++;
		descriptor=(ISO_PRIMARY_DESCRIPTOR*)&beg[PSECTOR*block];
	}

	if( (unsigned char) descriptor->type == 0 )
		descriptor=(ISO_PRIMARY_DESCRIPTOR*)&beg[PSECTOR*16];
	else
		joliet=true;
    }
	ISO_DIRECTORY_RECORD* dr=(ISO_DIRECTORY_RECORD*)&beg[descriptor->root_directory_record.extent.l*PSECTOR];
	Listing(beg,&descriptor->root_directory_record,"");

	m_curinfo=m_info.begin();
	bool ret=m_curinfo!=m_info.end();
	if(ret)
	{
		memcpy(lpSubInfo,&(*m_curinfo),sizeof(INDIVIDUALINFO));
		m_curinfo++;
	}
	return ret;
}

bool COpenIso::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	bool ret=m_curinfo!=m_info.end();
	if(ret)
	{
		memcpy(lpSubInfo,&(*m_curinfo),sizeof(INDIVIDUALINFO));
		m_curinfo++;
	}
	return ret;
}
void COpenIso::Listing(const BYTE* b,ISO_DIRECTORY_RECORD* dr,LPCSTR parent)
{
	int extent=dr->extent.l;
	int len=dr->size.l;
	int	basepos=extent*PSECTOR;
	int nowpos=0;

	if(len==0)return;
	ISO_DIRECTORY_RECORD* ndr=(ISO_DIRECTORY_RECORD*)&b[nowpos+basepos];

	for(;;)
	{
		if(ndr->length==0)
		{
			len-=PSECTOR;
			if(len)
			{
				nowpos=0;
				extent++;
				basepos=extent*PSECTOR;
				ndr=(ISO_DIRECTORY_RECORD*)&b[nowpos+basepos];
			}else
				break;
		}

		if((ndr->flags & 2) && (ndr->name_len!=1 || (ndr->name && ndr->name!=1)))//ディレクトリ
		{
			const BYTE* data=&b[ndr->extent.l*PSECTOR];
			char name[MAX_PATH]={0};
			lstrcpy(name,parent);
			if(!joliet)
				lstrcpyn(&name[lstrlen(name)],&ndr->name,ndr->name_len+1);
			else
			{
				wchar_t b[MAX_PATH]={0};
				::memcpy(&b,&ndr->name,(ndr->name_len));
				WtoM(b,&name[lstrlen(name)]);
			}
			lstrcat(name,"\\");
			Listing(b,(ISO_DIRECTORY_RECORD*)data,name);
		}else
		{
			if(ndr->name_len!=1 || (ndr->name && ndr->name!=1))
			{
				INDIVIDUALINFO info;
				::memset(&info,0,sizeof(INDIVIDUALINFO));
				char name[MAX_PATH]={0};
				if(!joliet)
					lstrcpyn(name,&ndr->name,ndr->name_len+1);
				else
				{
					wchar_t b[MAX_PATH]={0};
					::memcpy(&b,&ndr->name,(ndr->name_len));
					WtoM(b,name);
				}
				//;1等を消す
				for(int i=ndr->name_len;i;i--)
				{
					if(name[i]==';' || name[i]==':')
					{
						name[i]=0;
						break;
					}
				}
				lstrcpy(info.szFileName,parent);
				lstrcat(info.szFileName,name);
				int l=lstrlen(info.szFileName)-1;
				if(info.szFileName[l]=='.')
					info.szFileName[l]=0;
				info.dwOriginalSize=info.dwCompressedSize=ndr->size.l;
				isoTimeToDosTime(&ndr->date,&info.wDate,&info.wTime);
				m_info.push_back(info);
			}
		}
		nowpos+=ndr->length;
		ndr=(ISO_DIRECTORY_RECORD*)&b[nowpos+basepos];
	}
}
void COpenIso::isoTimeToDosTime(DATETIME_S* dtime,WORD* date,WORD* time)
{
	SYSTEMTIME stime;
	stime.wYear=1900+dtime->year;
	stime.wMonth=dtime->month;
	stime.wDayOfWeek=1;//適当でいいか^^;
	stime.wDay=dtime->day;
	stime.wHour=dtime->hour;
	stime.wMinute=dtime->minute;
	stime.wSecond=dtime->second;
	stime.wMilliseconds=0;
	FILETIME ftime,lftime;
	SystemTimeToFileTime(&stime,&ftime);
	//FileTimeToLocalFileTime(&ftime,&lftime);
	FileTimeToDosDateTime(&ftime,date,time);
}
char* COpenIso::WtoM(const wchar_t *pUcsStr, char* buff)
{
	wchar_t w[MAX_PATH]={0};
	for(int i=0;pUcsStr[i];i++)//エンディアン変換
	{
		w[i]=((pUcsStr[i] & 0xff00)>> 8)|(pUcsStr[i] & 0x00ff)<<8;
	}
	::WideCharToMultiByte( CP_ACP, 0, w, -1, buff, MAX_PATH, NULL, NULL );
	return buff;
}

