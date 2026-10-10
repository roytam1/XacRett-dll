#ifndef AFX_OPENGZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENGZ_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"

class COpenGz : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenGz() : COpenArchive( )
	{COpenGz::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	struct GzHeader
{
	BYTE Magic[2];	// 0x1f, 0x8b
	BYTE Method;	// deflate:0x08 , 他は予約
	BYTE Flag;		// bit0:Text
					// bit1:No2 or 3 or... of MultiPartGzip
					// bit2:特別な領域が存在
					// bit3:ファイル名が存在
					// bit4:コメントが存在
					// bit5:ファイルは暗号化されている
	DWORD Update;	// 最終更新日時。Unix形式。
	BYTE ExFlag;	// 拡張フラグ
	BYTE OSType;	// ファイルを作成したOSの種類(詳細略)

//	7:bit1がセットされているとき、パート番号(0,1,2,...)	2byte又はなし
//	8:bit2がセットされているとき、特別な領域の長さ		2byte又はなし
//	9:特別な領域									?byte又はなし
//	10:bit3がセットされているとき、元のファイル名	?byte又はなし('\0'で終わる)
//	11:bit4がセットされているとき、コメント			?byte又はなし('\0'で終わる)
//	12:bit5がセットされているとき、暗号化ヘッダ		12byte又はなし
};
//	13:圧縮データ						?byte
//	14:32ビットCRC						4byte
//	15:ファイルサイズ&0xffffffff		4byte

};
#endif