
#include "stdafx.h"
#include "LbrTool.h"

#define LBR_SECTOR 128
#define LBR_ENTRY 32

// CCITT/XMODEM CRC-16 (poly 0x1021, init 0). LBR stores the CRC so that
// member bytes (including short-sector pad) followed by the CRC itself
// high-byte-first yield zero.
static unsigned short LbrCrcUpdate(unsigned short crc, unsigned char b)
{
	int i;
	crc ^= (unsigned short)(b<<8);
	for(i=0;i<8;i++)
	{
		if(crc & 0x8000)
			crc = (crc<<1) ^ 0x1021;
		else
			crc <<= 1;
	}
	return (unsigned short)(crc & 0xFFFF);
}

bool CLbrTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<128 || fsize%LBR_SECTOR!=0 )
		return false;
	// directory control entry (fatal rule): active, blank name,
	// index 0, non-zero length
	if( hdr[0]!=0 )
		return false;
	{
		int i;
		for(i=1;i<=11;i++)
			if( hdr[i]!=0x20 )
				return false;
	}
	if( hdr[12]!=0 || hdr[13]!=0 )
		return false;
	{
		unsigned dirSectors = (hdr[14])|(hdr[15]<<8);
		if( dirSectors==0
			|| (unsigned long)dirSectors*LBR_SECTOR>(unsigned long)fsize
			|| (unsigned long)dirSectors*LBR_SECTOR>(unsigned long)siz )
			return false;
	}
	return true;
}

struct LbrSqNode
{
	short child[2];
};
#define LBR_SPEOF 256
#define LBR_NUMVALS 257

// Decode one squeezed (SQ, magic 76 FF) member to `out`.
// `start` points at the member's first byte. Returns true on clean EOF.
static bool UnSqueezeMember(FILE* lbr, long start, FILE* out)
{
	LbrSqNode node[LBR_NUMVALS];
	int bpos = 99, curin = 0;
	int numnodes, i, c;

	fseek(lbr,start,SEEK_SET);
	if( fgetc(lbr)!=0x76 || fgetc(lbr)!=0xFF )
		return false;
	fgetc(lbr); fgetc(lbr);// SQ checksum (LBR CRC guards integrity)
	// skip NUL-terminated embedded name
	for(i=0;i<256;i++)
	{
		c = fgetc(lbr);
		if(c<0)
			return false;
		if(c==0)
			break;
	}
	c = fgetc(lbr);
	{
		int d = fgetc(lbr);
		if(c<0 || d<0)
			return false;
		numnodes = c|(d<<8);
	}
	if( numnodes<=0 || numnodes>=LBR_NUMVALS )
		return false;
	// tree pairs, low byte first (sequenced reads; MSVC may reorder
	// unsequenced myfgetc() pairs under /O2)
	for(i=0;i<numnodes;i++)
	{
		int lo, hi;
		lo = fgetc(lbr); hi = fgetc(lbr);
		if(lo<0 || hi<0)
			return false;
		node[i].child[0] = (short)(lo|(hi<<8));
		lo = fgetc(lbr); hi = fgetc(lbr);
		if(lo<0 || hi<0)
			return false;
		node[i].child[1] = (short)(lo|(hi<<8));
	}
	// bit stream, LSB first, raw bytes out (no NCR layer in SQ)
	for(;;)
	{
		for(i=0;i>=0;)
		{
			if( ++bpos>7 )
			{
				curin = fgetc(lbr);
				if(curin<0)
					return false;
				bpos = 0;
				i = node[i].child[1&curin];
			}
			else
				i = node[i].child[1&(curin>>=1)];
		}
		i = -(i+1);
		if(i==LBR_SPEOF)
			break;
		fputc(i,out);
	}
	return true;
}

bool CLbrTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	FILE* lbr = fopen( fname,"rb" );
	if( !lbr )
		return false;
	if( LBR_ENTRY!=fread( buf,1,LBR_ENTRY,lbr ) )
	{fclose(lbr);return false;}

	// directory control entry (same fatal rule as Check)
	{
		int i;
		if(buf[0]!=0)
		{fclose(lbr);return false;}
		for(i=1;i<=11;i++)
			if(buf[i]!=0x20)
			{fclose(lbr);return false;}
		if(buf[12]!=0 || buf[13]!=0)
		{fclose(lbr);return false;}
	}
	unsigned dirSectors = (buf[14])|(buf[15]<<8);
	if(dirSectors==0)
	{fclose(lbr);return false;}
	int entries = dirSectors*4;

	for(int e=1;e<entries;e++)
	{
		BYTE rec[LBR_ENTRY];
		fseek(lbr,(long)e*LBR_ENTRY,SEEK_SET);
		if( LBR_ENTRY!=fread(rec,1,LBR_ENTRY,lbr) )
			break;
		if(rec[0]==0xFF)
			break;// unused terminates the directory
		if(rec[0]!=0)
			continue;// deleted
		unsigned index = (rec[12])|(rec[13]<<8);
		unsigned length = (rec[14])|(rec[15]<<8);
		unsigned crc = (rec[16])|(rec[17]<<8);
		unsigned pad = rec[26];
		if(pad>127 || length==0)
			continue;
		unsigned long fullsize = (unsigned long)length*LBR_SECTOR;
		if(fullsize<pad)
			continue;
		unsigned long datasize = fullsize-pad;
		long start = (long)index*LBR_SECTOR;

		char name[9], ext[4], outname[MAX_PATH];
		int i;
		for(i=0;i<8 && rec[1+i]!=0x20;i++)
			name[i] = (char)rec[1+i];
		name[i] = '\0';
		for(i=0;i<3 && rec[9+i]!=0x20;i++)
			ext[i] = (char)rec[9+i];
		ext[i] = '\0';
		if(ext[0])
			wsprintf(outname,"%s.%s",name,ext);
		else
			lstrcpy(outname,name);
		if(outname[0]=='\0')
			continue;

		// member kind (extension flag confirmed by magic)
		int kind = 0;// stored
		if(datasize>=2)
		{
			BYTE magic[2];
			fseek(lbr,start,SEEK_SET);
			if(2==fread(magic,1,2,lbr))
			{
				if((ext[1]=='q' || ext[1]=='Q')
					&& magic[0]==0x76 && magic[1]==0xFF)
					kind = 1;// squeezed
				else if((ext[1]=='z' || ext[1]=='Z')
					&& magic[0]==0x76
					&& (magic[1]==0xFE || magic[1]==0xFD))
					kind = 2;// crunched (no sample; listed but skipped)
			}
		}
		if(kind==2)
			continue;

		if (m_Endflg) return true;
		if (!CheckStr(outname))
			continue;

		char* cn;
		FILE* out = fopen( cn=kiutil::pathMake(outname), "wb" );
		if( !out )
			continue;

		bool ok = false;
		if(kind==0)
		{
			// stored: copy data bytes, CRC covers full sectors + pad
			unsigned short c = 0;
			unsigned long n;
			ok = true;
			fseek(lbr,start,SEEK_SET);
			for(n=0;n<fullsize;n++)
			{
				int b = fgetc(lbr);
				if(b<0){ ok = false; break; }
				c = LbrCrcUpdate(c,(unsigned char)b);
				if(n<datasize)
					fputc(b,out);
			}
			if(ok)
			{
				c = LbrCrcUpdate(c,(unsigned char)(crc>>8));
				c = LbrCrcUpdate(c,(unsigned char)(crc&0xFF));
				ok = (c==0);
			}
		}
		else
		{
			// squeezed: CRC-verify the compressed bytes first ...
			unsigned short c = 0;
			unsigned long n;
			fseek(lbr,start,SEEK_SET);
			ok = true;
			for(n=0;n<fullsize;n++)
			{
				int b = fgetc(lbr);
				if(b<0){ ok = false; break; }
				c = LbrCrcUpdate(c,(unsigned char)b);
			}
			if(ok)
			{
				c = LbrCrcUpdate(c,(unsigned char)(crc>>8));
				c = LbrCrcUpdate(c,(unsigned char)(crc&0xFF));
				ok = (c==0);
			}
			// ... then decode
			if(ok)
				ok = UnSqueezeMember(lbr,start,out);
		}

		fclose(out);
		if(!ok)
			DeleteFile(cn);
	}

	fclose(lbr);
	return true;
}
