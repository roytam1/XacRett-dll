
#include "stdafx.h"
#include "ZooTool.h"
#include "LzhDecoder2.h"

#define H_TYPE	1
#define MAJOR_VER 2
#define MINOR_VER 0
#define MAJOR_EXT_VER 1
#define MINOR_EXT_VER 0
#define MAJOR_LZH_VER	2
#define MINOR_LZH_VER	1

#define STACKSIZE 4000
#define IN_BUF_SIZE  8192
#define OUT_BUF_SIZE 8192
#define INBUFSIZ    (IN_BUF_SIZE - 10)
#define OUTBUFSIZ   (OUT_BUF_SIZE - 10)
#define MAXBITS     13
#define CLEAR       256
#define Z_EOF       257
#define FIRST_FREE  258
#define MAXMAX      8192

#define ZOO_TAG ((unsigned long) 0xFDC4A7DCL) /* A random choice */
#define SIZ_TEXT  20               /* Size of header text */
#define PATHSIZE 256               /* Max length of pathname */
#define FNAMESIZE 13               /* Size of DOS filename */
#define LFNAMESIZE 256             /* Size of long filename */
#define ROOTSIZE 8                 /* Size of fname without extension */
#define EXTLEN 3                   /* Size of extension */
#define SIZ_FLDR  5                /* 4 chars plus null */
#define MAX_PACK 2                 /* max packing method we can handle */

#define SYSID_NIX       0     /* UNIX and similar filesystems */
#define SYSID_MS        1     /* MS-DOS filesystem */
#define SYSID_PORTABLE  2     /* Portable syntax */

#define	FIXED_OFFSET 34		/* zoo_start in old archives */
#define	MINZOOHSIZ 34			/* minimum size of archive header */
#define  SIZ_ZOOH  42			/* length of current archive header */

/* offsets of items within the canonical zoo archive header */
#define  TEXT_I    0       	/* text in header */
#define  ZTAG_I    20      	/* zoo tag */
#define  ZST_I     24      	/* start offset */
#define  ZSTM_I    28      	/* negative of start offset */
#define  MAJV_I    32      	/* major version */
#define  MINV_I    33      	/* minor version */
#define	HTYPE_I	 34			/* archive header type */
#define	ACMTPOS_I 35			/* position of archive comment */
#define	ACMTLEN_I 39			/* length of archive comment */
#define	HVDATA_I	 41			/* version data */

/* offsets of items within the canonical directory entry structure */
#define  SIZ_DIR  51          /* length of type 1 directory entry */
#define  SIZ_DIRL 56          /* length of type 2 directory entry */
#define  DTAG_I   0           /* tag within directory entry */
#define  DTYP_I   4           /* type of directory entry */
#define  PKM_I    5           /* packing method */
#define  NXT_I    6           /* pos'n of next directory entry */
#define  OFS_I    10          /* position (offset) of this file */
#define  DAT_I    14          /* DOS format date */
#define  TIM_I    16          /* DOS format time */
#define  CRC_I    18          /* CRC of this file */
#define  ORGS_I   20          /* original size */
#define  SIZNOW_I 24          /* size now */
#define  DMAJ_I   28          /* major version number */
#define  DMIN_I   29          /* minor version number */
#define  DEL_I    30          /* deleted or not */
#define  STRUC_I  31          /* file structure */
#define  CMT_I    32          /* comment [offset] */
#define  CMTSIZ_I 36          /* comment size */
#define  FNAME_I  38          /* filename */
#define  VARDIRLEN_I  51      /* length of var. direntry */
#define  TZ_I     53          /* timezone */
#define  DCRC_I   54          /* CRC of directory entry */

#define  FNM_SIZ  13          /* size of stored filename */

/* Offsets within variable part of directory entry */
#define  NAMLEN_I   (SIZ_DIRL + 0)
#define  DIRLEN_I   (SIZ_DIRL + 1)
#define  LFNAME_I   (SIZ_DIRL + 2)
#define  DIRNAME_I  LFNAME_I  /* plus length of filename */

#define  MAXDIRSIZE  (SIZ_DIRL+1+1+256+256+2+3+1+2+5)

/* Value used to stuff into timezone field if it is not known */
#define  NO_TZ    127

/* Value for no file attributes */
#define	NO_FATTR		0L

/* version flag bits */
#define	VFL_ON	0x80		/* enable version numbering */
#define	VFL_GEN	0x0f		/* generation count */
#define	VFL_LAST 0x40		/* last generation of this file */

/* default generation value for archive */
#define	GEN_DEFAULT			3
/* max generation count, file or archive */
#define	MAXGEN				0x0f
/* version mask to prune down to correct size on large-word machines */
#define VER_MASK				0xffff

struct header {
   char text[SIZ_TEXT];       /* archive header text */
   unsigned long zoo_tag;     /* identifies archives */
   long zoo_start;            /* where the archive's data starts */
   long zoo_minus;      	  /* for consistency checking of zoo_start */
   BYTE major_ver;
   BYTE minor_ver;            /* minimum version to extract all files */
	BYTE type;				  /* type of archive header */
	long acmt_pos;			  /* position of archive comment */
	unsigned int acmt_len;	  /* length of archive comment */
	unsigned int vdata;		  /* byte in archive;  data about versions */
};

struct direntry {
   unsigned long zoo_tag; /* tag -- redundancy check */
   BYTE type;             /* type of directory entry.  always 1 for now */
   BYTE packing_method;   /* 0 = no packing, 1 = normal LZW */
   long next;             /* pos'n of next directory entry */
   long offset;           /* position of this file */
   unsigned int date;     /* DOS format date */
   unsigned int time;     /* DOS format time */
   unsigned int file_crc; /* CRC of this file */
   long org_size;
   long size_now;
   BYTE major_ver;
   BYTE minor_ver;        /* minimum version needed to extract */
   BYTE deleted;          /* will be 1 if deleted, 0 if not */
   BYTE struc;            /* file structure if any */
   long comment;          /* points to comment;  zero if none */
   unsigned int cmt_size; /* length of comment, 0 if none */
   char fname[FNAMESIZE]; /* filename */

   int var_dir_len;       /* length of variable part of dir entry */
   BYTE tz;               /* timezone where file was archived */
   unsigned int dir_crc;  /* CRC of directory entry */

   /* fields for variable part of directory entry follow */
   BYTE namlen;             /* length of long filename */
   BYTE dirlen;             /* length of directory name */
   char lfname[LFNAMESIZE]; /* long filename */
   char dirname[PATHSIZE];  /* directory name */
   unsigned int system_id;  /* Filesystem ID */
   unsigned long fattr;	    /* File attributes -- 24 bits */
   unsigned int vflag;		/* version flag bits -- one byte in archive */
   unsigned int version_no; /* file version number if any */
};

#define to_long(p) (long)( (DWORD)((p)[0]) | (DWORD)((p)[1]<<8) |\
						 (DWORD)((p)[2]<<16) | (DWORD)((p)[3]<<24) )
#define to_short(p) (short)( (WORD)((p)[0]) | (WORD)((p)[1]<<8) )

bool CZooTool::read_zoo_header( struct header* h,FILE* fp )
{
	BYTE bytes[SIZ_ZOOH];

	if( MINZOOHSIZ > fread( bytes,1,SIZ_ZOOH,fp ) )
		return false;

	for( int i=0; i<SIZ_TEXT; i++ )
		h->text[i] = bytes[TEXT_I+i];
	h->zoo_tag = to_long(&bytes[ZTAG_I]);
	h->zoo_start = to_long(&bytes[ZST_I]);
	h->zoo_minus = to_long(&bytes[ZSTM_I]);
	h->major_ver = bytes[MAJV_I];
	h->minor_ver = bytes[MINV_I];

	h->type = 0;
	h->acmt_pos = 0L;
	h->acmt_len = 0;
	h->vdata = 0;
	if( h->zoo_start!=FIXED_OFFSET )/* if newer header */
	{
		h->type = bytes[HTYPE_I];
		h->acmt_pos = to_long(&bytes[ACMTPOS_I]);
		h->acmt_len = to_short(&bytes[ACMTLEN_I]);
		h->vdata = bytes[HVDATA_I];
	}

	if( (h->zoo_start+h->zoo_minus)!=0 )
		return false;

	fseek( fp,h->zoo_start,SEEK_SET );
	return true;
}

bool CZooTool::read_zoo_dir( struct direntry *direntry,FILE* fp ) 
{
	BYTE bytes[MAXDIRSIZE];
	int i;

	if( SIZ_DIR > fread( bytes,1,MAXDIRSIZE,fp ) )
		return false;

	int sysid_offs;			/* temp variable */
//	unsigned int savecrc;
	direntry->zoo_tag = to_long(&bytes[DTAG_I]);
	direntry->type = bytes[DTYP_I];
	direntry->packing_method = bytes[PKM_I];
	direntry->next = to_long(&bytes[NXT_I]);
	direntry->offset = to_long(&bytes[OFS_I]);
	direntry->date = to_short(&bytes[DAT_I]);
	direntry->time = to_short(&bytes[TIM_I]);
	direntry->file_crc = to_short(&bytes[CRC_I]);
	direntry->org_size = to_long(&bytes[ORGS_I]);
	direntry->size_now = to_long(&bytes[SIZNOW_I]);
	direntry->major_ver = bytes[DMAJ_I];
	direntry->minor_ver = bytes[DMIN_I];
	direntry->deleted = bytes[DEL_I];
	direntry->struc = bytes[STRUC_I];
	direntry->comment = to_long(&bytes[CMT_I]);
	direntry->cmt_size = to_short(&bytes[CMTSIZ_I]);

	direntry->vflag = 0;
	direntry->version_no = 0;
	for( i=0; i<FNM_SIZ; i++ )
		direntry->fname[i] = bytes[FNAME_I+i];

	/* start by assuming variable part is zero bytes */
	direntry->var_dir_len = direntry->dir_crc    = 0;
	direntry->namlen      = direntry->dirlen     = 0;
	direntry->lfname[0]   = direntry->dirname[0] = '\0';
	direntry->tz = NO_TZ;              /* assume unknown */
	direntry->system_id = SYSID_NIX;
	direntry->fattr = NO_FATTR;		/* assume none */

	if( direntry->type==2 )
	{
		direntry->var_dir_len = to_short(&bytes[VARDIRLEN_I]);
		if( direntry->var_dir_len > MAXDIRSIZE )
			direntry->var_dir_len = MAXDIRSIZE;
		direntry->tz = bytes[TZ_I];   
		if( direntry->var_dir_len > 0 )
			direntry->namlen = bytes[NAMLEN_I];
		if( direntry->var_dir_len > 1 )
			direntry->dirlen = bytes[DIRLEN_I];
		for( i=0; i<direntry->namlen; i++ )
			direntry->lfname[i] = bytes[LFNAME_I + i];
		for( i=0; i<direntry->dirlen; i++ )
			direntry->dirname[i] = bytes[DIRNAME_I + direntry->namlen + i];
		sysid_offs = DIRNAME_I + direntry->namlen + i;	/* offset of system id */
		if( direntry->var_dir_len > direntry->namlen + direntry->dirlen + 2)
			direntry->system_id = to_short(&bytes[sysid_offs]);
		if (direntry->var_dir_len > direntry->namlen + direntry->dirlen + 4) {
			direntry->fattr = ((unsigned long) bytes[sysid_offs + 2]) |
								((unsigned long) bytes[sysid_offs + 3] << 8) |
								((unsigned long) bytes[sysid_offs + 4] << 16);
		}
		if (direntry->var_dir_len > direntry->namlen + direntry->dirlen + 7)
		{
			direntry->vflag = bytes[sysid_offs + 5];
			direntry->version_no = to_short(&bytes[sysid_offs + 6]);
		}
	}

	fseek( fp,direntry->offset,SEEK_SET );
	return true;
}

/********************************************/

#define push(x) stack[stack_pointer++]=(x)
#define pop()   stack[--stack_pointer]

unsigned CZooTool::rd_dcode()
{
	const static unsigned masks[] =
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff };

	register char *ptra, *ptrb;
	unsigned word;
	unsigned byte_offset;
	char nextch;
	unsigned ofs_inbyte;

	ofs_inbyte = bit_offset % 8;
	byte_offset = bit_offset / 8;
	bit_offset = bit_offset + nbits;

	if( byte_offset>=INBUFSIZ-5 )
	{
		int space_left;

		bit_offset = ofs_inbyte + nbits;
		space_left = INBUFSIZ - byte_offset;
		ptrb = byte_offset + in_buf_adr;
		ptra = in_buf_adr;

		while( space_left>0 )
		{
			*ptra++ = *ptrb++;
			space_left--;
		}
		if( -1==fread(ptra,1,byte_offset,in_f ) )
			return Z_EOF;// by K.INABA -- GoToEnd
		byte_offset = 0;
	}
	ptra = byte_offset + in_buf_adr;

	word = (unsigned char) *ptra; ptra++;
	word = word | ( ((unsigned char) *ptra) << 8 ); ptra++;

	nextch = *ptra;
	if( ofs_inbyte!=0 )
		word = (word>>ofs_inbyte) | (((unsigned)nextch)<<(16-ofs_inbyte));
	return (word & masks[nbits]); 
}

void CZooTool::wr_dchar(int ch)
{
	if( output_offset>=OUTBUFSIZ )
	{
		fwrite(out_buf_adr,1,output_offset,out_f );
		output_offset = 0;
	}
	out_buf_adr[output_offset++] = ch;
}

void CZooTool::ad_dcode()
{
	table[free_code].z_ch = k;
	table[free_code].next = old_code;
	free_code++;
	if( free_code>=max_code )
	{
		if( nbits<MAXBITS )
		{
			nbits++;
			max_code = max_code << 1;
		}
	}
}

void CZooTool::lzd( FILE* arc,FILE* out )
{
	in_f = arc;
	out_f = out;
	nbits = 9;
	max_code = 512;
	free_code = FIRST_FREE;
	stack_pointer = 0;
	bit_offset = 0;
	output_offset = 0;

	nbits = 9;
	max_code = 512;
	free_code = FIRST_FREE;

	fread( in_buf_adr,1,INBUFSIZ,in_f );
loop:
	cur_code = rd_dcode();
goteof: /* special case for CLEAR then Z_EOF, for 0-length files */
	if( cur_code==Z_EOF )
	{
		if( output_offset!=0 )
			fwrite( out_buf_adr,1,output_offset,out_f );
		return;
	}

	if( cur_code==CLEAR )
	{
		nbits = 9;
		max_code = 512;
		free_code = FIRST_FREE;

		fin_char = k = old_code = cur_code = rd_dcode();
		if( cur_code==Z_EOF )
			goto goteof;
		wr_dchar(k);
		goto loop;
	}

	in_code = cur_code;
	if( cur_code>=free_code )
	{
		cur_code = old_code;
		push(fin_char);
	}

	while( cur_code>255 )
	{
		push( table[cur_code].z_ch );
		cur_code = table[cur_code].next;
	}

	k = fin_char = cur_code;
	push(k);
	while( stack_pointer!=0 )
		wr_dchar(pop());

	ad_dcode();
	old_code = in_code;

	goto loop;
}

/********************************************/

bool CZooTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	// 小さすぎ
	if( MINZOOHSIZ+4 > siz )
		return false;
	// 書庫ヘッダでチェック
	unsigned long zoo_start = to_long( hdr+ZST_I );
	unsigned long zoo_minus = to_long( hdr+ZSTM_I );
	if( 0 != (zoo_start+zoo_minus)
	 ||	zoo_start   < MINZOOHSIZ
	 ||	zoo_start+4 > siz )
		return false;
	// 一個目のエントリでチェック
	return ( ZOO_TAG == to_long( hdr+zoo_start ) );
}

bool CZooTool::Extract( LPCSTR fname,LPCSTR ddir )
{
	FILE* zoo = fopen( fname,"rb" );
	if( !zoo ) return false;

	struct header zoo_header;
	if( !read_zoo_header( &zoo_header,zoo ) )
	{	fclose( zoo ); return false; }

	// 作業用メモリ確保
	table = new zoo_tabentry[MAXMAX+10];
	stack = new unsigned[STACKSIZE+20];
	out_buf_adr = new char[OUT_BUF_SIZE+100];
	in_buf_adr = new char[OUT_BUF_SIZE+300];
	CLzhDecoder2 lzh;
	/////////////

	struct direntry direntry;
	while( read_zoo_dir( &direntry,zoo ) )
	{
		if( direntry.zoo_tag != ZOO_TAG )goto loop_again; // BAD HEADER
		if( direntry.next==0 )break;					  // END OF CHAIN

		{
			// 出力先ファイル名取得…
			char outname[256];

			lstrcpy(outname,direntry.dirname);
			outname[direntry.dirlen-1] = 0x5C;
			lstrcpy(&outname[direntry.dirlen],direntry.fname);
			
			/* 出力先ファイル名取得…
			char* outname =	kiutil::pathMake(
				(direntry.namlen!=0)?direntry.lfname:direntry.fname);*/

			if (m_Endflg) return true;//追加 By Bonty
			//ワイルドカード処理
			if (!CheckStr(outname))//追加 By Bonty
			{
				fseek( zoo,direntry.next,SEEK_SET );
				continue;
			}
			lstrcpy(outname,kiutil::pathMake(outname));

			FILE* out = fopen( outname,"wb" );
			if( out==NULL )goto loop_again;

			switch( direntry.packing_method )
			{
			case 0:
				lzh.Decode( LH0, zoo, direntry.size_now,
								 out, direntry.org_size );
				break;
			case 1:
				lzd( zoo,out );
				break;
			case 2:
				lzh.Decode( LH5, zoo, direntry.size_now,
								 out, direntry.org_size );
				break;
			}

			fclose( out );
			kiutil::timeSet( outname, direntry.date, direntry.time );
		}
loop_again:
		fseek( zoo,direntry.next,SEEK_SET );
	}

	// 作業用メモリ解放
	delete [] table;
	delete [] stack;
	delete [] out_buf_adr;
	delete [] in_buf_adr;
	///////////////////

	fclose( zoo );
	return true;
}
