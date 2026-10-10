#ifndef AFX_ISHTOOL_H__7104A281_4501_11D4_8D96_B7EEA0A4E334__INCLUDED_
#define AFX_ISHTOOL_H__7104A281_4501_11D4_8D96_B7EEA0A4E334__INCLUDED_

#include "Tool.h"

class CIshTool : public CTool
{
//
// original source is
//
// ish fot unix 2.01a5 ( 1993.09.25 )
//   http://www.vector.co.jp/soft/unix/util/se004518.html
//

public:
	CIshTool() : CTool( "Ish Decode" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"ish");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	FILE *ish;
	char IshName[MAX_PATH];

	bool FindHeader(bool bfirst=true);
	bool ProcessHeader(BYTE* dat,int l,bool bfirst);
	int  GetBlockData();
	bool GetNextVol();

	// １ブロック用のデータ
	BYTE block[100][100];
	int  blocklen[100];
	BYTE checker[100];
	BYTE checker2[100];
	int ErrCt,ErrLine[2];

	char h_LFN[MAX_PATH];
					// 1    必ず0
	BYTE h_Mv;		// 2    0:single vol format , 1-250:number of mv format files , 251-reserved
	DWORD h_FLen;	// 3- 6 もとのファイルの長さ(LE)
	WORD h_LLen;	// 7- 8 一行の長さ(69/63/73/69)
	WORD h_LNum;	// 9-10 ブロック当りの行数(68/62/72/68)
	BYTE h_MType;	// 11   元のﾌｧｲﾙの符号長？？(8:jis8, 16:jis7 or shift jis or shift jis(non-kana))
	BYTE h_IType;	// 12   ishﾌｧｲﾙの符号長(8:jis8, 13:jis7, 14: shift jis(non-kana), 15:shift jis)
	char h_FName[12];// 13-23 元のﾌｧｲﾙ名
					// 24   そのﾌｧｲﾙの種類(ｼﾞｬﾝﾙ)(0:未使用)
					// 25   対象OS
	bool h_bOld;	// 26   ish fileの拡張ﾀｲﾌﾟ（２ﾊﾞｲﾄ目の補足用）
					//      (２ﾊﾞｲﾄ目が0の時 -> 0:ols_sytle, 1:2行訂正)
	BYTE h_Flag;	// 27 0ﾋﾞｯﾄ -> 1:time stamp is available, 0:not available
					//    1ﾋﾞｯﾄ -> 1:difference of time is available, 0:not avail.
					//    2ﾋﾞｯﾄ -> 1:CRC-16(file) is available, 0:not avail.
					//    3ﾋﾞｯﾄ -> 1:CRC-32(file) is available, 0:not avail.
					//    4ﾋﾞｯﾄ -> 1:CRC-16(volume) is available, 0:not avail.
	DWORD h_LcTime;	//28-31
	DWORD h_DifTm;	//32-34  符号付き３バイト。時差(秒)
	WORD  h_FCRC16;	//35-36  file全体のCRC-16
	DWORD h_FCRC32;	//37-40  file全体のCRC-32
	DWORD h_VLen;	//41-44  各volumeに含まれるByte数(single volume formatの時は0)
	WORD  h_VCRC16;	//45-46  このvolumeのCRC-16(single volume formatの時は0)
					//47-61 (0)(ﾍｯﾀﾞをjis8 formatで作成した場合には、47～67ﾊﾞｲﾄ目)
	WORD  h_CRC;	//62-63 １～67(61/67/61)ﾊﾞｲﾄ目までのCRC
					//       (ﾍｯﾀﾞをjis8 formatで作成した場合には、68～69ﾊﾞｲﾄ目)
};

#endif
