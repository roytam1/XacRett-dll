
#include "stdafx.h"
#include "CptTool.h"

#define F_FNAME          0
#define F_FOLDER        32
#define F_FOLDERSIZE	33
#define F_VOLUME        35
#define F_FILEPOS       36
#define F_FTYPE         40
#define F_CREATOR       44
#define F_CREATIONDATE  48
#define F_MODDATE       52
#define F_FNDRFLAGS     56
#define F_FILECRC       58
#define F_CPTFLAG       62
#define F_RSRCLENGTH    64
#define F_DATALENGTH    68
#define F_COMPRLENGTH   72
#define F_COMPDLENGTH   76
#define FILEHDRSIZE     80

bool CCptTool::ReadCptHdr()
{
	BYTE tmp[300];
	isMacBin=false;
	if( 8!=fread(tmp,1,8,cpt) )
		return false;
	if( *tmp!=1 )
	{
		fseek(cpt,128,SEEK_SET);
		if( 8!=fread(tmp,1,8,cpt) )
			return false;
		if( *tmp!=1 )
			return false;
		isMacBin=true;
	}

	// INDEXまで跳ぶ
	fseek( cpt, 
		((tmp[4]<<24)|(tmp[5]<<16)|(tmp[6]<<8)|tmp[7])+(isMacBin?128:0),
		SEEK_SET );
	if( 7!=fread(tmp+8,1,7,cpt) )
		return false;

	DWORD hdrcrc = ((tmp[8]<<24) | (tmp[9]<<16) | (tmp[10]<<8) | tmp[11]);
	entries = ((tmp[12]<<8) | tmp[13]);
	BYTE commentsize = tmp[14];
	DWORD cpt_crc = updcrc(0xffffffff,tmp+12,3);

	if( commentsize!=fread(tmp,1,commentsize,cpt) )
		return false;
	cpt_crc = updcrc(cpt_crc,tmp,commentsize);

	// INDEX用データ格納場所を確保
	delete [] cptindex;
	cptindex = new BYTE[entries*FILEHDRSIZE];
	if( cptindex==0 )return false;

	// INDEX取得
	BYTE* cptptr=cptindex;
	for( WORD i=0; i!=entries; i++ )
	{
		*cptptr = fgetc(cpt);
		cpt_crc = updcrc(cpt_crc,cptptr,1);
		if( *cptptr&0x80 )
		{
			cptptr[F_FOLDER] = 1;
			*cptptr &= 0x3f;
		}
		else
			cptptr[F_FOLDER] = 0;

		if( *cptptr!=fread(cptptr+1,1,*cptptr,cpt) )
			return false;
		cpt_crc = updcrc(cpt_crc,cptptr+1,*cptptr);

		if( cptptr[F_FOLDER] )
		{
			if( 2!=fread(cptptr+F_FOLDERSIZE,1,2,cpt) )
				return false;
			cpt_crc = updcrc(cpt_crc,cptptr+F_FOLDERSIZE,2);
		}
		else
		{
			if( FILEHDRSIZE-F_VOLUME
				!= fread(cptptr+F_VOLUME,1,FILEHDRSIZE-F_VOLUME,cpt) )
				return false;
			cpt_crc = updcrc(cpt_crc,cptptr+F_VOLUME,FILEHDRSIZE-F_VOLUME);
		}
		cptptr += FILEHDRSIZE;
    }

	return hdrcrc==cpt_crc;
}

void CCptTool::ReadFileHdr(BYTE* hdr)
{
	
	// 再帰するならメンバ変数じゃない方がいい？？？
	folder = hdr[32];
	if( folder )
	{
		memcpy(path,hdr+1,hdr[0]);
		path[ hdr[0] ]=0x5C;
		path[ hdr[0] +1]=0;
		//lstrcpy(path,kiutil::pathMake(path));
		foldersize = ((hdr[33]<<8)|hdr[34]);
	}else{
		lstrcpy(FileName,path);
		for(int i = 0;FileName[i]!=0x00;i++);
		memcpy(&FileName[i],hdr+1,hdr[0]);
		FileName[ hdr[0]+i ]=0;

		cptFlag  = (hdr[62]<< 8) + hdr[63];
		rsrcULen = (hdr[64]<<24) + (hdr[65]<<16) + (hdr[66]<<8) + hdr[67];
		dataULen = (hdr[68]<<24) + (hdr[69]<<16) + (hdr[70]<<8) + hdr[71];
		rsrcCLen = (hdr[72]<<24) + (hdr[73]<<16) + (hdr[74]<<8) + hdr[75];
		dataCLen = (hdr[76]<<24) + (hdr[77]<<16) + (hdr[78]<<8) + hdr[79];
		filepos  = (hdr[36]<<24) + (hdr[37]<<16) + (hdr[38]<<8) + hdr[39]
				 + (isMacBin?128:0);
    }
}

bool CCptTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
//--------------------------------------------------------------------//

	// できるだけファイルは開かずにチェック…
	if( hdr[0]==1 )
	{
		if( fsize<88 )
			return false;
		if(	fsize <
			(unsigned)((hdr[4]<<24)+(hdr[5]<<16)+(hdr[6]<<8)+hdr[7]) )
			return false;
	}
	else if( hdr[128]==1 )
	{
		if( fsize<216 )
			return false;
		if(	fsize <	(unsigned)((hdr[132]<<24)+(hdr[133]<<16)+
							   (hdr[134]<<8)+hdr[135]) )
			return false;
	}
	else
		return false;

	// 一応開いてチェック…
	cpt = fopen( fname,"rb" );
	if( !cpt )return false;
	bool ans=ReadCptHdr();
	fclose( cpt );
	return ans;
}

bool CCptTool::Extract( const char* fname,const char* ddir )
{
	cpt=fopen(fname,"rb");
	if(cpt==NULL)return false;
	if( !ReadCptHdr() )
	{fclose(cpt);return false;}

	*path = '\0';

	BYTE* cptptr = cptindex;
	for( WORD i=0; i!=entries; i++ )
	{
		ReadFileHdr(cptptr);

		/*if( folder )
		{
			int n=foldersize;
			cpt_folder( cptptr );
			i += n;
			cptptr += n * FILEHDRSIZE;
		}
		else
			cpt_uncompact();*///変更しています

		if (m_Endflg) return true;//追加 By Bonty
		if( folder )
		{
			kiutil::pathMake(path);
		}else if(CCptTool::CheckStr(CCptTool::FileName))
			cpt_uncompact();
		cptptr += FILEHDRSIZE;
	}

	fclose( cpt );
	return true;
}

void CCptTool::cpt_folder(BYTE* cptptr)
{
	// start folder
	strcat( path,FileName );
	strcat( path,"\\" );

	cptptr += FILEHDRSIZE;
	int nfiles = foldersize;

	for( int i=0; i!=nfiles; i++ )
	{
		ReadFileHdr( cptptr );

		if( folder )
		{
			int n=foldersize;
			cpt_folder( cptptr );
			i += n;
			cptptr += n * FILEHDRSIZE;
		}
		else
			cpt_uncompact();

		cptptr += FILEHDRSIZE;
	}

	// end folder
	char *p,*pp,*ppp;
	for( p=path,pp=path,ppp=path; *p!=0; p=CharNext(p) )
		if( *p=='\\' )
			{ppp=pp;pp=p+1;}
	*ppp=0;
}

#define	ESC1		0x81
#define	ESC2		0x82
#define NONESEEN	0
#define ESC1SEEN	1
#define ESC2SEEN	2

void CCptTool::cpt_uncompact()
{
	if( cptFlag&1 )return;

	if( dataULen==0 )
	{
		dataULen = rsrcULen;
		dataCLen = rsrcCLen;
		rsrcCLen = 0;
	}

	fseek( cpt,filepos+rsrcCLen,SEEK_SET );

	if( out=fopen( kiutil::pathMake(FileName),"wb" ) )
	{
		cpt_outstat = NONESEEN;
	    cpt_LZptr = 0;
	    cpt_blocksize = 0x1fff0;

		if( cptFlag&4 )
			cpt_rle_lzh();
		else
		{
			while( dataCLen-- )
				cpt_outch( fgetc( cpt ) );
		}

		fclose( out );
	}
}

void CCptTool::cpt_outch(unsigned char ch)
{
	cpt_LZbuff[ cpt_LZptr++ & (CIRCSIZE-1) ] = ch;

	switch( cpt_outstat )
	{
	case NONESEEN:
		if( ch==ESC1 )
			cpt_outstat = ESC1SEEN;
		else
			dataULen--,fputc( cpt_savechar=ch, out );
		break;

	case ESC1SEEN:
		if( ch==ESC2 )
			cpt_outstat = ESC2SEEN;
		else
		{
			dataULen--,fputc( cpt_savechar=ESC1, out );
			if( ch!=ESC1 )
			{
				cpt_outstat = NONESEEN;
				dataULen--,fputc( cpt_savechar=ch, out );
			}
		}
		break;

	case ESC2SEEN:
		cpt_outstat = NONESEEN;
		if( ch!=0 )
			while( --ch )
				dataULen--,fputc(cpt_savechar,out);
		else
		{
			dataULen--,fputc( ESC1, out );
			dataULen--,fputc( cpt_savechar=ESC2, out );
		}
    }
}

void CCptTool::cpt_rle_lzh()
{
	int block_count;
	unsigned int bptr;
	int Huffchar, LZlength, LZoffs;

	cpt_LZbuff[CIRCSIZE - 3] = 0;
	cpt_LZbuff[CIRCSIZE - 2] = 0;
	cpt_LZbuff[CIRCSIZE - 1] = 0;
	cpt_LZptr = 0;

	while( dataULen!=0 )
	{
		cpt_readHuff(256,cpt_Hufftree);
		cpt_readHuff( 64,cpt_LZlength);
		cpt_readHuff(128,cpt_LZoffs  );
		block_count = 0;
		cpt_newbits = (fgetc(cpt)<<8);
		cpt_newbits = cpt_newbits | fgetc(cpt);
		cpt_newbits = cpt_newbits << 16;
		cpt_bitsavail = 16;
		while( block_count<cpt_blocksize && dataULen!=0 )
		{
			if( cpt_getbit() )
			{
				Huffchar = gethuffbyte(cpt_Hufftree);
				cpt_outch((unsigned char)Huffchar);
				block_count += 2;
			}
			else
			{
				LZlength = gethuffbyte(cpt_LZlength);
				LZoffs = gethuffbyte(cpt_LZoffs);
				LZoffs = (LZoffs << 6) | cpt_get6bits();
				bptr = cpt_LZptr - LZoffs;
				while( LZlength-->0 )
					cpt_outch(cpt_LZbuff[bptr++&(CIRCSIZE-1)]);
				block_count += 3;
			}
		}
	}
}

int CCptTool::gethuffbyte(cpt_node* l_nodelist)
{
	register cpt_node *np;
	np = l_nodelist;
	while(np->flag == 0)
		np = cpt_getbit() ? np->one : np->zero;
	return np->byte;
}

void CCptTool::cpt_readHuff(int size,cpt_node* Hufftree)
{
	cpt_sf_entry tree_entry[256 + SLACK];
	int tree_entries;
	int tree_MaxLength;
	int treeBytes, i, len;
	cpt_sf_entry *ejm1;
	int j;
	cpt_sf_entry *entry;
    cpt_sf_entry tmp;
    int entries;
    unsigned a, b;
	int codelen, lvlstart, next, parents;
	int tree_count[32];

	treeBytes = fgetc(cpt);
	if( size<treeBytes*2 )
		return;
	for( i=0; i!=32; i++ )
		tree_count[i] = 0;
	i = 0;
	tree_MaxLength = 0;
	tree_entries = 0;
	while( treeBytes-->0 )
	{
		int c=fgetc(cpt);
		len = c >> 4;
		ungetc(c,cpt);

		if(len != 0)
		{
			if(len > tree_MaxLength)
				tree_MaxLength = len;
			tree_count[len]++;
			tree_entry[tree_entries].Value = i;
			tree_entry[tree_entries++].BitLength = len;
		}
		i++;
//		len = fgetc(cpt) & NIBBLEMASK;
		len = fgetc(cpt) & 0x0f;
		if(len != 0)
		{
			if(len > tree_MaxLength)
				tree_MaxLength = len;
			tree_count[len]++;
			tree_entry[tree_entries].Value = i;
			tree_entry[tree_entries++].BitLength = len;
		}
		i++;
	}

	j = 0;
    for( i=0; i<=tree_MaxLength; i++ )
		j = (j << 1) + tree_count[i];
	j = (1 <<tree_MaxLength) - j;
	for( i=0; i<j; i++ )
	{
		tree_entry[tree_entries].Value = size;
		tree_entry[tree_entries++].BitLength = tree_MaxLength;
	}

	entry = &(tree_entry[0]);
	entries = tree_entries;
    for( i=0; ++i<entries; )
	{
		tmp = entry[i];
		b = tmp.BitLength;
		j = i;
		while((j > 0) && ((a = (ejm1 = &(entry[j - 1]))->BitLength) >= b))
		{
			if((a == b) && (ejm1->Value <= tmp.Value))
				break;
			*(ejm1 + 1) = *ejm1;
			--j;
		}
		entry[j] = tmp;
	}

    i = tree_entries - 1;
    lvlstart = next = size * 2 + SLACK - 1;
    for(codelen = tree_MaxLength; codelen >= 1; --codelen)
	{
		while((i >= 0) && (tree_entry[i].BitLength == codelen))
		{
			Hufftree[next].byte = tree_entry[i].Value;
			Hufftree[next].flag = 1;
			next--;
			i--;
		}
		parents = next;
		if(codelen > 1)
		{
			for(j = lvlstart; j > parents + 1; j-= 2)
			{
				Hufftree[next].one = &(Hufftree[j]);
				Hufftree[next].zero = &(Hufftree[j - 1]);
				Hufftree[next].flag = 0;
				next--;
			}
		}
		lvlstart = parents;
	}
	Hufftree[0].one = &(Hufftree[next + 2]);
	Hufftree[0].zero = &(Hufftree[next + 1]);
	Hufftree[0].flag = 0;
}

int CCptTool::cpt_get6bits()
{
	int cn,b=(cpt_newbits >> 26) & 0x3f;
	cpt_bitsavail -= 6;
	cpt_newbits <<= 6;
	if(cpt_bitsavail < 16)
	{
		cn = (fgetc(cpt) << 8);
		cn |= fgetc(cpt);
		cpt_newbits |= (cn << (16 - cpt_bitsavail));
		cpt_bitsavail += 16;
	}
	return b;
}

int CCptTool::cpt_getbit()
{
	int b = (cpt_newbits >> 31) & 1;
	cpt_bitsavail--;
	if( cpt_bitsavail<16 )
	{
		cpt_newbits |= (fgetc(cpt) << 8);
		cpt_newbits |= fgetc(cpt);
		cpt_bitsavail += 16;
	}
	cpt_newbits <<= 1;
	return b;
}
