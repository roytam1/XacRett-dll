
#include "stdafx.h"
#include "AceTool.h"

bool CAceTool::Check( const char* fname, unsigned long fsize )
{
	if( !open_archive(fname) )
		return false;
	close(archan);
	return (adat.vol_num==0);
}

bool CAceTool::Extract( const char* fname,const char* ddir )
{
	if( !open_archive(fname) )
		return false;

	if( adat.vol_num )
	{	close(archan);return false;	}// 一番目のボリュームでない。
	else
	{
		while( read_header() )
		{
			if( head.HEAD_TYPE!=FILE_BLK )
				continue;

			comment_skip();

			if( dcpr_init_file() )
			{
				char name[PATH_MAX];
				strncpy( name,(char*)fhead.FNAME,fhead.FNAME_SIZE );
				name[ fhead.FNAME_SIZE ] = '\0';

				if (m_Endflg) return true;//追加 By Bonty
				if (!CheckStr(name))//追加 By Bonty
				{
					int rd;
					while( rd=dcpr_adds_blk(buf_wr, size_wrb) );
					continue;
				}

				if( fhead.ATTR & _A_SUBDIR )
				{
					strcat( name,"\\" );
					kiutil::pathMake( name );
					if( adat.sol )
						analyze_file();
				}
				else
				{
					char* outname = kiutil::pathMake( name );
					FILE* wf=fopen(outname,"wb");
					/*wrhan = open( outname,
							O_WRONLY | O_TRUNC | O_CREAT | O_BINARY );*/
					if(wf)
					{
						wrhan=fileno(wf);
						if( wrhan>=0 )
						{
							extract_file();
							close(wrhan);

							kiutil::timeSet( outname,(WORD)(fhead.FTIME>>16),(WORD)fhead.FTIME );
							SetFileAttributes(outname,fhead.ATTR);
						}
					}
				}
			}
		}
	}

	close(archan);
	return true;
}

/*************************************************/

#define max_sfx_size 65536
static const char *acesign = "**ACE**";

bool CAceTool::find_arc_head()
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

bool CAceTool::open_archive(const char* aname)
{
	archan = open(aname, O_RDONLY | O_BINARY);
	if( archan==-1 )
		return false;
	if( !find_arc_head() )
	{	close(archan);	return false;	}

	if( arcname!=aname )
		strcpy(arcname,aname);//マルチボリューム用にファイル名をとっておく。
	comment_skip();
	return true;
}

void CAceTool::analyze_file()
{
	while( dcpr_adds_blk(buf_wr,size_wrb) );
}

void CAceTool::extract_file()
{
	int rd;
	while( rd=dcpr_adds_blk(buf_wr, size_wrb) )
		write( wrhan,buf_wr,rd );
}

bool CAceTool::read_header()
{
	WORD rd,head_size;
	long crc;
	BYTE *tp=readbuf;

	lseek(archan,skipsize,SEEK_CUR);

	if( read(archan,&head,4)<4 )
		return false;

	head_size = head.HEAD_SIZE;
	rd = (head_size > size_headrdb) ? size_headrdb : head_size;
	if( read(archan, readbuf, rd)<rd )
		return false;
	head_size -= rd;
	crc = getcrc(CRC_MASK, readbuf, rd);

	while( head_size )
	{                            
		rd = (head_size > size_buf) ? size_buf : head_size;
		if( read(archan,buf,rd)<rd )
			return false;
		head_size -= rd;
		crc = getcrc(crc,(BYTE*)buf, rd);
	}

	head.HEAD_TYPE = *tp++;
	head.HEAD_FLAGS=BUFP2WORD(tp);

	if( head.HEAD_FLAGS & ACE_ADDSIZE )
		skipsize = head.ADDSIZE = BUF2LONG(tp);
	else
		skipsize = 0;

	if( head.HEAD_CRC!=(crc&0xffff) )
		return false;

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

/******  DECOMPRESSORS  *********************/

void CAceTool::dcpr_init()
{
	dcpr_frst_file = 1;
	dcpr_dic = 20;
	while( (dcpr_text=new char[dcpr_dicsiz=(long)(1<<dcpr_dic)])==0 )
		dcpr_dic--;
	dcpr_dican = dcpr_dicsiz - 1;
}

bool CAceTool::dcpr_init_file()
{
	UINT i;

	if( head.HEAD_FLAGS & ACE_PASSW )
		return false;// Decryption not supported.

	rd_crc = CRC_MASK;
	dcpr_size = fhead.SIZE;
	if( fhead.TECH.TYPE == TYPE_LZ1 )
	{
		if( (fhead.TECH.PARM&15)+10 > dcpr_dic )
			return false;// Not enough memory

		i = size_rdb * sizeof(LONG);
		read_adds_blk((CHAR *) buf_rd, i);
		code_rd = buf_rd[0];
		bits_rd = rpos = 0;

		blocksize = 0;
	}
	if( !adat.sol || dcpr_frst_file )
		dcpr_dpos = 0;

	dcpr_oldnum = 0;
	memset(&dcpr_olddist, 0, sizeof(dcpr_olddist));

	dcpr_frst_file = 0;
	return true;
}

int CAceTool::dcpr_adds_blk(char* buf, UINT len)
{
	int r;

	switch( fhead.TECH.TYPE )
	{
	case TYPE_STORE:
		r = unstore(buf,len);
		break;
	case TYPE_LZ1:
		r = decompress_blk(buf,len);
		break;
	default:
		r = 0;
	}
	rd_crc = getcrc(rd_crc,(BYTE*)buf, r);
	return r;
}

static void get_next_volname(char* aname)
{
	char *cp;
	int num;

	if( (cp=(char*)strrchr(aname,'.'))==NULL || !*(cp+1) )
		num = -1;
	else
	{
		cp++;
		num = (cp[1]-'0')*10+(cp[2]-'0');
		if( num<0 || 99<num )
			num = -1;
		if( '0'<=*cp && *cp<='9' )
			num += (*cp-'0')*100;
	}
	num++;

	cp[0]=(num<100)?'C':num/100+'0';
	cp[1]=(num/10)%10+'0';
	cp[2]=   num  %10+'0';
	cp[3]=0;
}

bool CAceTool::proc_vol()
{
	get_next_volname(arcname);

	if( !open_archive(arcname) )
		return false;
	return true;
}

bool CAceTool::proc_next_vol()
{
	close(archan);
	if( !proc_vol() )	return false;
	if( !read_header() )return false;
	return true;
}

int CAceTool::read_adds_blk(char* buffer,int len)
{
	int rd = 0,l = len;
	long i;

	while( len && skipsize )
	{
		i = (skipsize>len)?len:skipsize;
		skipsize -= i;

		rd += read(archan, buffer, i);

		buffer += i;
		len -= i;

		if(!skipsize)
			if(head.HEAD_FLAGS & ACE_SP_AFTER && !proc_next_vol())
				break;
	}
	return (rd > l ? l : rd);
}


//------------------------------ 以下、いい加減にコピー(^^; ---------------------------------//

#define xchg_def(v1,v2) {int dummy;\
                         dummy=v1; \
                         v1=v2;    \
                         v2=dummy;}

void CAceTool::sortrange(int left, int right)
{
   int  zl = left,
        zr = right,
        hyphen;

   hyphen = sort_freq[right];

   //divides by hyphen the given range into 2 parts
   do
   {
      while (sort_freq[zl] > hyphen)
         zl++;
      while (sort_freq[zr] < hyphen)
         zr--;
      //found a too small (left side) and
      //a too big (right side) element-->exchange them
      if (zl <= zr)
      {
         xchg_def(sort_freq[zl], sort_freq[zr]);
         xchg_def(sort_org[zl], sort_org[zr]);
         zl++;
         zr--;
      }
   }
   while (zl < zr);

   //sort partial ranges - when very small, sort directly
   if (left < zr)
      if (left < zr - 1)
         sortrange(left, zr);
      else if (sort_freq[left] < sort_freq[zr])
      {
         xchg_def(sort_freq[left], sort_freq[zr]);
         xchg_def(sort_org[left], sort_org[zr]);
      }

   if (right > zl)
      if (zl < right - 1)
         sortrange(zl, right);
      else if (sort_freq[zl] < sort_freq[right])
      {
         xchg_def(sort_freq[zl], sort_freq[right]);
         xchg_def(sort_org[zl], sort_org[right]);
      }
}

void CAceTool::quicksort(int n)
{
   for (int i = n + 1; i--;)
      sort_org[i] = i;
   sortrange(0, n);
}

void CAceTool::readdat()
{
   UINT i;

   i = (size_rdb - 2) << 2;
   rpos -= size_rdb - 2;
   buf_rd[0] = buf_rd[size_rdb - 2];
   buf_rd[1] = buf_rd[size_rdb - 1];
   read_adds_blk((CHAR *) & buf_rd[2], i);
}

#define addbits(bits)                                   \
{                                                       \
  rpos+=(bits_rd+=bits)>>5;                             \
  bits_rd&=31;                                          \
  if (rpos==(size_rdb-2)) readdat();                    \
  code_rd=(buf_rd[rpos] << bits_rd)                     \
    +((buf_rd[rpos+1] >> (32-bits_rd))&(!bits_rd-1));   \
}

int CAceTool::unstore(char* buf,UINT len)
{
	UINT rd = 0,i,pos = 0;

	while( (i=read_adds_blk((char*)buf_rd,(int)((i = ((len > dcpr_size) ? dcpr_size : len)) > size_rdb ? size_rdb : i))) != 0 )
	{
		rd += i;
		len -= i;
		dcpr_size -= i;
		memcpy(&buf[pos], buf_rd, i);
		pos += i;
	}
	for( i=0; i<rd; i++ )
	{
		dcpr_text[dcpr_dpos] = buf[i];
		dcpr_dpos++;
		dcpr_dpos &= dcpr_dican;
	}
	return (int)rd;
}

int CAceTool::decompress_blk(char* buf, UINT len)
{
	long old_pos = dcpr_dpos;
	int i;

	dcpr_do = 0;
	if ((dcpr_do_max = len - maxlength) > (long)dcpr_size)
		dcpr_do_max = (short)dcpr_size;
	if ((LONG) dcpr_size > 0 && dcpr_do_max)
	{
		decompress();
		if (dcpr_do <= (int)len)
		{
			if (old_pos + dcpr_do > (long)dcpr_dicsiz)
			{
			i = dcpr_dicsiz - old_pos;
			memcpy(buf, &dcpr_text[old_pos], i);
			memcpy(&buf[i], dcpr_text, dcpr_do - i);
			}
			else
				memcpy(buf, &dcpr_text[old_pos], dcpr_do);
		}
	}
	dcpr_size -= dcpr_do;
	return (dcpr_do);
}

void CAceTool::decompress()
{
	int c,lg,i,k;
	DWORD dist;

	while (dcpr_do < dcpr_do_max)
	{
		if (!blocksize)
			if (!calc_dectabs())
				return;

		addbits(dcpr_wd_mn[(c = dcpr_code_mn[code_rd >> (32 - maxwd_mn)])]);
		blocksize--;
		if (c > 255)
		{
			if (c > 259)
			{
				if ((c -= 260) > 1)
				{
					dist = (code_rd >> (33 - c)) + (1L << (c - 1));
					addbits(c - 1);
				}
				else
					dist = c;
				dcpr_olddist[(dcpr_oldnum = (dcpr_oldnum + 1) & 3)] = dist;
				i = 2;
				if (dist > maxdis2)
				{
					i++;
					if (dist > maxdis3)
						i++;
				}
			}
			else
			{
				dist = dcpr_olddist[(dcpr_oldnum - (c &= 255)) & 3];
				for (k = c + 1; k--;)
					dcpr_olddist[(dcpr_oldnum - k) & 3] = dcpr_olddist[(dcpr_oldnum - k + 1) & 3];
				dcpr_olddist[dcpr_oldnum] = dist;
				i = 2;
				if (c > 1)
					i++;
			}
			addbits(dcpr_wd_lg[(lg = dcpr_code_lg[code_rd >> (32 - maxwd_lg)])]);
			dist++;
			lg += i;
			copystr(dist, lg);
		}
		else
			wrchar(c);
	}
}




static void memset16(WORD* dest,WORD val,int len)
{
	while(len--)
		*(dest++) = val;
}

int CAceTool::makecode(UINT maxwd, UINT size1_t, BYTE* wd, WORD* code)
{
   UINT maxc,
        size2_t,
        l,
        c,
        i,
        max_make_code;

   memcpy(&sort_freq, wd, (size1_t + 1) * sizeof(CHAR));
   if (size1_t)
      quicksort(size1_t);
   else
      sort_org[0] = 0;
   sort_freq[size1_t + 1] = size2_t = c = 0;
   while (sort_freq[size2_t])
      size2_t++;
   if (size2_t < 2)
   {
      i = sort_org[0];
      wd[i] = 1;
      size2_t += (size2_t == 0);
   }
   size2_t--;

   max_make_code = 1 << maxwd;
   for (i = size2_t + 1; i-- && c < max_make_code;)
   {
      maxc = 1 << (maxwd - sort_freq[i]);
      l = sort_org[i];
      if (c + maxc > max_make_code)
      {
         dcpr_do = dcpr_do_max;
         return (0);
      }
      memset16(&code[c], l, maxc);
      c += maxc;
   }
   return (1);
}

int CAceTool::read_wd(UINT maxwd, WORD* code, BYTE* wd, int max_el)
{
   UINT c,
        i,
        j,
        num_el,
        l,
        uplim,
        lolim;

   memset(wd, 0, max_el * sizeof(CHAR));
   memset(code, 0, (1 << maxwd) * sizeof(SHORT));

   num_el = code_rd >> (32 - 9);
   addbits(9);
   if (num_el > (UINT)max_el)
      num_el = max_el;

   lolim = code_rd >> (32 - 4);
   addbits(4);
   uplim = code_rd >> (32 - 4);
   addbits(4);

   for (i = -1; ++i <= uplim;)
   {
      wd_svwd[i] = (BYTE)(code_rd >> (32 - 3));
      addbits(3);
   }
   if (!makecode(maxwd_svwd, uplim, wd_svwd, code))
      return (0);
   j = 0;
   while (j <= num_el)
   {
      c = code[code_rd >> (32 - maxwd_svwd)];
      addbits(wd_svwd[c]);
      if (c < uplim)
         wd[j++] = c;
      else
      {
         l = (code_rd >> 28) + 4;
         addbits(4);
         while (l-- && j <= num_el)
            wd[j++] = 0;
      }
   }
   if (uplim)
      for (i = 0; ++i <= num_el;)
         wd[i] = (wd[i] + wd[i - 1]) % uplim;
   for (i = -1; ++i <= num_el;)
      if (wd[i])
         wd[i] += lolim;

   return (makecode(maxwd, num_el, wd, code));
}

int CAceTool::calc_dectabs()
{
   if (!read_wd(maxwd_mn, dcpr_code_mn, dcpr_wd_mn, max_cd_mn)
       || !read_wd(maxwd_lg, dcpr_code_lg, dcpr_wd_lg, max_cd_lg))
      return (0);

   blocksize = (short)(code_rd >> (32 - 15));
   addbits(15);

   return (1);
}

void CAceTool::wrchar(char ch)
{
   dcpr_do++;

   dcpr_text[dcpr_dpos] = ch;
   dcpr_dpos++;
   dcpr_dpos &= dcpr_dican;
}

void CAceTool::copystr(long d,int l)
{
   int  mpos;

   dcpr_do += l;

   mpos = dcpr_dpos - d;
   mpos &= dcpr_dican;

   if( ((DWORD)mpos >= dcpr_dicsiz - maxlength) || (dcpr_dpos >= dcpr_dicsiz - maxlength))
   {
      while (l--)
      {
         dcpr_text[dcpr_dpos] = dcpr_text[mpos];
         dcpr_dpos++;
         dcpr_dpos &= dcpr_dican;
         mpos++;
         mpos &= dcpr_dican;
      }
   }
   else
   {
      while (l--)
         dcpr_text[dcpr_dpos++] = dcpr_text[mpos++];
      dcpr_dpos &= dcpr_dican;
   }
}

//---------------------- COMMENT DECOMPRESSION -----------------------------//

//main-comment-definitions
#define H_MCOMM_SIZE(head) (BUF2WORD((UCHAR*)&(head).AV+(head).AV_SIZE))
#define MCOMM_SIZE         H_MCOMM_SIZE(mhead)
#define H_MCOMM(head)      ((UCHAR*)&(head).AV+(head).AV_SIZE+sizeof((head).COMM_SIZE))
#define MCOMM              H_MCOMM(mhead)
//file-comment-definitions
#define H_FCOMM_SIZE(head) (BUF2WORD((UCHAR*)&(head).FNAME+(head).FNAME_SIZE))
#define FCOMM_SIZE         H_FCOMM_SIZE(fhead)
#define H_FCOMM(head)      ((UCHAR*)&(head).FNAME+(head).FNAME_SIZE+sizeof((head).COMM_SIZE))
#define FCOMM              H_FCOMM(fhead)
char* comm;

int comm_cpr_size=0;

void CAceTool::comment_skip()
{
	int i;

	if( head.HEAD_FLAGS & ACE_COMM )
	{
		if( head.HEAD_TYPE==MAIN_BLK )
		{
			comm = (char*)MCOMM;
			comm_cpr_size = MCOMM_SIZE;
		}
		else
		{
			comm = (char*)FCOMM;
			comm_cpr_size = FCOMM_SIZE;
		}
		i = sizeof(head) - (INT)(comm - (CHAR*) &head);
		if (comm_cpr_size > i)
		comm_cpr_size = i;
		dcpr_comm(i);              // decompress comment
	}
}

#define comm_cpr_hf(a,b) (a+b)

void CAceTool::dcpr_comm_init()
{
   int i;
   i = comm_cpr_size > size_rdb * sizeof(LONG) ? size_rdb * sizeof(LONG) : comm_cpr_size;
   memcpy(buf_rd, comm, i);
   code_rd = buf_rd[0];
   rpos = bits_rd = 0;
}

void CAceTool::dcpr_comm(int comm_size)
{
   short hash[comm_cpr_hf(255, 255) + 1];
   int  dpos = 0,
        c,
        pos,
        len,
        hs;

   memset(&hash, 0, sizeof(hash));
   if (comm_cpr_size)
   {
      dcpr_comm_init();
      len = code_rd >> (32 - 15);
      addbits(15);
      if (len >= comm_size)
         len = comm_size - 1;
      if (read_wd(maxwd_mn, dcpr_code_mn, dcpr_wd_mn, max_cd_mn))
         do
         {
            if (dpos > 1)
            {
               pos = hash[hs = comm_cpr_hf(comm[dpos - 1], comm[dpos - 2])];
               hash[hs] = dpos;
            }
            addbits(dcpr_wd_mn[(c = dcpr_code_mn[code_rd >> (32 - maxwd_mn)])]);
            if (rpos == size_rdb - 3)
               rpos = 0;
            if (c > 255)
            {
               c -= 256;
               c += 2;
               while (c--)
                  comm[dpos++] = comm[pos++];
            }
            else
            {
               comm[dpos++] = c;
            }
         }
         while (dpos < len);
      comm[len] = 0;
   }
}

