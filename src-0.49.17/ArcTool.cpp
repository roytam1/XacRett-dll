
#include "stdafx.h"
#include "ArcTool.h"
#include "complib.h"

#define NO_PRED  0xFFFF
#define EMPTY    0xFFFF
#define NOT_FND  0xFFFF

bool CArcTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<29 )
		return false;

	if( hdr[0]==0x1a )
	{
		if( hdr[1]==0 || (hdr[1]>0x0b && hdr[1]!=0x1e) )
			return false;
		int j;
		for( j=2; j!=15; j++ )
			if( hdr[j]==0 )
				break;
		if( j==15 || j==2 )
			return false;
		return true;
	}

	if( hdr[0]=='M' && hdr[1]=='Z' && siz>5000 )
	{
		arc = fopen( fname,"rb" );
		if( !arc ) return false;
		int ans = SkipSfxStub( hdr+5000,
			siz>10000 ? 5000 : siz-5000, 5000 );
		fclose( arc );
		return (ans!=-1);
	}
	return false;
}

bool CArcTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	arc = fopen(fname,"rb");
	if( arc==NULL )return false;
	*path=0;
	pass=NULL;

	// ÉwÉbÉ_ÇíTÇ∑Å`
	int hpos=0,length=fread( buf,1,XACR_BUFSIZE,arc);
	if( length==0 )
	{fclose(arc);return false;}
	if( buf[0]!=0x1a )
	{
		if( -1==(hpos=SkipSfxStub( buf+1,length-1,1 )) )
		{fclose(arc);return false;}
	}
	fseek( arc,hpos,SEEK_SET );

	// ìWäJ
	while( ReadHeader() )
	{
		unsigned long curpos = ftell(arc);
		unsigned long nextpos = curpos+hdr.csize;

retry:
		char temp[MAX_PATH];
		strcpy(temp,path);
		strcat(temp,hdr.name);

		if (m_Endflg) return true;//í«â¡ By Bonty
		if (!CheckStr(temp))//í«â¡ By Bonty
		{
			fseek(arc,nextpos,SEEK_SET);
			continue;
		}
		char* outname = kiutil::pathMake(temp);

		out = fopen(outname,"wb");
		if( out!=NULL )
		{
			// Ç¢ÇÎÇ¢ÇÎèâä˙âª
			ppos=pass;
			myfwrite_r( NULL,-1 );
			crc=0;

			switch(hdr.method)
			{
			case  1:
			case  2:
			case  3:	Unstore();	break;
			case  4:	UnSqueeze();break;
			case  5:
			case  6:
			case  7:	UnLz();		break;
			case  8:
			case  9:	UnLzw();	break;
//			case 10:	UnCrush();	break;
//			case 11:	UnDistill();break;
			}
			fclose(out);
			kiutil::timeSet( outname,hdr.date,hdr.time );
			// password????
			if( hdr.crc!=crc )
			{
				DeleteFile(outname);
				if( pass==NULL && 
					(hdr.method<4||hdr.method==8||hdr.method==9) )
				{
					pass=GetPassword();
					for( char* tmp=pass; *tmp!=0; tmp++ )
						if( 'a'<=*tmp && *tmp<='z' )
							*tmp+='A'-'a';
					fseek(arc,curpos,SEEK_SET);
					goto retry;
				}
			}
		}

		fseek(arc,nextpos,SEEK_SET);
	}

	fclose(arc);
	return true;
}

int CArcTool::SkipSfxStub( const BYTE* dat,unsigned long siz,int curpos )
{
	for( int i=0; i<(signed)siz-29; i++ )
	{
		const BYTE* hdrbuf=dat+i;

		// Magic
		if( hdrbuf[0]!=0x1a )
			continue;

		// Method
		if( hdrbuf[1]==0 || (hdrbuf[1]>0x0b && hdrbuf[1]!=0x1e) )
			continue;

		// read filename
		int j;
		for( j=2; j!=15; j++ )
			if( hdrbuf[j]==0 )
				break;
		if( j==15 || j==2 )
			continue;

		unsigned long csize =
			(hdrbuf[15])|(hdrbuf[16]<<8)|
			(hdrbuf[17]<<16)|(hdrbuf[18]<<24);
		fseek(arc,curpos+i+29+csize,SEEK_SET);
		if( fgetc(arc)!=0x1a )
			continue;

		// OK,header found.
		return curpos+i;
	}
	return -1;
}

bool CArcTool::ReadHeader()
{
	BYTE hdrbuf[29];

// Magic & Method
	if( 2!=fread(hdrbuf,1,2,arc) )
		return false;
	if( hdrbuf[0]!=0x1a )
		return false;

	// end of arc
	if( hdrbuf[1]==0 )
		return false;
	// end of folder
	else if( hdrbuf[1]==0x1f )
	{
		char *p,*pp,*ppp;
		for( p=path,pp=path,ppp=path; *p!=0; p=CharNext(p) )
			if( *p=='\\' )
				{ppp=pp;pp=p+1;}
		*ppp=0;
		return ReadHeader();
	}
	// unknown method
	else if( hdrbuf[1]>0x0b && hdrbuf[1]!=0x1e )
		return false;

// Other Data
	if( hdrbuf[1]==1 )
	{
		if( 23!=fread(hdrbuf+2,1,23,arc) )
			return false;
		hdrbuf[25]=hdrbuf[15],hdrbuf[26]=hdrbuf[16],
		hdrbuf[27]=hdrbuf[17],hdrbuf[28]=hdrbuf[18];
	}
	else
	{
		if( 27!=fread(hdrbuf+2,1,27,arc) )
			return false;
	}

	// read filename
	int j;
	for( j=2; j!=15; j++ )
		if( (hdr.name[j-2]=(char)hdrbuf[j])==0 )
			break;
	if( j==15 )
		return false;

	// folder start
	if( hdrbuf[1]==0x1e )
	{
		strcat(path,hdr.name);
		strcat(path,"\\");
		return ReadHeader();
	}

	hdr.method= hdrbuf[1];
	hdr.csize = (hdrbuf[15]) | (hdrbuf[16]<<8) |
				(hdrbuf[17]<<16) | (hdrbuf[18]<<24);
	hdr.date  = (hdrbuf[19]) | (hdrbuf[20]<<8);
	hdr.time  = (hdrbuf[21]) | (hdrbuf[22]<<8);
	hdr.crc   = (hdrbuf[23]) | (hdrbuf[24]<<8);
	hdr.usize = (hdrbuf[25]) | (hdrbuf[26]<<8) |
				(hdrbuf[27]<<16) | (hdrbuf[28]<<24);

	return true;
}

/********************************************************/

const static unsigned short crctab[] = {
0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,};

void CArcTool::myfputc(int n)
{
	fputc(n,out);
	crc = ((crc>>8)&0xff)^crctab[(crc^((BYTE)n))&0xff];
}
void CArcTool::myfputc_r(int n)
{
	BYTE m=(BYTE)n;
	myfwrite_r(&m,1);
}
void CArcTool::myfwrite(BYTE* buffer,int len)
{
	fwrite(buffer,1,len,out);
	while(len--)
		crc = ((crc>>8)&0xff)^crctab[(crc^*buffer++)&0xff];
}
void CArcTool::myfwrite_r(BYTE* buffer,int len)
{
	// Initializing-Command
	if( len<0 ){prevc=0,repeating=false;return;}
	// Normal
	for( int i=0; i!=len; i++ )
	{
		if( repeating )
		{
			if( buffer[i]==0 )
				myfputc( prevc=0x90 );
			else
				while( --buffer[i] )
					myfputc( prevc );
			repeating=false;
		}
		else
		{
			if( buffer[i]!=0x90 )	myfputc( prevc=buffer[i] );
			else					repeating = true;
		}
	}
}
int CArcTool::myfgetc()
{
	int c=fgetc(arc);
	if( c==-1 )return -1;
	char a=c;
	if( pass!=NULL )codebuf(&a,1);
	return (BYTE)a;
}
int CArcTool::myfread(BYTE* buffer,int len)
{
	int ans=fread(buffer,1,len,arc);
	if( ans<=0 )return 0;
	if( pass!=NULL )
		codebuf((char*)buffer,ans);
	return ans;
}
void CArcTool::codebuf(char* buffer,int len)
{
	for( int i=0; i!=len; i++ )
	{
		if( !*ppos )
			ppos = pass;
		*buffer++ ^= *ppos++;
	}
}

/***************************************/

void CArcTool::Unstore()
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	int how_much;
	int sz=hdr.csize;

	while( sz>0 )
	{
		how_much = sz>XACR_BUFSIZE?XACR_BUFSIZE:sz;
		if( !(how_much=myfread( buf,how_much )) )
			break;
		if( hdr.method==3 )	myfwrite_r( buf,how_much );
		else				myfwrite( buf,how_much );
		sz -= how_much;
	}
}

/***************************************/

struct nd
{
	int child[2];
};
#define SPEOF 256
#define NUMVALS 257

void CArcTool::UnSqueeze()
{
	// initialize ///////////////////
	nd node[NUMVALS];
	int bpos = 99,curin = 0;

	int numnodes = (myfgetc()|(myfgetc()<<8));
	if( numnodes<0 || numnodes>=NUMVALS )
		return;// error

	node[0].child[0] = -(SPEOF+1);
	node[0].child[1] = -(SPEOF+1);

	int i;
	for( i=0; i!=numnodes; i++ )
	{
		node[i].child[0] = (myfgetc()|(myfgetc()<<8));
		node[i].child[1] = (myfgetc()|(myfgetc()<<8));
	}

	// un /////////////////////////////
	for( unsigned int j=0; j!=hdr.usize; j++ )
	{
		for( i=0; i>=0; )
		{
			if( ++bpos>7 )
			{
				curin = (myfgetc()|(myfgetc()<<8));
				bpos = 0;
				i = node[i].child[ 1&curin ];
			}
			else
				i = node[i].child[ 1&(curin>>=1) ];
		}
		i = -(i+1);
		if( i==SPEOF )
			break;
		myfputc_r(i);
	}
}

/***************************************/

int CArcTool::tolzw(void* buffer,int size,unsigned long param)
{
	CArcTool* p=(CArcTool*)param;
	if( p->lzwsize<=0 )return 0;
	int len = (int)p->lzwsize>size ? size : p->lzwsize;
	len = p->myfread((BYTE*)buffer,len);
	if( len==-1 )len=0;
	p->lzwsize -= len;
	return len;
}

void CArcTool::UnLzw()
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	lzwsize=hdr.csize;

	int bits=13;
	if( hdr.method==8 )
		if( (lzwsize--)==0 || 12!=(bits=myfgetc()) )
			return;
	LZWDEC* lzw=lzw_start( bits,true,tolzw,(unsigned long)this );
	if( lzw!=NULL )
	{
		int size;
		while( size=lzw_read(buf,XACR_BUFSIZE,lzw) )
		{
			if( hdr.method==8 )
				myfwrite_r(buf,size);
			else
				myfwrite(buf,size);
		}
		lzw_end(lzw);
	}
}

/***************************************/

static unsigned short oldh(unsigned pred,unsigned char foll)
{long local = ((pred+foll)|0x0800)&0xFFFF;
local *= local;return (local>>6) & 0x0FFF;}
static unsigned short newh(unsigned pred,unsigned char foll)
{return (((pred+foll)&0xFFFF)*15073)&0xFFF;}

#define	PUSH(c)	stack[sp++]=((char)(c))
#define POP() ((sp>0)?(int)stack[--sp]:EMPTY)

#define	GOCODE(x)\
if( (inflag^=1) ){x=(inbuf<<4);x|=((inbuf=myfgetc())>>4);} \
else{x=(inbuf&0x0f)<<8;x|=myfgetc();inbuf=myfgetc();}

unsigned short CArcTool::eolist(unsigned short index)
{
	int temp;
	while( temp=string_tab[index].next )
		index = temp;
	return index;
}

unsigned short CArcTool::hash(unsigned short pred,unsigned char foll)
{
	unsigned short local,tempnext;
	arc_entry *ep;

	local = (*h)(pred,foll);
	if( !string_tab[local].used )
		return local;

	local = eolist(local);
	tempnext = (local+101) & 0x0FFF;
	ep = &string_tab[tempnext];
	while( ep->used )
	{
		if( ++tempnext==ARC_TABSIZE )
		{
			tempnext = 0;
			ep = string_tab;
		}
		else
			++ep;
	}
	string_tab[local].next = tempnext;
	return tempnext;
}

void CArcTool::upd_tab(unsigned short pred,unsigned char foll)
{
	arc_entry *ep=&string_tab[hash(pred,foll)];
	ep->used = TRUE;
	ep->next = 0;
	ep->predecessor = pred;
	ep->follower = foll;
}

void CArcTool::UnLz()
{
	// init
	int code_count,oldcode,code,newcode,finchar,sp=0;
	arc_entry *ep;
	unsigned long lzsize=hdr.usize;
	unsigned char stack[1<<13];
	int inflag=0;
	unsigned char inbuf=myfgetc();

	h = (hdr.method==7)?newh:oldh;

	memset((char*)string_tab,0,ARC_TABSIZE*sizeof(arc_entry));
	for( int i=0; i!=256; i++ )
		upd_tab(NO_PRED,i);

	code_count = ARC_TABSIZE - 256;

	GOCODE(oldcode);

	finchar = string_tab[oldcode].follower;
	if( lzsize-- )return;
	if( hdr.method==5 )	myfputc(finchar);
	else				myfputc_r(finchar);

	// de
	while(true)
	{
		while( sp )
		{
			if( lzsize-- )
				break;
			if( hdr.method==5 )	myfputc(POP());
			else				myfputc_r(POP());
		}

		GOCODE(newcode);

		code = newcode;
		ep = &string_tab[code];
		if( !ep->used )
		{
			code = oldcode;
			ep = &string_tab[code];
			PUSH(finchar);
		}
		while( ep->predecessor!=NO_PRED )
		{
			PUSH(ep->follower);
			code = ep->predecessor;
			ep = &string_tab[code];
		}

		PUSH(finchar=ep->follower);

		if( code_count )
		{
			upd_tab(oldcode,finchar);
			--code_count;
		}
		oldcode = newcode;
	}
}
