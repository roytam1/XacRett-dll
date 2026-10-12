#include "stdafx.h"
#include "AinTool.h"
#include "AinCodec.h"
#include <stdlib.h>

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

bool CAinTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<AIN_HDR )
		return false;
	if( hdr[0]!=AIN_MAGIC )
		return false;
	if( hdr[1]<0x11 || hdr[1]>0x14 )
		return false;
	if( hdr[3] & 0x40 )
		return false;// multi-volume: deferred
	{
		unsigned long idx_pos =
			(hdr[0x0E])|(hdr[0x0F]<<8)|(hdr[0x10]<<16)|(hdr[0x11]<<24);
		if( idx_pos==0 || idx_pos>=(unsigned long)fsize )
			return false;
	}
	{
		WORD want = (hdr[0x16])|(hdr[0x17]<<8);
		if( AinHeaderCrc(hdr)!=want )
			return false;
	}
	return true;
}

// Step one index record; returns false at the end. Fills name/orig,
// advances *ppos past the record (29 + NUL name + optional pad).
static bool AinNextRecord(const BYTE* idx_raw, unsigned long idx_rawlen,
	unsigned long* ppos, char* name, DWORD* orig)
{
	unsigned long pos = *ppos;
	unsigned long fnlen, k;
	if(pos+AIN_IDXREC>idx_rawlen)
		return false;
	*orig = (idx_raw[pos+5])|(idx_raw[pos+6]<<8)
		|(idx_raw[pos+7]<<16)|(idx_raw[pos+8]<<24);
	fnlen = 0;
	while(pos+AIN_IDXREC+fnlen<idx_rawlen
		&& idx_raw[pos+AIN_IDXREC+fnlen]!=0
		&& fnlen<MAX_PATH-1)
		fnlen++;
	for(k=0;k<fnlen;k++)
		name[k] = (char)idx_raw[pos+AIN_IDXREC+k];
	name[fnlen] = '\0';
	pos += AIN_IDXREC+fnlen+1;
	if(pos<idx_rawlen && idx_raw[pos]==0)
		pos++;
	*ppos = pos;
	return true;
}

bool CAinTool::Extract( const char* fname,const char* ddir )
{
	BYTE hdr[AIN_HDR];
	unsigned n_files, idx_pos;
	int method;
	FILE* ain;
//--------------------------------------------------------------------//

	ain = fopen( fname,"rb" );
	if( !ain )
		return false;
	if( AIN_HDR!=fread(hdr,1,AIN_HDR,ain) )
	{fclose(ain);return false;}
	if(hdr[0]!=AIN_MAGIC)
	{fclose(ain);return false;}
	method = hdr[1];
	if(method<0x11 || method>0x14)
	{fclose(ain);return false;}
	if(hdr[3] & 0x40)
	{fclose(ain);return false;}
	n_files = (hdr[8])|(hdr[9]<<8);
	idx_pos = (hdr[0x0E])|(hdr[0x0F]<<8)|(hdr[0x10]<<16)|(hdr[0x11]<<24);
	fseek(ain,0,SEEK_END);
	{
		long fsize = ftell(ain);
		if(idx_pos==0 || (long)idx_pos>=fsize)
		{fclose(ain);return false;}
	}
	{
		WORD want = (hdr[0x16])|(hdr[0x17]<<8);
		if(AinHeaderCrc(hdr)!=want)
		{fclose(ain);return false;}
	}

	// index stream is always an M1 Huffman session
	BYTE* idx_raw = NULL;
	unsigned long idx_rawlen = 0;
	{
		long fsize;
		long idx_len;
		BYTE* idx_comp;
		fseek(ain,0,SEEK_END);
		fsize = ftell(ain);
		idx_len = fsize-(long)idx_pos;
		if(idx_len<=0 || idx_len>0x1000000)
		{fclose(ain);return false;}
		idx_comp = new BYTE[idx_len];
		if(!idx_comp)
		{fclose(ain);return false;}
		fseek(ain,(long)idx_pos,SEEK_SET);
		if(idx_len!=fread(idx_comp,1,idx_len,ain))
		{
			delete[] idx_comp;
			fclose(ain);
			return false;
		}
		{
			WORD want = (hdr[0x12])|(hdr[0x13]<<8);
			if((AinSum(idx_comp,idx_len)&0xFFFFu)!=want)
			{
				delete[] idx_comp;
				fclose(ain);
				return false;
			}
		}
		idx_raw = AinDecode(idx_comp,(unsigned long)idx_len,&idx_rawlen);
		delete[] idx_comp;
		if(!idx_raw && idx_rawlen>0)
		{fclose(ain);return false;}
	}

	// stream checksum is carried by the last index entry (0 = unchecked)
	WORD stream_want = 0;
	{
		unsigned long p = 0;
		char nm[MAX_PATH];
		DWORD os = 0;
		unsigned j;
		for(j=0;j<n_files;j++)
		{
			unsigned long save = p;
			if(!AinNextRecord(idx_raw,idx_rawlen,&p,nm,&os))
				break;
			if(j+1==n_files && save+29<=idx_rawlen)
				stream_want = (idx_raw[save+27])|(idx_raw[save+28]<<8);
		}
		if(j!=n_files)
		{
			if(idx_raw) free(idx_raw);
			fclose(ain);
			return false;
		}
	}

	// data stream: verbatim for M4, one Huffman session otherwise
	BYTE* data_raw = NULL;
	unsigned long data_rawlen = 0;
	{
		long stream_len = (long)idx_pos-AIN_HDR;
		BYTE* stream_comp = NULL;
		if(stream_len<0)
		{
			if(idx_raw) free(idx_raw);
			fclose(ain);
			return false;
		}
		if(stream_len>0)
		{
			stream_comp = new BYTE[stream_len];
			if(!stream_comp)
			{
				if(idx_raw) free(idx_raw);
				fclose(ain);
				return false;
			}
			fseek(ain,AIN_HDR,SEEK_SET);
			if(stream_len!=fread(stream_comp,1,stream_len,ain))
			{
				delete[] stream_comp;
				if(idx_raw) free(idx_raw);
				fclose(ain);
				return false;
			}
			if(stream_want!=0
				&&(AinSum(stream_comp,stream_len)&0xFFFFu)!=stream_want)
			{
				delete[] stream_comp;
				if(idx_raw) free(idx_raw);
				fclose(ain);
				return false;
			}
		}
		fclose(ain);
		if((method & 0x0F)==4)
		{
			data_raw = stream_comp;
			data_rawlen = stream_len;
		}
		else
		{
			data_raw = AinDecode(stream_comp?stream_comp:(BYTE*)"",
				(unsigned long)(stream_len>0?stream_len:0),&data_rawlen);
			delete[] stream_comp;
			if(!data_raw && data_rawlen>0)
			{
				if(idx_raw) free(idx_raw);
				return false;
			}
		}
	}

	// slice the solid stream by orig_size
	{
		unsigned long pos = 0;
		unsigned long offset = 0;
		unsigned i;
		char outname[MAX_PATH];
		DWORD orig_size = 0;
		for(i=0;i<n_files;i++)
		{
			if(!AinNextRecord(idx_raw,idx_rawlen,&pos,outname,&orig_size))
				break;
			if(offset+orig_size>data_rawlen)
				break;// truncated solid stream
			if (m_Endflg){ break; }
			if (!CheckStr(outname))
			{
				offset += orig_size;
				continue;
			}
			{
				char* cn;
				FILE* out = fopen( cn=kiutil::pathMake(outname), "wb" );
				if(out)
				{
					if(orig_size>0)
						fwrite(data_raw+offset,1,orig_size,out);
					fclose(out);
				}
			}
			offset += orig_size;
		}
		if(idx_raw) free(idx_raw);
		if(data_raw) free(data_raw);
		if(i!=n_files)
			return false;
	}
	return true;
}
