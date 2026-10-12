#include "stdafx.h"
#include "OpenAin.h"
#include "../AinCodec.h"

#define AIN_MAGIC 0x21
#define AIN_HDR 24
#define AIN_IDXREC 29

static WORD AinHeaderCrc(const BYTE* hdr22)
{
	WORD s = 0;
	int i;
	for(i=0;i<22;i++)
		s = (WORD)(s + hdr22[i]);
	return (WORD)(s ^ 0x5555u);
}

static unsigned long AinSum(const BYTE* data, unsigned long size)
{
	unsigned long s = 0;
	unsigned long i;
	for(i=0;i<size;i++)
		s += data[i];
	return s & 0xFFFFu;
}

bool COpenAin::LoadIndex(void)
{
	BYTE hdr[AIN_HDR];
	long fsize;
	unsigned n_files, idx_pos;
	int method;

	if(COpenAin::m_entries)
	{
		delete[] COpenAin::m_entries;
		COpenAin::m_entries = NULL;
	}
	COpenAin::m_count = 0;
	COpenAin::m_cur = 0;

	fseek(COpenAin::fp,0,SEEK_END);
	fsize = ftell(COpenAin::fp);
	fseek(COpenAin::fp,0,SEEK_SET);
	if( AIN_HDR!=fread(hdr,1,AIN_HDR,COpenAin::fp) )
		return false;
	if(hdr[0]!=AIN_MAGIC)
		return false;
	method = hdr[1];
	if(method<0x11 || method>0x14)
		return false;
	if(hdr[3] & 0x40)
		return false;// multi-volume: deferred
	n_files = (hdr[8])|(hdr[9]<<8);
	idx_pos = (hdr[0x0E])|(hdr[0x0F]<<8)|(hdr[0x10]<<16)|(hdr[0x11]<<24);
	if(idx_pos==0 || (long)idx_pos>=fsize)
		return false;
	{
		WORD want = (hdr[0x16])|(hdr[0x17]<<8);
		if(AinHeaderCrc(hdr)!=want)
			return false;
	}
	COpenAin::m_method = (BYTE)method;

	// index stream is always an M1 Huffman session
	{
		long idx_len = fsize-(long)idx_pos;
		BYTE* idx_comp;
		BYTE* idx_raw;
		unsigned long idx_rawlen = 0;
		unsigned i;
		if(idx_len<=0 || idx_len>0x1000000)
			return false;
		idx_comp = new BYTE[idx_len];
		if(!idx_comp)
			return false;
		fseek(COpenAin::fp,(long)idx_pos,SEEK_SET);
		if(idx_len!=fread(idx_comp,1,idx_len,COpenAin::fp))
		{
			delete[] idx_comp;
			return false;
		}
		{
			WORD want = (hdr[0x12])|(hdr[0x13]<<8);
			if((AinSum(idx_comp,idx_len)&0xFFFFu)!=want)
			{
				delete[] idx_comp;
				return false;
			}
		}
		idx_raw = AinDecode(idx_comp,(unsigned long)idx_len,&idx_rawlen);
		delete[] idx_comp;
		if(!idx_raw && idx_rawlen>0)
			return false;
		if(n_files>0)
		{
			COpenAin::m_entries = new AinEntry[n_files];
			if(!COpenAin::m_entries)
			{
				if(idx_raw) free(idx_raw);
				return false;
			}
		}
		{
			unsigned long pos = 0;
			for(i=0;i<n_files;i++)
			{
				AinEntry* e = &COpenAin::m_entries[i];
				unsigned long fnlen, k;
				if(pos+AIN_IDXREC>idx_rawlen)
					break;
				e->timestamp = (idx_raw[pos+1])|(idx_raw[pos+2]<<8)
					|(idx_raw[pos+3]<<16)|(idx_raw[pos+4]<<24);
				e->orig_size = (idx_raw[pos+5])|(idx_raw[pos+6]<<8)
					|(idx_raw[pos+7]<<16)|(idx_raw[pos+8]<<24);
				e->comp_size = (idx_raw[pos+9])|(idx_raw[pos+10]<<8)
					|(idx_raw[pos+11]<<16)|(idx_raw[pos+12]<<24);
				e->crc = (idx_raw[pos+27])|(idx_raw[pos+28]<<8);
				fnlen = 0;
				while(pos+AIN_IDXREC+fnlen<idx_rawlen
					&& idx_raw[pos+AIN_IDXREC+fnlen]!=0
					&& fnlen<MAX_PATH-1)
					fnlen++;
				for(k=0;k<fnlen;k++)
					e->name[k] = (char)idx_raw[pos+AIN_IDXREC+k];
				e->name[fnlen] = '\0';
				pos += AIN_IDXREC+fnlen+1;
				if(pos<idx_rawlen && idx_raw[pos]==0)
					pos++;
			}
			if(i!=n_files)
			{
				delete[] COpenAin::m_entries;
				COpenAin::m_entries = NULL;
				if(idx_raw) free(idx_raw);
				return false;
			}
			COpenAin::m_count = n_files;
		}
		if(idx_raw) free(idx_raw);
	}
	return true;
}

bool COpenAin::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	if(!LoadIndex())
		return false;
	return FindSub(lpSubInfo);
}

bool COpenAin::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return FindSub(lpSubInfo);
}

bool COpenAin::FindSub(INDIVIDUALINFO *lpSubInfo)
{
	AinEntry* e;
	if(COpenAin::m_cur>=COpenAin::m_count)
		return false;
	e = &COpenAin::m_entries[COpenAin::m_cur++];
	lstrcpy(lpSubInfo->szFileName,e->name);
	lpSubInfo->dwCRC = e->crc;
	lpSubInfo->dwCompressedSize = e->comp_size;
	lpSubInfo->dwOriginalSize = e->orig_size;
	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	switch(COpenAin::m_method)
	{
	case 0x11: lstrcpy(lpSubInfo->szMode,"ultra"); break;
	case 0x12: lstrcpy(lpSubInfo->szMode,"normal"); break;
	case 0x13: lstrcpy(lpSubInfo->szMode,"fast"); break;
	default: lstrcpy(lpSubInfo->szMode,"store"); break;
	}
	lpSubInfo->uOSType = 0;// DOS
	lpSubInfo->wDate = (WORD)(e->timestamp>>16);
	lpSubInfo->wTime = (WORD)(e->timestamp & 0xFFFF);

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	return true;
}
