#ifndef AFX_OPENIso_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENIso_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include <vector>

class COpenIso : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenIso() : COpenArchive( )
	{COpenIso::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	#pragma pack(push)
	#pragma pack(1)
	struct UINT32BOTH
	{
		UINT32 l;
		UINT32 b;
	};
	struct UINT16BOTH
	{
		UINT16 l;
		UINT16 b;
	};

	struct DATETIME_S
	{
		UINT8 year;//西暦年-1900 0～255(1900年～2155年) 
		UINT8 month;//月 1～12 
		UINT8 day;//日 1～31 
		UINT8 hour;//時 0～23 
		UINT8 minute;//分 0～59 
		UINT8 second;//秒 0～59 
		INT8 l;//UTCからの偏差(15分単位) -48～+52 (-12時間～+13時間) 
	};
	struct DATETIME_L
	{
		char year[4];//西暦年-1900 0～255(1900年～2155年) 
		char month[2];//月 1～12 
		char day[2];//日 1～31 
		char hour[2];//時 0～23 
		char minute[2];//分 0～59 
		char second[2];//秒 0～59
		char msecond[2];//1/100秒 0～99 
		INT8 l;//UTCからの偏差(15分単位) -48～+52 (-12時間～+13時間) 
	};
	struct ISO_DIRECTORY_RECORD {
		UINT8 length;//ディレクトリレコードの長さ(LEN_DR)
		UINT8 ext_attr_length;//拡張属性レコードの長さ
		UINT32BOTH extent;//エクステントの位置
		UINT32BOTH size;//データ長
		DATETIME_S date;//記録日付及び時刻
		char flags;//ファイルフラグ
		UINT8 file_unit_size;//ファイルユニットの大きさ
		UINT8 interleave;//インタリーブ間隙の大きさ
		UINT16BOTH volume_sequence_number;//ボリューム順序番号
		unsigned char name_len;//ファイル識別子の長さ(LEN_FI)
		char name;				//ファイル/ディレクトリ識別子
	};

	struct ISO_PRIMARY_DESCRIPTOR {
		UINT8 type; //ボリューム記述子種別
		char id[5];//規格識別子
		UINT8 version;//ボリューム記述子版数
		char unused1;//ボリュームフラグ(SVDのみ)
		char system_id[32]; // astr(32)システム識別子
		char volume_id[32]; // dstr(32)ボリューム識別子
		char unused2[8];//(未使用)(*)
		UINT32BOTH volume_space_size; //ボリューム空間の大きさ
		char escape_sequences[32];//エスケープシーケンス(SVDのみ)
		UINT16BOTH volume_set_size;//ボリューム集合の大きさ(*)
		UINT16BOTH volume_sequence_number; //ボリューム順序番号(*)
		UINT16BOTH logical_block_size; //論理ブロック長(*)
		UINT32BOTH path_table_size; //パステーブルの大きさ
		UINT32 type_l_path_table; //L形パステーブルの位置
		UINT32 opt_type_l_path_table;//任意L形パステーブルの位置
		UINT32 type_m_path_table;//M形パステーブルの位置
		UINT32 opt_type_m_path_table;//任意M形パステーブルの位置
		ISO_DIRECTORY_RECORD  root_directory_record;//ルートディレクトリ用ディレクトリレコード 
		char volume_set_id[128];//ボリューム集合識別子
		char publisher_id[128];//出版者識別子
		char preparer_id[128];//データ編集者識別子
		char application_id[128];//応用システム識別子
		char copyright_file_id[37];//著作権ファイル識別子
		char abstract_file_id[37];//抄録ファイル識別子
		char bibliographic_file_id[37];//書誌ファイル識別子
		DATETIME_L creation_date;//ボリューム作成日付及び時刻
		DATETIME_L modification_date;//ボリューム更新日付及び時刻
		DATETIME_L expiration_date;//ボリューム失効日付及び時刻
		DATETIME_L effective_date;//ボリューム発効日付及び時刻
		UINT8 file_structure_version;//ファイル構造版数
		char unused4;//(予約)(*)
		char application_data[511];//応用システム用
		char unused5[653];//(予約)(*)
	};
	struct ELTORITO_BOOT_DESCRIPTOR {
		UINT8 id;//ボリューム記述子種別
		char id2[5];//規格識別子
		UINT8 version;//ボリューム記述子版数
		char system_id[32];//起動システム識別子
		char unused2[32];//起動識別子;
		char unused5[1977];
	};

	struct ISO_PATH_TABLE{
		UINT8  name_len;//ディレクトリ識別子の長さ(LEN_DI)
		UINT8  extname_len;//拡張属性レコードの長さ
		UINT32 extent;//エクステントの位置
		UINT16 parent;//親ディレクトリの番号
		char* name;//ディレクトリ識別子
	};
	#pragma pack(pop)
	#define PSECTOR 2048

	void Listing(const BYTE* b,ISO_DIRECTORY_RECORD* dr,LPCSTR parent);
	void isoTimeToDosTime(DATETIME_S* itime,WORD* date,WORD* time);
	char* WtoM(const wchar_t *pUcsStr, char* buff);

	std::vector<INDIVIDUALINFO> m_info;
	std::vector<INDIVIDUALINFO>::iterator m_curinfo;
	bool joliet;
};

#endif