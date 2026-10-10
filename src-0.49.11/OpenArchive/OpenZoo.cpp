#include "stdafx.h"
#include "OpenZoo.h"

bool COpenZoo::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	struct header zoo_header;
	if( !read_zoo_header( &zoo_header) )
		return false;

	return read_Sub(lpSubInfo);

}

bool COpenZoo::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return read_Sub(lpSubInfo);
}
bool COpenZoo::read_Sub(INDIVIDUALINFO *lpSubInfo)
{

	struct direntry direntry;
	read_zoo_dir(&direntry);
	if (direntry.next==0)
		return false;
	fseek( COpenZoo::fp,direntry.next,SEEK_SET );

	lpSubInfo->dwCompressedSize = direntry.size_now;
	lpSubInfo->dwCRC = direntry.file_crc;
	lpSubInfo->dwOriginalSize = direntry.org_size;
	lstrcpy(lpSubInfo->szAttribute , "A---W");
	//lstrcpy(lpSubInfo->szMode,"");

	lstrcpy(lpSubInfo->szFileName,direntry.dirname);
	lpSubInfo->szFileName[direntry.dirlen-1] = 0x5C;
	lstrcpy(&lpSubInfo->szFileName[direntry.dirlen],direntry.fname);

	lpSubInfo->wDate = direntry.date;
	
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}

	lpSubInfo->wTime = direntry.time;
	
	return true;
}

bool COpenZoo::read_zoo_header( struct header* h)
{
	BYTE bytes[SIZ_ZOOH];

	if( MINZOOHSIZ > fread( bytes,1,SIZ_ZOOH,COpenZoo::fp ) )
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

	fseek( COpenZoo::fp,h->zoo_start,SEEK_SET );
	return true;
}

bool COpenZoo::read_zoo_dir( struct direntry *direntry) 
{
	BYTE bytes[MAXDIRSIZE];
	int i;

	if( SIZ_DIR > fread( bytes,1,MAXDIRSIZE,COpenZoo::fp ) )
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

	fseek( COpenZoo::fp,direntry->offset,SEEK_SET );
	return true;
}