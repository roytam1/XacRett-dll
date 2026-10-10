#ifndef AFX_OPENZOO_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENZOO_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

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

#define to_long(p) (long)( (DWORD)((p)[0]) | (DWORD)((p)[1]<<8) |\
						 (DWORD)((p)[2]<<16) | (DWORD)((p)[3]<<24) )
#define to_short(p) (short)( (WORD)((p)[0]) | (WORD)((p)[1]<<8) )


class COpenZoo : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenZoo() : COpenArchive( )
	{COpenZoo::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
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
	bool read_zoo_header( struct header* h);
	bool read_zoo_dir( struct direntry *direntry);
	bool read_Sub(INDIVIDUALINFO *lpSubInfo);
};

#endif