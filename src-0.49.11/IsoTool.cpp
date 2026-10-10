
#include "stdafx.h"
#include "IsoTool.h"

bool CIsoTool::Check( const char* fname, unsigned long fsize )
{
	/*MemmapFile mf;
	if( !mf.Open(fname) )
		return false;
	const BYTE* beg = mf.base();
//--------------------------------------------------------------------//
	if( fsize <= PSECTOR*16 )
		return false;
	beg+=PSECTOR*16;
	if(beg[0]!=1 && beg[0]!=2)
		return false;
	if(beg[1]=='C' && beg[2]=='D' && beg[3]=='0' && beg[4]=='0' && beg[5]=='1')
		return true;
	return false;*/
	if( fsize <= PSECTOR*16 )
		return false;
	const BYTE* beg=CTool::common_buf;
	beg+=PSECTOR*16;
	if(beg[0]!=1 && beg[0]!=2)
		return false;
	if(beg[1]=='C' && beg[2]=='D' && beg[3]=='0' && beg[4]=='0' && beg[5]=='1')
		return true;
	return false;
}

bool CIsoTool::Extract( const char* fname, const char* ddir )
{
	/*if(!Check(fname,0))
		return false;*/
	MemmapFile mf;
	if( !mf.Open(fname) )
		return false;
	const BYTE* beg = mf.base();

//--------------------------------------------------------------------//
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
	m_extract(beg,&descriptor->root_directory_record,"");
	return true;
}
void CIsoTool::m_extract(const BYTE* b,ISO_DIRECTORY_RECORD* dr,LPCSTR ddir)
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

		if((ndr->flags & 2) && ((ndr->name && ndr->name!=1)))//ディレクトリ
		{
			const BYTE* data=&b[ndr->extent.l*PSECTOR];
			char name[MAX_PATH];
			lstrcpy(name,ddir);
			if(!joliet)
				lstrcpyn(&name[lstrlen(name)],&ndr->name,ndr->name_len+1);
			else
			{
				wchar_t b[MAX_PATH]={0};
				::memcpy(&b,&ndr->name,(ndr->name_len));
				WtoM(b,&name[lstrlen(name)]);
			}
			lstrcat(name,"\\");
			m_extract(b,(ISO_DIRECTORY_RECORD*)data,name);
		}else
		{
			if(ndr->name_len!=1 || (ndr->name && ndr->name!=1))
			{
				char outname[MAX_PATH]={0};
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
					if(name[i]==';')
					{
						name[i]=0;
						break;
					}
				}
				lstrcpy(outname,ddir);
				lstrcat(outname,name);
				int l=lstrlen(outname)-1;
				if(outname[l]=='.')
					outname[l]=0;
				if (m_Endflg) return;
				if (!CheckStr(outname))//ワイルドカード処理
				{
					nowpos+=ndr->length;
					ndr=(ISO_DIRECTORY_RECORD*)&b[nowpos+basepos];
					continue;
				}
				kiutil::pathMake( outname );
				FILE* fp = fopen( outname, "wb" );
				if(fp)
				{
					fwrite(&b[ndr->extent.l*PSECTOR],1,ndr->size.l,fp);
					fclose(fp);
					FILETIME ft;
					isoTimeToFileTime(&ndr->date,&ft);
					kiutil::timeSet( outname, &ft );
				}
			}
		}
		nowpos+=ndr->length;
		ndr=(ISO_DIRECTORY_RECORD*)&b[nowpos+basepos];
	}
}
void CIsoTool::isoTimeToFileTime(DATETIME_S* dtime,FILETIME* ft)
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
	//FILETIME ftime;
	SystemTimeToFileTime(&stime,ft);
	//FileTimeToLocalFileTime(&ftime,ft);
}
char* CIsoTool::WtoM(const wchar_t *pUcsStr, char* buff)
{
	wchar_t w[MAX_PATH]={0};
	for(int i=0;pUcsStr[i];i++)//エンディアン変換
	{
		w[i]=((pUcsStr[i] & 0xff00)>> 8)|(pUcsStr[i] & 0x00ff)<<8;
	}
	::WideCharToMultiByte( CP_ACP, 0, w, -1, buff, MAX_PATH, NULL, NULL );
	return buff;
}