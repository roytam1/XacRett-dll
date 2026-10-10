#include "stdafx.h"
#include "OpenAce.h"

bool COpenAce::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	COpenAce::COpenAce();
	if(!open_archive(COpenAce::szArcFileName))
	{
		close(archan);
		return false;
	}
	comment_skip();

	if( adat.vol_num )// 一番目のボリュームでない。
	{
		close(archan);
		return false;
	}

	return FindSub(lpSubInfo);
}

bool COpenAce::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	bool temp=FindSub(lpSubInfo);
	if(!temp)
		close(archan);

	return temp;
}

bool COpenAce::FindSub(INDIVIDUALINFO *lpSubInfo)
{

	if (!read_header() )
		return false;

	if( head.HEAD_TYPE!=FILE_BLK )
		return FindSub(lpSubInfo);

	comment_skip();

		char name[PATH_MAX];
		strncpy( name,(char*)fhead.FNAME,fhead.FNAME_SIZE );
		name[ fhead.FNAME_SIZE ] = '\0';

		if( !(fhead.ATTR & _A_SUBDIR) )
		{
			//lstrcpy(lpSubInfo->szMode,"");
			lpSubInfo->dwCompressedSize = fhead.PSIZE;
			lpSubInfo->dwCRC=fhead.CRC32;
			lpSubInfo->dwOriginalSize=fhead.SIZE;
			lstrcpy(lpSubInfo->szFileName,name);
			lpSubInfo->wDate=(WORD)(fhead.FTIME>>16);
			lpSubInfo->wTime = (WORD)(fhead.FTIME);

			double tmp1 = (double)lpSubInfo->dwOriginalSize;
			double tmp2 = (double)lpSubInfo->dwCompressedSize;
			if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
				lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
			}

			lstrcpy(lpSubInfo->szAttribute ,"----W");
			if (fhead.ATTR & FILE_ATTRIBUTE_READONLY)
				lpSubInfo->szAttribute[4] = 'R';
			if (fhead.ATTR & FILE_ATTRIBUTE_HIDDEN)
				lpSubInfo->szAttribute[2] = 'H';
			if (fhead.ATTR & FILE_ATTRIBUTE_SYSTEM)
				lpSubInfo->szAttribute[3] = 'S';
			if (fhead.ATTR & FILE_ATTRIBUTE_DIRECTORY)
				lpSubInfo->szAttribute[1] = 'D';
			if (fhead.ATTR & FILE_ATTRIBUTE_ARCHIVE)
				lpSubInfo->szAttribute[0] = 'A';
			return true;
		}else{
			return FindSub(lpSubInfo);
		}
}

//-------------------------------------------------------

static const char *acesign = "**ACE**";
//ここにバグあり
bool COpenAce::find_arc_head()
{
	int i,flags,buf_pos = 0;
	long arc_head_pos,old_fpos,fpos = 0;

	struct stat st;
	fstat(archan, &st);

	memset( buf,0,size_buf );

	while( lseek(archan,0,SEEK_CUR)<st.st_size && fpos<max_sfx_size )
	{
		old_fpos = fpos;
		fpos += read( archan,&buf[buf_pos],size_buf-buf_pos );

		for( i=0; i<size_buf; i++ )
		{
			if( !memcmp(acesign,&buf[i],7) )
			{
				arc_head_pos = old_fpos + i - buf_pos -  7;
				lseek( archan,arc_head_pos,SEEK_SET );
				if( read_header() )
				{
					flags = mhead.HEAD_FLAGS;
					adat.sol     = (flags & ACE_SOLID) > 0;
					adat.vol     = (flags & ACE_MULT_VOL) > 0;
					adat.vol_num = mhead.VOL_NUM;
					adat.time_cr = mhead.TIME_CR;
					return true;
				}
			}
		}
		lseek(archan, fpos, SEEK_SET);
		memcpy(buf, &buf[size_buf - 512], 512);
		buf_pos = 512;
   }
   return false;
}

bool COpenAce::open_archive(const char* aname)
{
	skipsize=0;

	archan = open(aname, O_RDONLY | O_BINARY);
	if( archan==-1 ){
		return false;}
	if( !find_arc_head() )
	{	close(archan); return false;	}

	if( arcname!=aname )
		strcpy(arcname,aname);//マルチボリューム用にファイル名をとっておく。
	comment_skip();
	return true;
}
/*
void COpenAce::analyze_file()
{
	while( dcpr_adds_blk(buf_wr,size_wrb) );
}

void COpenAce::extract_file()
{
	int rd;
	while( rd=dcpr_adds_blk(buf_wr, size_wrb) )
		write( wrhan,buf_wr,rd );
}
*/
bool COpenAce::read_header()
{
	WORD rd,head_size;
	long crc;
	BYTE *tp=readbuf;

	lseek(archan,skipsize,SEEK_CUR);

	if( read(archan,&head,4)<4 ){
		return false;}

	head_size = head.HEAD_SIZE;
	rd = (head_size > size_headrdb) ? size_headrdb : head_size;
	if( read(archan, readbuf, rd)<rd ){
		return false;}
	head_size -= rd;
	crc = getcrc(CRC_MASK, readbuf, rd);

	while( head_size )
	{                            
		rd = (head_size > size_buf) ? size_buf : head_size;
		if( read(archan,buf,rd)<rd ){
			return false;}
		head_size -= rd;
		crc = getcrc(crc,(BYTE*)buf, rd);
	}

	head.HEAD_TYPE = *tp++;
	head.HEAD_FLAGS=BUFP2WORD(tp);

	if( head.HEAD_FLAGS & ACE_ADDSIZE )
		skipsize = head.ADDSIZE = BUF2LONG(tp);
	else
		skipsize = 0;

	if( head.HEAD_CRC!=(crc&0xffff) ){
		return false;}

	switch( head.HEAD_TYPE )
	{
		case MAIN_BLK:
			memcpy(mhead.ACESIGN, tp, 7); tp+=7;
			mhead.VER_MOD=*tp++;
			mhead.VER_CR =*tp++;
			mhead.HOST_CR=*tp++;
			mhead.VOL_NUM=*tp++;
			mhead.TIME_CR=BUFP2LONG(tp);
			mhead.RES1   =BUFP2WORD(tp);
			mhead.RES2   =BUFP2WORD(tp);
			mhead.RES    =BUFP2LONG(tp);
			mhead.AV_SIZE=*tp++;
			memcpy(mhead.AV, tp, rd-(USHORT)(tp-readbuf));
			break;
		case FILE_BLK:
			fhead.PSIZE     =BUFP2LONG(tp);
			fhead.SIZE      =BUFP2LONG(tp);
			fhead.FTIME     =BUFP2LONG(tp);
			fhead.ATTR      =BUFP2LONG(tp);
			fhead.CRC32     =BUFP2LONG(tp);
			fhead.TECH.TYPE =*tp++;
			fhead.TECH.QUAL =*tp++;
			fhead.TECH.PARM =BUFP2WORD(tp);
			fhead.RESERVED  =BUFP2WORD(tp);
			fhead.FNAME_SIZE=BUFP2WORD(tp);
			memcpy(fhead.FNAME, tp, rd-(USHORT)(tp-readbuf));
		break;
	}

	return true;
}

void COpenAce::comment_skip()
{
	int i=0;

	if( head.HEAD_FLAGS & ACE_COMM )
	{
		if( head.HEAD_TYPE==MAIN_BLK )
		{
			comm = (char*)MCOMM;
		}
		else
		{
			comm = (char*)FCOMM;
		}
		i = sizeof(head) - (INT)(comm - (CHAR*) &head);
		lseek(archan,i,SEEK_CUR);
	}
	
}
