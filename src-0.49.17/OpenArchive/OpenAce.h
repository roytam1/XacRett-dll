#ifndef AFX_OPENACE_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENACE_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_

#include "OpenArchive.h"

class COpenAce : public COpenArchive
{
private:
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

	AceHead head;
	AceMainHead *t_mhead;
	AceFileHead *t_fhead;
	AceDat adat;
#define mhead (*t_mhead)
#define fhead (*t_fhead)
#define max_sfx_size 65536

	bool find_arc_head();
	bool open_archive(const char* aname);
	//void analyze_file();
	//void extract_file();
	bool read_header();

	void comment_skip();

//-------- file
	int archan,wrhan;
	long skipsize;
	char arcname[PATH_MAX];
	char  buf[size_buf];
	BYTE  readbuf[size_headrdb];

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

	DWORD getcrc(DWORD crc,BYTE* addr,int len)
	{while(len--)crc = crctable[(BYTE)crc^(*addr++)]^(crc>>8);return crc;};
	DWORD crctable[256],rd_crc;

	bool FindSub(INDIVIDUALINFO *lpSubInfo);

public: //-- COpenArchive 共通インターフェイス -----------

	COpenAce() : COpenArchive( )
	{
		COpenAce::fp=NULL;

		DWORD r,i,j;
		for( i=0; i<=255; i++ )
		{
			for( r=i,j=8; j; j-- )
				r = (r & 1) ? (r >> 1) ^ CRCPOLY : (r >> 1);
			crctable[i] = r;
		}
		RtlZeroMemory(&head ,sizeof(AceHead));
		t_mhead = (AceMainHead*)&head;
		t_fhead = (AceFileHead*)&head;

	}

	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);

};

#endif