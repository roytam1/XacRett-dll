
#include "stdafx.h"
#include "ImpTool.h"

// IMP_DIR_ENTRY flags
#define DFLAG_DIR     1
#define DFLAG_E8SHORT 2
#define DFLAG_E8LONG  4

// IMP_HEADER flags
#define HFLAG_MULTIVOL 1
#define HFLAG_LASTVOL  2
#define HFLAG_BLOWFISH 4

// methods
#define METHOD_STORE   0
#define METHOD_LZ77    1
#define METHOD_BWT     2
#define METHOD_LZ77_MM 3

// LZ77
#define NLLCODES 288 /* Number of literal-length codes */
#define NDCODES 42   /* Number of distance codes */
#define ND2CODES 14  /* Number of distance codes for length==2 */
#define ND3CODES 28  /* Number of distance codes for length==3 */
#define MIN_LZ_CODES (NLLCODES+NDCODES+ND2CODES)
#define LONGMATCHBITS 15  /*extra bits for long match*/
#define LONGMATCHMASK ((1<<LONGMATCHBITS)-1)
#define LONGMATCHCODE (NLLCODES-2)
#define MMCODE (NLLCODES-1) /*multimedia code*/
#define LLTTBITS 9
#define DTTBITS 7
#define LLTTSIZE (1<<LLTTBITS)
#define DTTSIZE (1<<DTTBITS)
#define MAX_MM_CODES 1024

// BWT
#define NCODES 258
#define WINDOW 50
#define BWTABLES 6
#define BWTTBITS 9
#define BWTTSIZE (1<<BWTTBITS)

#define MAX_NCODES NLLCODES
#define MAXCODELEN 16
#define NLENCODES (MAXCODELEN+5) /* no. of code length codes */
#define ZRUN (MAXCODELEN+1) /* run of zeros */
#define DZRUN (MAXCODELEN+3)/* run of delta-zeros*/
#define HC_MCL 14    /* max code length for storing Huffman codes */
#define HCTTBITS 6
#define HCTTSIZE (1<<HCTTBITS)

#define USEDBITS(n) {bitpos-=n;bitbuf>>=n;}


//////////////////////////////////////////////////////////////

bool CImpTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	// 小さすぎ
	if( siz<IMP_HEADER_SIZE )
		return false;

	// IMPならCRCチェック
	if( hdr[0]=='I' && hdr[1]=='M' && hdr[2]=='P' && hdr[3]=='\n' )
		return HeaderCheck( hdr );

	// EXEならヘッダ探し
	if( hdr[0]=='M' && hdr[1]=='Z' )
	{
		const unsigned char* src = hdr+2;
		const unsigned char* end
			= hdr+(siz>32768 ? 32768 : siz)-IMP_HEADER_SIZE+1;
		do
		{
			while(src!=end)
				if( *(src++)=='I' )
					break;
			if( src==end )return false;
			if( src[0]=='M' && src[1]=='P' && src[2]=='\n' )
				if( HeaderCheck(src-1) )
					return true;
			src++;
		}while( src<end );
		return false;
	}

	return false;
}

bool CImpTool::Extract( const char* fname,const char* ddir )
{
	imp = fopen(fname,"rb");
	if( imp==NULL )return false;

	if( !ReadImpHead() )
		{fclose(imp);return false;}
	if(ih.flags&HFLAG_MULTIVOL)
		{fclose(imp);return false;}
	if(ih.flags&HFLAG_BLOWFISH)
		{fclose(imp);return false;}

	g_blockbuffer=0;g_prevfpos=0xffffffff;
	dirbufsrc = dirbufend = dirbuf;

	char outname[MAX_PATH], *name;
	IMP_DIR_ENTRY dirent;
	for( unsigned long ent=0; ent!=ih.direntries; ent++ )
	{
		if( !get_dir_entry(&dirent,outname) )
			return false;

		if (m_Endflg) return true;//追加 By Bonty
		if (!CheckStr(outname))//追加 By Bonty
			continue;

		name = kiutil::pathMake( outname );
		if( dirent.flags&DFLAG_DIR )continue;
		out = fopen( name,"wb" );
		if( !out ) continue;

		decomp( out, dirent.fpos,
			dirent.unitpos, dirent.esize, dirent.flags );
		fclose( out );

		kiutil::timeSet( name,(WORD)(dirent.mdate),(WORD)dirent.mtime );
		SetFileAttributes( name,dirent.attrib );
	}

	return true;
}


/********************************/
static const int g_ncodes[3]={NDCODES,ND2CODES,ND3CODES},
				 g_ncoffset[3]={NLLCODES,NLLCODES+NDCODES,NLLCODES+NDCODES+ND2CODES};
static const unsigned long g_overlaps[3][8]=
   {{32768,65536,73728,98304,122880,147456,172032,196608}, /*normal*/
    {32768,65536,73728,98304,122880,147456,172032,196608}, /*normal*/
    {32768,131072,196608,262144,327680,393216,458752,524288}}; /*best*/

bool CImpTool::ReadImpHead()
{
	unsigned char ihbuf[IMP_HEADER_SIZE];
	g_exesize=0;

	// ヘッダ分読む
	if( IMP_HEADER_SIZE!=fread(ihbuf,1,IMP_HEADER_SIZE,imp) )
		return false;

	// IMPならCRCチェック
	if( ihbuf[0]=='I' && ihbuf[1]=='M' && ihbuf[2]=='P' && ihbuf[3]=='\n' )
	{if( !HeaderCheck(ihbuf) )return false;}
	// EXEならヘッダ探し
	else if( ihbuf[0]=='M' && ihbuf[1]=='Z' )
	{
		unsigned char *buffer=new unsigned char[32768],*src,*end;
		int nread = fread(buffer,1,32768,imp);
		if( nread<IMP_HEADER_SIZE )return false;
		src=buffer;
		end=buffer+nread-IMP_HEADER_SIZE+1;
		do
		{
			while(src!=end)
				if( *(src++)=='I' )
					break;
			if( src==end )return false;
			if( src[0]=='M' && src[1]=='P' && src[2]=='\n' )
				if( HeaderCheck(src-1) )
				{
					g_exesize = src-1-buffer+IMP_HEADER_SIZE;
					memcpy(ihbuf,src-1,IMP_HEADER_SIZE);
					break;
				}
			src++;
		}while( src<end );
		if( src>=end )return false;
		delete [] buffer;
	}
	// でなけりゃ失敗
	else
		return false;

	// 読む
	ih.diroffset = ihbuf[ 4] + (ihbuf[ 5]<<8) + (ihbuf[ 6]<<16) + (ihbuf[ 7]<<24);
	ih.direntries= ihbuf[ 8] + (ihbuf[ 9]<<8) + (ihbuf[10]<<16) + (ihbuf[11]<<24);
	ih.cmoffset  = ihbuf[16] + (ihbuf[17]<<8) + (ihbuf[18]<<16) + (ihbuf[19]<<24);
	ih.flags     = ihbuf[38] + (ihbuf[39]<<8);
	ih.crc       = ihbuf[40] + (ihbuf[41]<<8);

	dirfpos=ih.diroffset+g_exesize;
	return true;
}

bool CImpTool::HeaderCheck(const unsigned char* p)
{
	const unsigned char dm[2]={0,0};
	DWORD crc=0xffffffff;

	crc = calc_crc( p,40,crc );
	crc = calc_crc( dm,2,crc );

	return ((~crc)&0xffff) == (unsigned)(p[40]|(p[41]<<8));
}

bool CImpTool::get_dir_entry( IMP_DIR_ENTRY *pdirent,char *name )
{
	if( dirbufsrc >= dirbufend )
		if( !extract_dir() )
			return false;
	if( dirbufend-dirbufsrc<IMP_DIR_ENTRY_SIZE )
		return false;

	pdirent->ver	= dirbufsrc[0] + (dirbufsrc[1]<<8);
	pdirent->fpos	= dirbufsrc[4] + (dirbufsrc[5]<<8) + (dirbufsrc[6]<<16) + (dirbufsrc[7]<<24);
	pdirent->commlen= dirbufsrc[10];
	pdirent->flags	= dirbufsrc[11];
	pdirent->unitpos= dirbufsrc[12] + (dirbufsrc[13]<<8) + (dirbufsrc[14]<<16) + (dirbufsrc[15]<<24);
	pdirent->esize	= dirbufsrc[16] + (dirbufsrc[17]<<8) + (dirbufsrc[18]<<16) + (dirbufsrc[19]<<24);
	pdirent->crc	= dirbufsrc[20] + (dirbufsrc[21]<<8) + (dirbufsrc[22]<<16) + (dirbufsrc[23]<<24);
	pdirent->extra	= dirbufsrc[24] + (dirbufsrc[25]<<8);
	pdirent->namelen= dirbufsrc[26] + (dirbufsrc[27]<<8);
	pdirent->mtime	= dirbufsrc[32] + (dirbufsrc[33]<<8);
	pdirent->mdate	= dirbufsrc[34] + (dirbufsrc[35]<<8);
	pdirent->entcrc	= dirbufsrc[36] + (dirbufsrc[37]<<8);

	if( (pdirent->ver&0xfff)>0x10A/*MAX_VERSION*/ )
		return false;
	int i=IMP_DIR_ENTRY_SIZE+pdirent->namelen+pdirent->commlen+pdirent->extra;
	if( dirbufend-dirbufsrc<i )
		return false;
	dirbufsrc[36]=0;
	dirbufsrc[37]=0;
	if( (~calc_crc(dirbufsrc,i,0xffffffff)&0xffff)!=pdirent->entcrc )
		return false;
	memcpy( name,dirbufsrc+IMP_DIR_ENTRY_SIZE,pdirent->namelen );
	name[ pdirent->namelen ]=0;
	for(char*p=name;*p;p=CharNext(p))if(*p=='/')*p='\\';

	dirbufsrc+=i;
	return true;
}

bool CImpTool::extract_dir()
{
	int i,method;
	unsigned long csize,exsize;
	unsigned char sig[6];
	static const unsigned char dirsig[]={0x49,0x4D,0x50,0x44,0x45,0};
 
	unsigned long fpos=ftell(imp);
	fseek( imp,dirfpos,SEEK_SET );

	dirbufsrc=dirbuf;
	dirbufend=dirbuf;
	if( 6!=fread(sig,1,6,imp) )		return false;
	if( 0!=memcmp(sig,&dirsig,6) )	return false;

	init_getbits();
	method=getbits(5)&15;
	USEDBITS(5);
	if(method>METHOD_LZ77)return false;
	exsize=getbits(20)&((1L<<20)-1);
	USEDBITS(20);
	if(exsize>8192)return false;
	csize=getbits(20)&((1L<<20)-1);
	USEDBITS(20);
	if(method==METHOD_LZ77)
	{
		USEDBITS(5);
		i=expand_lz((unsigned char*)dirbuf,0,exsize,0);
	}
	else i=unstore((unsigned char*)dirbuf,exsize);
	dirfpos+=6+csize;

	fseek( imp,fpos,SEEK_SET );
	dirbufend+=i;
	return true;
}

void CImpTool::init_getbits()
{
	dcsrc=dcend=dcbuffer;
	bitpos=0;
	bitbuf=0;
}

unsigned long CImpTool::getbits(int nbits)
{
	int iseof=0;
	if( nbits>bitpos )
		while( bitpos<25 )
		{
			if(dcsrc>=dcend)
			{
				dcsrc=dcbuffer;
				if(!iseof)
				{
					dcend=dcbuffer+fread(dcbuffer,1,sizeof(dcbuffer),imp);
					iseof=feof(imp);
				}
			}
			bitbuf+=(*dcsrc++)<<bitpos;
			bitpos+=8;
		}
	return bitbuf;
}

int CImpTool::unstore(unsigned char *output,int size)
{
	fseek(imp,dcsrc-dcend-(bitpos>>3),SEEK_CUR);
	return fread(output,1,size,imp);
}

int CImpTool::decomp( FILE *outfile,unsigned long fpos,unsigned long unitpos,unsigned long togo,unsigned char flags)
{
	unsigned char lhbuf[LOCAL_HEADER_SIZE];
	unsigned short namelen;
	int i,lhptr,nameptr;
	unsigned long method,level,ovlevel;
	int nread,written;
	unsigned long overlap,e8offset,srcsize,block_csize;
	unsigned char *blocksrc;
 
	if(!togo){g_crc=0;return 0;}
	g_crc=0xFFFFFFFF;
	g_e8extra=0;
	srcsize=togo;
	e8offset=0;
	g_exlast=0;
	lhptr=0;nameptr=0;
	g_e8t=0;
	if(flags&DFLAG_E8SHORT)g_e8t=2;
	else if(flags&DFLAG_E8LONG)g_e8t=4;
	fpos+=6+g_exesize;
	if(fpos!=g_prevfpos || unitpos<g_unitoffset)
	{
		g_blockstart=0;
		g_blocksize=0;
		g_exlast=0;
		g_exprev=0;
		g_unitoffset=0;
		g_exerror=0;
		g_mmptr=0;
		g_prevfpos=fpos;
		fseek(imp,fpos,SEEK_SET);
		if(g_blockbuffer){delete [] g_blockbuffer;g_blockbuffer=NULL;}
	}

	while(togo)
	{
		if(g_exerror)return -3;
		if(unitpos<g_unitoffset+g_blocksize)
		{
			blocksrc=g_blockbuffer+g_blockstart+unitpos-g_unitoffset;
			nread=g_blocksize-(unitpos-g_unitoffset);
			if(lhptr<LOCAL_HEADER_SIZE)
			{
				i=LOCAL_HEADER_SIZE-lhptr;
				if(i>nread)i=nread;
				memcpy(lhbuf+lhptr,blocksrc,i);
				lhptr+=i;
				unitpos+=i;
				nread-=i;
				if(lhptr<LOCAL_HEADER_SIZE)continue;
			}
			namelen=lhbuf[4]+(lhbuf[5]<<8);
			if(nameptr<namelen)
			{
				i=namelen-nameptr;
				if(i>nread)i=nread;
				nameptr+=i;
				unitpos+=i;
				continue;
			}
			if((unsigned)nread>togo)nread=togo;
			if(!g_e8t)
			{
				g_crc=calc_crc(blocksrc,nread,g_crc);
				written=fwrite(blocksrc,1,nread,outfile);
			}
			else
			{
				i=e8ut_crc_write(outfile,blocksrc,nread,e8offset,srcsize,(unsigned)nread==togo);
				if(i<0)return i;
				written=nread;
				e8offset+=nread;
			}
			togo-=nread;
			unitpos+=nread;
		}
		if(togo)
		{
			g_exerror=1;
			if(g_exlast)return 0;
			g_blockstart+=g_blocksize;
			g_unitoffset+=g_blocksize;
			fpos=ftell(imp);
			init_getbits();
			method=getbits(4)&15;
			USEDBITS(4);
			if(method>METHOD_LZ77_MM)return -3;
			g_exlast=(unsigned char)getbits(1)&1;
			USEDBITS(1);
			g_blocksize=getbits(20)&((1L<<20)-1);
			USEDBITS(20);
			block_csize=getbits(20)&((1L<<20)-1);
			USEDBITS(20);
			if(!g_blockbuffer)
			{
				g_exmemory=g_blocksize+512;
				g_blockbuffer=new unsigned char[g_exmemory];
			}
			if(method==METHOD_LZ77 || method==METHOD_LZ77_MM)
			{
				if(g_exprev)
				{
					level=getbits(2)&3;
					USEDBITS(2);
					if(level>2)return -3;
					ovlevel=getbits(3)&7;
					USEDBITS(3);
					overlap=g_overlaps[level][ovlevel];
					redo_mm(overlap);
					if(g_blockstart>overlap)
					{
						memmove(g_blockbuffer,g_blockbuffer+g_blockstart-overlap,overlap);
						g_blockstart=overlap;
					}
					if(g_blocksize+g_blockstart>g_exmemory)
					{
						unsigned long old_ex = g_exmemory;
						g_exmemory=g_blocksize+g_blockstart+512;
						unsigned char *t = new unsigned char[g_exmemory];
						memcpy(t,g_blockbuffer,old_ex);
						g_blockbuffer=t;
					}
				}
				else USEDBITS(5);
				written=expand_lz(g_blockbuffer,g_blockstart,g_blocksize,1);
				if(written<0)return written;
				if((unsigned long)written<g_blocksize){g_blocksize=(unsigned long)written;g_exlast=1;}
			}
			else
			{
				if(g_blocksize>g_exmemory)
				{
					unsigned long old_ex = g_exmemory;
					g_exmemory=g_blocksize+512;
					unsigned char *t = new unsigned char[g_exmemory];
					memcpy(t,g_blockbuffer,old_ex);
					g_blockbuffer=t;
				}
				if(!g_exlast)
				{
					overlap=524288-g_blocksize;
					if((int)overlap<0)overlap=0;
					else if(overlap>g_exmemory-g_blocksize)overlap=g_exmemory-g_blocksize;
					redo_mm(overlap);
					if(g_blockstart>overlap)
					{
						memmove(g_blockbuffer,g_blockbuffer+g_blockstart-overlap,overlap);
						g_blockstart=overlap;
					}
				}
				else g_blockstart=0;
				g_mmptr=0;
				if(method==METHOD_BWT)
				{
					i=expand_bwt(g_blockbuffer+g_blockstart,g_blocksize);
					if(i<0)return i;
				}
				else
				{
					i=unstore(g_blockbuffer+g_blockstart,g_blocksize);
					if(i<0)return i;
					if((unsigned long)i<g_blocksize){g_blocksize=(unsigned long)i;g_exlast=1;}
				}
			}
			fseek(imp,fpos+block_csize,SEEK_SET);
			g_exerror=0;
			g_exprev=1;
		}
	}
	g_crc=~g_crc;
	return 0;
}

int CImpTool::expand_bwt(unsigned char *output, int exsize)
{
/*
 int count[257];
 unsigned char hclengths[BWTABLES][NCODES];
 unsigned long huffcodes[NCODES];
 int ttable[BWTABLES][BWTTSIZE];
 int hdchain[BWTABLES][NCODES][2];
 unsigned char mtf[256],treemtf[BWTABLES];
 unsigned long *links;
 unsigned char *dest,*output_end;
 int i,treenum,code,codes,numtrees;
 int rptbase,rpt,link,start;
 unsigned long hcode;
 unsigned char rc;
 
 rc=(unsigned char)getbits(8)&0xFF;
 USEDBITS(8);
 numtrees=getbits(3)&7;
 USEDBITS(3);
 if(numtrees<1)return -3;
 for(treenum=0;treenum<numtrees;treenum++)
  {
   i=load_hcodes(hclengths[treenum],NCODES,1,0);
   if(i<0)return i;
   make_codes(huffcodes,hclengths[treenum],NCODES);
   make_decode_tables(ttable[treenum],hdchain[treenum],huffcodes,hclengths[treenum],BWTTBITS,NCODES);
  }
 bitpos&=~7;
 for(i=0;i<256;i++)mtf[i]=i;
 for(i=0;i<BWTABLES;i++)treemtf[i]=i;
 memset(count,0,sizeof(count));
 rptbase=1;codes=WINDOW;
 dest=output;
 output_end=output+exsize+1;
 start=0; //prevent crash from invalid data
 while(dest<output_end)
  {
   if(codes==WINDOW)
    {
     hcode=getbits(numtrees);
     code=0;
     while(hcode&1){hcode>>=1;code++;}
     if(code>=numtrees)return -3;
     USEDBITS(code+1);
     treenum=treemtf[code];
     if(code)
      {
       for(;code;code--)treemtf[code]=treemtf[code-1];
       treemtf[0]=treenum;
      }
     codes=0;
     continue;
    }
   codes++;
   hcode=getbits(BWTTBITS)&(BWTTSIZE-1);
   code=ttable[treenum][hcode];
   if(code<0)
    {
     if(code==-MAX_NCODES)return -3;
     hcode=getbits(MAXCODELEN)>>BWTTBITS;
     code=-code;
     do
      {
       code=hdchain[treenum][code][hcode&1];
       hcode>>=1;
      }
     while(code>0);
     if(code==-MAX_NCODES)return -3;
     code=-code;
    }
   USEDBITS(hclengths[treenum][code]);
   if(code<2)
    {
     rpt=rptbase<<code;
     if(dest+rpt>output_end)rpt=output_end-dest;
     i=mtf[0];
     count[i+1]+=rpt;
     while(rpt--)*dest++=i;
     rptbase<<=1;
    }
   else if(code<257)
    {
     i=mtf[--code];
     *dest++=i;
     for(;code;code--)mtf[code]=mtf[code-1];
     mtf[0]=i;
     count[i+1]++;
     rptbase=1;
    }
   else
    {
     start=dest-output;
     dest++;
     rptbase=1;
    }
  }
 count[0]=1;
 for(i=1;i<256;i++)count[i]+=count[i-1];
 links=new unsigned long[exsize+1];
 for(link=0;link<=exsize;link++)
   if(link!=start)links[link]=(output[link]<<24)|count[output[link]]++;
 links[start]=0;
 for(link=0,dest=output_end-2;dest>=output;dest--)
  {
   link=links[link&0xFFFFFF];
   *dest=(link>>24)+rc;
  }
 delete [] links;
 return 0;
*/

 int count[257];
 unsigned char hclengths[BWTABLES][NCODES];
 unsigned long huffcodes[NCODES];
 int ttable[BWTABLES][BWTTSIZE];
 int hdchain[BWTABLES][NCODES][2];
 unsigned char mtf[256],treemtf[BWTABLES];
 unsigned long *links;
 unsigned char *dest,*output_end;
 int i,treenum,code,codes,numtrees;
 int rptbase,rpt,link,start;
 unsigned long hcode;
 unsigned char rc;
 
 rc=(unsigned char)getbits(8)&0xFF;
 USEDBITS(8);
 numtrees=getbits(3)&7;
 USEDBITS(3);
 if(numtrees<1)return -3;
 for(treenum=0;treenum<numtrees;treenum++)
  {
   i=load_hcodes(hclengths[treenum],NCODES,1,0);
   if(i<0)return i;
   make_codes(huffcodes,hclengths[treenum],NCODES);
   make_decode_tables(ttable[treenum],hdchain[treenum],huffcodes,hclengths[treenum],BWTTBITS,NCODES);
  }
 bitbuf>>=bitpos&7;
 bitpos&=~7;
 for(i=0;i<256;i++)mtf[i]=i;
 for(i=0;i<BWTABLES;i++)treemtf[i]=i;
 memset(count,0,sizeof(count));
 rptbase=1;codes=WINDOW;
 dest=output;
 output_end=output+exsize+1;
 start=0; //prevent crash from invalid data
 while(dest<output_end)
  {
   if(codes==WINDOW)
    {
     hcode=getbits(numtrees);
     code=0;
     while(hcode&1){hcode>>=1;code++;}
     if(code>=numtrees)return -3;
     USEDBITS(code+1);
     treenum=treemtf[code];
     if(code)
      {
       for(;code;code--)treemtf[code]=treemtf[code-1];
       treemtf[0]=treenum;
      }
     codes=0;
     continue;
    }
   codes++;
   hcode=getbits(BWTTBITS)&(BWTTSIZE-1);
   code=ttable[treenum][hcode];
   if(code<0)
    {
     if(code==-MAX_NCODES)return -3;
     hcode=getbits(MAXCODELEN)>>BWTTBITS;
     code=-code;
     do
      {
       code=hdchain[treenum][code][hcode&1];
       hcode>>=1;
      }
     while(code>0);
     if(code==-MAX_NCODES)return -3;
     code=-code;
    }
   USEDBITS(hclengths[treenum][code]);
   if(code<2)
    {
     rpt=rptbase<<code;
     if(dest+rpt>output_end)rpt=output_end-dest;
     i=mtf[0];
     count[i+1]+=rpt;
     while(rpt--)*dest++=i;
     rptbase<<=1;
    }
   else if(code<257)
    {
     i=mtf[--code];
     *dest++=i;
     for(;code;code--)mtf[code]=mtf[code-1];
     mtf[0]=i;
     count[i+1]++;
     rptbase=1;
    }
   else
    {
     start=dest-output;
     dest++;
     rptbase=1;
    }
  }
 count[0]=1;
 for(i=1;i<256;i++)count[i]+=count[i-1];
 links=new unsigned long[exsize+1];
 for(link=0;link<=exsize;link++)
   if(link!=start)links[link]=(output[link]<<24)|count[output[link]]++;
 links[start]=0;
 for(link=0,dest=output_end-2;dest>=output;dest--)
  {
   link=links[link&0xFFFFFF];
   *dest=(link>>24)+rc;
  }
 delete [] links;
 return 0;
}

int CImpTool::expand_lz(unsigned char *output, int start, int exsize, int mm)
{
 unsigned long huffcodes[MAX_NCODES];
 int llcode_table[LLTTSIZE],distcode_table[3][DTTSIZE];
 int llcode_chain[NLLCODES][2],distcode_chain[3][NDCODES][2];
 unsigned long dist_base[NDCODES-6],dist_mask[NDCODES-6];
 unsigned char dist_extra[NDCODES-6],len_base[NLLCODES-268],len_mask[NLLCODES-268],
   len_extra[NLLCODES-268];
 unsigned char *dest,*output_end;
 int i,j,treenum,code,num_dist_trees,dist_split,numcodes,numtrees;
 unsigned long len;
 unsigned long hcode,dist,last_dist,last_dist2;
 unsigned char *code_lengths,*codelen_src;

 if(mm)g_mmptr=0;
 num_dist_trees=getbits(2)&3;
 USEDBITS(2);
 if(!num_dist_trees)return -3;
 dist_split=num_dist_trees>2;
 numcodes=NLLCODES+NDCODES+ND2CODES;
 if(dist_split)numcodes+=ND3CODES;
 dest=output+start;
 output_end=dest+exsize;
 numtrees=getbits(6);
 USEDBITS(6);
 numtrees&=63;
 if(numtrees)code_lengths=new unsigned char[numcodes*numtrees];
 else code_lengths=new unsigned char[MIN_LZ_CODES];

 i=load_hcodes(code_lengths,numcodes,numtrees,start+exsize);
 if(i<0){delete [] code_lengths;return i;}
 for(i=2,len=11,code=0; code<NLLCODES-268; i<<=1)
   for(j=code+4; code<j; len+=i)len_base[code++]=(unsigned char)len;
 for(i=1,code=0; code<NLLCODES-268; i++)
   for(j=code+4; code<j; code++)len_extra[code]=i;
 for(code=0,len=1; code<NLLCODES-268; len=(len<<1)+1)
   for(j=code+4; code<j; code++)len_mask[code]=(unsigned char)len;
 for(hcode=2,dist=5,code=0; code<NDCODES-6; hcode<<=1)
  {
   dist_base[code]=dist; dist+=hcode;
   dist_base[code+1]=dist; dist+=hcode;
   code+=2;
  }
 for(i=1,code=0; code<NDCODES-6; i++,code+=2)
   dist_extra[code]=dist_extra[code+1]=i;
 for(code=0,dist=1; code<NDCODES-6; dist=(dist<<1)+1,code+=2)
   dist_mask[code]=dist_mask[code+1]=dist;
 codelen_src=code_lengths-numcodes;
 last_dist=0;last_dist2=0;
 code=256;
 numtrees+=!numtrees;
 while(dest<output_end)
  {
   if(code==256)
    {
     if(!numtrees)break;
     numtrees--;
     codelen_src+=numcodes;
     make_codes(huffcodes,codelen_src,NLLCODES);
     make_decode_tables(llcode_table,llcode_chain,huffcodes,codelen_src,LLTTBITS,NLLCODES);
     codelen_src+=NLLCODES;
     for(i=0;i<num_dist_trees;i++)
      {
       make_codes(huffcodes,codelen_src,g_ncodes[i]);
       make_decode_tables(distcode_table[i],distcode_chain[i],huffcodes,codelen_src,DTTBITS,g_ncodes[i]);
       codelen_src+=g_ncodes[i];
      }
     codelen_src-=numcodes;
     code=0;
    }
   code=llcode_table[getbits(LLTTBITS)&(LLTTSIZE-1)];
   if(code<0)
    {
     if(code==-MAX_NCODES)break;
     hcode=getbits(MAXCODELEN)>>LLTTBITS;
     code=-code;
     do
      {
       code=llcode_chain[code][hcode&1];
       hcode>>=1;
      }
     while(code>0);
     if(code==-MAX_NCODES)break;
     code=-code;
    }
   USEDBITS(codelen_src[code]);
   if(code<256)*dest++=code;
   else if(code>256)
    {
     if(code<266)len=code-255;
     else if(code<LONGMATCHCODE)
      {
       len=getbits(5);
       USEDBITS(len_extra[code-266]);
       len=(len&len_mask[code-266])+len_base[code-266];
      }
     else if(code==LONGMATCHCODE)
      {
       len=(getbits(LONGMATCHBITS)&LONGMATCHMASK)+259;
       USEDBITS(LONGMATCHBITS);
      }
     else
      {
       len=getbits(3)&7;
       USEDBITS(3);
       g_mmlist[g_mmptr++]=(dest-output)|(len<<24);
       continue;
      }
     treenum=(len==2)+((dist_split&(len==3))<<1);
     code=distcode_table[treenum][getbits(DTTBITS)&(DTTSIZE-1)];
     if(code<0)
      {
       if(code==-MAX_NCODES)break;
       hcode=getbits(MAXCODELEN)>>DTTBITS;
       code=-code;
       do
        {
         code=distcode_chain[treenum][code][hcode&1];
         hcode>>=1;
        }
       while(code>0);
       if(code==-MAX_NCODES)break;
       code=-code;
      }
     USEDBITS(codelen_src[code+g_ncoffset[treenum]]);
     if(!code)dist=last_dist;
     else if(code==1)
      {
       dist=last_dist2;
       last_dist2=last_dist;
       last_dist=dist;
      }
     else
      {
       if(code<6)dist=code-1;
       else
        {
         dist=getbits(18);
         USEDBITS(dist_extra[code-6]);
         dist=(dist&dist_mask[code-6])+dist_base[code-6];
         last_dist2=last_dist;
         last_dist=dist;
        }
      }
     if(dist<=(unsigned long)(dest-output))
       while(len-- && dest<output_end)*dest=dest[-(signed)dist],dest++;
    }
  }
 delete [] code_lengths;
 if(mm)reverse_mm(output,start+exsize);
 return dest-output-start;
}

int CImpTool::load_hcodes(unsigned char *hclengths, int ncodes, int ntrees, int exsize)
{
 int i,j,dzrpt,zrpt,len,hcode,code;
 unsigned char hlclens[NLENCODES];
 unsigned char *hcldest,*hclend,*hclrpt;
 unsigned long huffcodes[NLENCODES];
 int ttable[HCTTSIZE];
 int hdchain[NLENCODES][2];

 if(!ntrees){load_fixed_codes(hclengths,exsize);return 0;}
 memset(hlclens,0,NLENCODES);
 for(i=0,j=1;i<NLENCODES;)
  {
   code=getbits(5)&31;
   USEDBITS(5);
   if(code<2)
    {i+=j<<code;
     j<<=1;}
   else
    {if(code>MAXCODELEN+1)return -3;
     hlclens[i++]=code-1;
     j=1;}
  }
 make_codes(huffcodes,hlclens,NLENCODES);
 make_decode_tables(ttable,hdchain,huffcodes,hlclens,HCTTBITS,NLENCODES);
 hcldest=hclengths;
 hclend=hclengths+ncodes*ntrees;
 hclrpt=hclengths+ncodes;
 len=0;dzrpt=1;zrpt=1;
 while(hcldest<hclend)
  {
   code=ttable[getbits(HCTTBITS)&(HCTTSIZE-1)];
   if(code<0)
    {
     if(code==-MAX_NCODES)return -3;
     hcode=getbits(MAXCODELEN)>>HCTTBITS;
     code=-code;
     do
      {
       code=hdchain[code][hcode&1];
       hcode>>=1;
      }
     while(code>0);
     if(code==-MAX_NCODES)return -3;
     code=-code;
    }
   USEDBITS(hlclens[code]);
   if(code>=DZRUN)
    {
     code=dzrpt<<(code-DZRUN);
     while(code-- && hcldest<hclend)
      {
       if(hcldest>=hclrpt && hcldest[-ncodes])len=hcldest[-ncodes];
       *hcldest++=len;
      }
     dzrpt<<=1;
     zrpt=1;
    }
   else if(code>=ZRUN)
    {
     code=zrpt<<(code-ZRUN);
     while(code-- && hcldest<hclend)*hcldest++=0;
     zrpt<<=1;
     dzrpt=1;
    }
   else
    {
     if(hcldest>=hclrpt && hcldest[-ncodes])len=code+hcldest[-ncodes];
     else len+=code;
     if(len>MAXCODELEN)len-=MAXCODELEN+1;
     *hcldest++=len;
     dzrpt=1;zrpt=1;
    }
  }
 for(hcldest=hclengths;hcldest<hclend;hcldest++)if(*hcldest>MAXCODELEN)return -3;
 return 0;
}

void CImpTool::reverse_mm(unsigned char *block,int bsize)
{
 int m,d;
 unsigned char *src,*end;
 
 for(m=0;m<g_mmptr;m++)
  {
   d=-(signed)(g_mmlist[m]>>24);
   if(!d)continue;
   src=block+(g_mmlist[m]&0xFFFFFF);
   end=block+bsize;
   if(m<g_mmptr-1)end=block+(g_mmlist[m+1]&0xFFFFFF);
   while(src<end)*src+=src[d],src++;
  }
}

void CImpTool::redo_mm(unsigned long overlap)
{
 int mmptr,d;
 unsigned char *start,*src,*end;
 
 start=g_blockbuffer+g_blockstart-overlap;
 if(start<g_blockbuffer)start=g_blockbuffer;
 for(mmptr=g_mmptr-1;mmptr>=0;mmptr--)
  {
   d=-(signed)(g_mmlist[mmptr]>>24);
   if(!d)continue;
   end=g_blockbuffer+(g_mmlist[mmptr]&0xFFFFFF);
   if(end<start)end=start;
   src=g_blockbuffer+g_blockstart;
   if(mmptr<g_mmptr-1)src=g_blockbuffer+(g_mmlist[mmptr+1]&0xFFFFFF);
   for(src--;src>=end;src--)*src-=src[d];
   if(end<=start)break;
  }
}


void CImpTool::make_codes(unsigned long *huffcodes,unsigned char *hclengths,int ncodes)
{
 unsigned long i,j,k,l,count[MAXCODELEN+1],nextcode[MAXCODELEN+1];

 memset(count,0,sizeof(count));
 for(i=0;i<(unsigned)ncodes;i++)count[hclengths[i]]++;
 count[0]=0;
 for(i=1,k=0;i<=MAXCODELEN;i++)
  {
   k=(k+count[i-1])<<1;
   nextcode[i]=k;
  }
 for(i=0;i<(unsigned)ncodes;i++)
  {
   j=hclengths[i];
   if(j)
    {
     l=0;
     k=nextcode[j]++;
     for(;j>0;j--)
      {
       l<<=1;
       l+=k&1;
       k>>=1;
      }
     huffcodes[i]=l;
    }
  }
}

int CImpTool::make_decode_tables(int *ttable, int hdchain[][2], unsigned long *huffcodes,unsigned char *hclengths, int ttbits, int ncodes)
{
 int hdnode,hdndest,i,ttsize;
 unsigned long base,n,hcode;

 ttsize=1<<ttbits;
 if(hdchain)for(i=0;i<ncodes;i++)hdchain[i][0]=hdchain[i][1]=-MAX_NCODES;
 for(i=0;i<ttsize;i++)ttable[i]=-MAX_NCODES;
 hdndest=1;
 for(i=0;i<ncodes;i++)if(hclengths[i])
  {
   base=1<<hclengths[i];
   n=huffcodes[i];
   if(base<=(unsigned)ttsize)
    {
     while(n<(unsigned)ttsize) /*n should be less than ttsize but it might be invalid*/
      {
       ttable[n]=i;
       n+=base;
      }
    }
   else if(hdchain)
    {
     base=hclengths[i]-ttbits-1;
     n&=ttsize-1;
     hcode=huffcodes[i]>>ttbits;
     if(ttable[n]==-MAX_NCODES)
      {
       ttable[n]=-hdndest;
       hdnode=hdndest++;
      }
     else
      {
       hdnode=-ttable[n];
       while(hdchain[hdnode][hcode&1]>=0)
        {
         hdnode=hdchain[hdnode][hcode&1];
         hcode>>=1;
         base--;
        }
      }
     while(base--)
      {
       if(hdndest==ncodes)return -3;
       hdchain[hdnode][hcode&1]=hdndest;
       hdnode=hdndest++;
       hcode>>=1;
      }
     hdchain[hdnode][hcode&1]=-i;
    }
  }
 return 0;
}

void CImpTool::load_fixed_codes(unsigned char *hclens,int exsize)
{
 memset(hclens,8,244);
 memset(hclens+244,9,17);
 hclens[256]=0;
 memset(hclens+261,10,5);
 memset(hclens+266,11,NLLCODES-266);
 if(exsize<=8192)
  {memset(hclens+NLLCODES,4,4);
   memset(hclens+NLLCODES+4,5,24);
   memset(hclens+NLLCODES+28,0,NDCODES-28);}
 else
  {memset(hclens+NLLCODES,5,22);
   memset(hclens+NLLCODES+22,6,NDCODES-22);}
 memset(hclens+NLLCODES+NDCODES,4,ND2CODES);
 hclens[NLLCODES+NDCODES]=3;
 hclens[NLLCODES+NDCODES+1]=3;
}



int CImpTool::e8ut_crc_write(FILE *outfile,unsigned char *block,int size,unsigned long e8offset,unsigned long srcsize,int last)
{
 int i,w;
 unsigned char *src,*src2,*end;

 if(g_e8extra)
  {
   i=sizeof(g_e8buf)-g_e8extra;
   memcpy(g_e8buf+g_e8extra,block,i>size ? size : i);
   if(i>size)block=g_e8buf,size+=g_e8extra,e8offset-=g_e8extra;
   else
    {
     if(g_e8t==2)i=e8ut_crc_wr16(outfile,g_e8buf,g_e8extra,e8offset-g_e8extra);
     else i=e8ut_crc_wr32(outfile,g_e8buf,g_e8extra,e8offset-g_e8extra,srcsize);
     if(i<0)return i;
     block+=i;
     e8offset+=i;
     size-=i;
    }
  }
 if(g_e8t==2)i=e8ut_crc_wr16(outfile,block,size-g_e8t,e8offset);
 else i=e8ut_crc_wr32(outfile,block,size-g_e8t,e8offset,srcsize);
 if(i<0)return i;
 end=block+size;
 src=end-g_e8t+i;
 src2=src;
 while(src2<end && (*src2!=0xE8 || last))g_crc=crctbl[(unsigned char)g_crc^*src2++]^(g_crc>>8);
 if(src2>src)
  {
   w=fwrite(src,1,src2-src,outfile);
   if(w<src2-src)return -3;
  }
 g_e8extra=end-src2;
 if(g_e8extra)memmove(g_e8buf,src2,g_e8extra);
 return 0;
}

int CImpTool::e8ut_crc_wr16(FILE *outfile,unsigned char *block,int size,unsigned long e8offset)
{
 unsigned char buffer[16384];
 int w;
 unsigned int u;
 unsigned char *src,*end,*dest;
 unsigned char c;

 src=block;
 end=block+size;
 dest=buffer;
 while(src<end)
  {
   if(end-src>sizeof(buffer)-3)end=src+sizeof(buffer)-3;
   while(src<end)
    {
     c=*dest++=*src++;
     g_crc=crctbl[(unsigned char)g_crc^c]^(g_crc>>8);
     if(c==0xE8)
      {
       u=(src[0]+(src[1]<<8))-(unsigned short)(e8offset+(src-block));
       *dest=(unsigned char)u;
       dest[1]=(unsigned char)(u>>8);
       src+=2;
       g_crc=crctbl[(unsigned char)g_crc^*dest++]^(g_crc>>8);
       g_crc=crctbl[(unsigned char)g_crc^*dest++]^(g_crc>>8);
      }
    }
   w=fwrite(buffer,1,dest-buffer,outfile);
   dest=buffer;
   end=block+size;
  }
 return src-end;
}

int CImpTool::e8ut_crc_wr32(FILE *outfile,unsigned char *block,int size,unsigned long e8offset,unsigned long srcsize)
{
 unsigned char buffer[16384];
 int i,j,w;
 unsigned char *src,*end,*end2,*dest;
 unsigned char c;

 src=block;
 end=block+size;
 dest=buffer;
 if(end-src>sizeof(buffer)-5)end=src+sizeof(buffer)-5;
 while(src<end)
  {
   if(end-src>sizeof(buffer)-5)end=src+sizeof(buffer)-5;
   while(src<end)
    {
     c=*dest++=*src++;
     g_crc=crctbl[(unsigned char)g_crc^c]^(g_crc>>8);
     if(c==0xE8)
      {
       i=src[0]+(src[1]<<8)+(src[2]<<16)+(src[3]<<24);
	   j=e8offset+(src-block);
       if(i<0 && i>=-j)i=srcsize-i-j-1;
       else if((unsigned int)i<(unsigned int)srcsize)i-=j;
       src+=4;
       for(end2=dest+4;dest<end2;dest++,i>>=8)
        {
         *dest=(unsigned char)i;
         g_crc=crctbl[(unsigned char)g_crc^*dest]^(g_crc>>8);
        }
      }
    }
   w=fwrite(buffer,1,dest-buffer,outfile);
   dest=buffer;
   end=block+size;
  }
 return src-end;
}
