#ifndef AFX_ACETOOL_H__8D834FA0_414C_11D4_8D96_958B20B6F733__INCLUDED_
#define AFX_ACETOOL_H__8D834FA0_414C_11D4_8D96_958B20B6F733__INCLUDED_

#include "Tool.h"

#define PATH_MAX   512
#define CRC_MASK 0xFFFFFFFFL
#define CRCPOLY  0xEDB88320L
#define size_rdb  1024
#define size_wrb  2048
#define size_buf  1024
#define size_headrdb (sizeof(AceHead)-20)

#define maxdic      22
#define maxwd_mn    11
#define maxwd_lg    11
#define maxwd_svwd   7
#define maxlength  259
#define maxdis2    255
#define maxdis3   8191
#define maxcode   (255+4+maxdic)
#define svwd_cnt    15
#define max_cd_mn (256+4+(maxdic+1)-1)
#define max_cd_lg (256-1)

//archive-header-flags
#define ACE_LIM256   1024
#define ACE_MULT_VOL 2048
#define ACE_AV       4096
#define ACE_RECOV    8192
#define ACE_LOCK    16384
#define ACE_SOLID   32768

//file-header-flags
#define ACE_ADDSIZE  1
#define ACE_PASSW    16384
#define ACE_SP_BEF   4096
#define ACE_SP_AFTER 8192
#define ACE_COMM     2

//block types
#define MAIN_BLK 0
#define FILE_BLK 1
#define REC_BLK  2

//known compression types
#define TYPE_STORE 0
#define TYPE_LZ1   1

//io
#define BUFP2WORD(p) *(WORD*)((p+=2)-2)
#define BUFP2LONG(p) *(DWORD*)((p+=4)-4)
#define BUF2WORD(p) (*(WORD*)p)
#define BUF2LONG(p) (*(DWORD*)p)

struct AceHead
{
	WORD  HEAD_CRC;
	WORD  HEAD_SIZE;
	BYTE  HEAD_TYPE;
	WORD  HEAD_FLAGS;
	DWORD ADDSIZE;
	BYTE  other[2048];
};
struct AceMainHead
{
	WORD  HEAD_CRC;
	WORD  HEAD_SIZE;
	BYTE  HEAD_TYPE;
	WORD  HEAD_FLAGS;

	BYTE  ACESIGN[7];
	BYTE  VER_MOD;
	BYTE  VER_CR;
	BYTE  HOST_CR;
	BYTE  VOL_NUM;
	DWORD TIME_CR;
	WORD  RES1;
	WORD  RES2;
	DWORD RES;
	BYTE  AV_SIZE;
	BYTE  AV[256];
	WORD  COMM_SIZE;
	BYTE  COMM[2048];
};
struct AceTech
{
   UCHAR TYPE;
   UCHAR QUAL;
   USHORT PARM;
};
struct AceFileHead
{
	WORD HEAD_CRC;
	WORD HEAD_SIZE;
	BYTE HEAD_TYPE;
	WORD HEAD_FLAGS;

	DWORD PSIZE;
	DWORD SIZE;
	DWORD FTIME;
	DWORD ATTR;
	DWORD CRC32;
	AceTech TECH;
	WORD  RESERVED;
	WORD  FNAME_SIZE;
	BYTE  FNAME[PATH_MAX];
	WORD  COMM_SIZE;
	BYTE  COMM[2048];
};
struct AceDat
{
	int sol,vol,vol_num;
	DWORD time_cr;
};

class CAceTool : public CTool
{
//
// original source is
//
// public UNACE v1.2b ( June 1998 )
//   ftp://ftp.externet.hu/pub/mirror/sac/pack/unacepub.zip
// Copyright by Marcel Lemke
//

public:
	CAceTool() : CTool( "Ace “WŠJ" )
	{
		buf_rd = new DWORD[size_rdb];
		buf = new char[size_buf];
		buf_wr = new char[size_wrb];
		readbuf = new BYTE[size_headrdb];

		t_mhead = (AceMainHead*)&head;
		t_fhead = (AceFileHead*)&head;

		skipsize=0;

		//initialize CRC table
		rd_crc=0;
		DWORD r,i,j;
		for( i=0; i<=255; i++ )
		{
			for( r=i,j=8; j; j-- )
				r = (r & 1) ? (r >> 1) ^ CRCPOLY : (r >> 1);
			crctable[i] = r;
		}

		rpos           =0,
		dcpr_do        =0,
		dcpr_do_max    =0,
		blocksize      =0,
		dcpr_dic       =0,
		dcpr_oldnum    =0,
		bits_rd        =0,
		dcpr_frst_file =0;
		dcpr_dpos      =0,
		cpr_dpos2      =0,
		dcpr_dicsiz    =0,
		dcpr_dican     =0,
		dcpr_size      =0,
		dcpr_olddist[0]=0,
		dcpr_olddist[1]=0,
		dcpr_olddist[2]=0,
		dcpr_olddist[3]=0,
		code_rd        =0;
		dcpr_init();
	}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"ace");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

	~CAceTool()
	{
		delete [] buf_rd;
		delete [] buf;
		delete [] buf_wr;
		delete [] readbuf;
		delete [] dcpr_text;
	}

private:
	//-------- file
	int archan,wrhan;
	long skipsize;
	char arcname[PATH_MAX];

	//-------- header & cetra
	AceHead head;
	AceMainHead *t_mhead;
	AceFileHead *t_fhead;
	AceDat adat;
#define mhead (*t_mhead)
#define fhead (*t_fhead)

	//-------- buffers
	DWORD *buf_rd;
	char  *buf;
	char  *buf_wr;
	BYTE  *readbuf;

	//-------- crc
	DWORD getcrc(DWORD crc,BYTE* addr,int len)
	{while(len--)crc = crctable[(BYTE)crc^(*addr++)]^(crc>>8);return crc;};
	DWORD crctable[256],rd_crc;

	//-------- functions
	bool find_arc_head();
	bool open_archive(const char* aname);
	void analyze_file();
	void extract_file();
	bool read_header();

	bool proc_vol();
	bool proc_next_vol();
	int read_adds_blk(char* buffer,int len);

	void comment_skip();
	void dcpr_comm_init();
	void dcpr_comm(int comm_size);

	//-------- decompressors
	void dcpr_init();
	bool dcpr_init_file();
	int  dcpr_adds_blk(char* buf, UINT len);
	int  unstore(char* buf,UINT len);
	int decompress_blk(char* buf, UINT len);
	void decompress();
	int  makecode(UINT maxwd, UINT size1_t, BYTE* wd, WORD* code);
	int read_wd(UINT maxwd, WORD* code, BYTE* wd, int max_el);
	int calc_dectabs();
	void wrchar(char ch);
	void copystr(long d,int l);
	void readdat();

	short rpos,dcpr_do,dcpr_do_max,blocksize,dcpr_dic,dcpr_oldnum,bits_rd,dcpr_frst_file;
	WORD  dcpr_code_mn[1<<maxwd_mn],dcpr_code_lg[1<<maxwd_lg];
	BYTE  dcpr_wd_mn[maxcode+2],dcpr_wd_lg[maxcode+2],wd_svwd[svwd_cnt];
	DWORD dcpr_dpos,cpr_dpos2,dcpr_dicsiz,dcpr_dican,dcpr_size,dcpr_olddist[4],code_rd;
	char* dcpr_text;

	//-------- quicksort
	WORD sort_org[maxcode + 2];
	BYTE sort_freq[(maxcode + 2) * 2];
	void quicksort(int n);
	void sortrange(int left, int right);
};

#endif
