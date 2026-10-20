#ifndef AFX_LZHDECODER2_H__31F8AA04_3F84_11D4_8D96_8AB5A6462337__INCLUDED_
#define AFX_LZHDECODER2_H__31F8AA04_3F84_11D4_8D96_8AB5A6462337__INCLUDED_

#define MAXMATCH	256
#define THRESHOLD	3

#define NT      (16+3)
#define TBIT 	5
#define CBIT	9
#define NC 	    (255+MAXMATCH+2-THRESHOLD)
#define NPT 	0x80

#define N_CHAR      (256+60-THRESHOLD+1)
#define TREESIZE_C  (N_CHAR*2)
#define TREESIZE_P  (128*2)
#define TREESIZE    (TREESIZE_C+TREESIZE_P)
#define ROOT_C      0
#define ROOT_P      TREESIZE_C

enum lzh_method{ LZS,LZ4,LZ5,LH0,LH1,LH2,LH3,LH4,LH5,LH6,LH7,ARJ,LZS2,UNKNOWN };

class CLzhDecoder2  
{
//
// original source is
//
// LHa for UNIX 1.14d
//   http://www2m.biglobe.ne.jp/~dolphin/lha/lha-unix.htm
// Zoo 2.10 ( 88/01/27 )
//   http://www.vector.co.jp/soft/dos/util/se010838.html
//

public:
	// set the LArc 7 level field parameters for the bit
	// oriented LZSS methods; must be called before Decode()
	bool SetLzsParams( int pos_bits, int len_bits );
	void Decode( lzh_method mhd,FILE* infile,DWORD insize,
								FILE* outfile,DWORD outsize );

private:
	void Unstore();
	WORD Decode_C(lzh_method mhd)
	{switch( mhd ){
		case LH1:return decode_c_dyn();
		case LH2:return decode_c_dyn();	
		case LH3:return decode_c_st0();
		case LZS:return decode_c_lzs();
		case LZ5:return decode_c_lz5();
		case LZS2:return decode_c_lzs2();
		case ARJ:
		case LH4:
		case LH5:
		case LH6:
		case LH7:return decode_c_st1();
	}return 0;};
	WORD Decode_P(lzh_method mhd)
	{switch( mhd ){
		case LH1:return decode_p_st0();
		case LH2:return decode_p_dyn();	
		case LH3:return decode_p_st0();
		case LZS:return decode_p_lzs();
		case LZ5:return decode_p_lz5();
		case LZS2:return decode_p_lzs2();
		case ARJ:
		case LH4:
		case LH5:
		case LH6:
		case LH7:return decode_p_st1();
	}return 0;};

private:
	// ‘S”Ê“I‚É
	FILE *in,*out;
	DWORD cmpsize,orisize;

	DWORD loc;

	// lzs & lz5
	int matchpos;
	WORD decode_c_lzs();
	WORD decode_p_lzs();
	int flag,flagcnt;
	WORD decode_c_lz5();
	WORD decode_p_lz5();
	// parameterised bit oriented LZSS (LArc 7 -lz1-/-lz2-/-lz3-)
	WORD decode_c_lzs2();
	WORD decode_p_lzs2();
	int lzs_pos_bits, lzs_len_bits;
	int lzs_size, lzs_ce2, lzs_threshold, lzs_rotation;

	// lh1-lh3
	void ready_made(int method);
	void read_tree_c();
	void read_tree_p();
	WORD decode_c_st0();
	WORD decode_p_st0();
	WORD pt_code[NPT];

	WORD decode_c_dyn();
	WORD decode_p_dyn();
	void reconst(int start,int end);
	int swap_inc(int p);
	void update_c(int p);
	void update_p(int p);
	void make_new_node(int p);
	short child[TREESIZE],s_node[TREESIZE/2],parent[TREESIZE],
			block[TREESIZE], edge[TREESIZE], stock[TREESIZE];
	WORD freq[TREESIZE],total_p;
	int avail,n1,most_p,nn;
	DWORD nextcount,count;
	DWORD n_max;

	// lh4-lh7
	void make_table(WORD nchar,BYTE* bitlen,WORD tablebits,WORD* table);
	void read_pt_len(short nn,short nbit,short i_special);
	void read_c_len();
	WORD decode_c_st1();
	WORD decode_p_st1();

	WORD left[ 2*NC-1 ], right[ 2*NC-1 ];
	BYTE c_len[NC], pt_len[NPT];
	WORD c_table[4096],pt_table[256];

	WORD blocksize;
	int  pbit;
	int  np;

	// bit’PˆÊ‚Ìread&write
	void init_getbits();
	void fillbuf(BYTE n);
	WORD getbits(BYTE n);
	WORD bitbuf;BYTE subbitbuf,bitcount;
};

#endif
