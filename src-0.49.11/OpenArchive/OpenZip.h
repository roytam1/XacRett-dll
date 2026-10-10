#ifndef AFX_OPENZIP_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENZIP_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include <vector>

class COpenZip : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenZip() :COpenArchive( )
	{COpenZip::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	bool Find_sub(INDIVIDUALINFO *lpSubInfo);

	// ローカルヘッダ
	struct ZipLocalHeader
	{
		// 'P' 'K' 03 04
		 WORD ver; // version_needed_to_extract
		 WORD flg; // general_purpose_bit_flag
		 WORD mhd; // compression_method
		 WORD tim; // last_modified_file_time
		 WORD dat; // last_modified_file_date
		DWORD crc; // crc32
		DWORD csz; // compressed-size
		DWORD usz; // uncompressed-size
		 WORD fnl; // filename-len
		 WORD exl; // extra_field_length

		char fnm[MAX_PATH];
	//	BYTE ext[];
	};
	//Central directory structure
	struct ZipCDHeader
	{
		char sig[4];//0x02014b50
		WORD ver_m; // version made
		WORD ver; // version_needed_to_extract
		WORD flg; // general_purpose_bit_flag
		WORD mhd; // compression method 
		WORD tim; // last_modified_file_time
		WORD dat; // last_modified_file_date
		DWORD crc; // crc32
		DWORD csz; // compressed-size
		DWORD usz; // uncompressed-size
		WORD fnl; // filename-len
		WORD exl; // extra_field_length
		WORD fcl;	//file comment length
		WORD disk_s; //disk number start 2 bytes 
		WORD attributes_i; //internal file attributes 2 bytes 
		WORD attributes_e; //external file attributes 4 bytes 
		WORD attributes_e_d;
		DWORD offset;	//relative offset of local header 4 bytes 
		/*file name (variable size) 
		extra field (variable size) 
		file comment (variable size)*/ 
	};
	//End of central dir record
	struct ZipEndCDHeader
	{
		char sig[4];//0x06054b50
		WORD nod; // number of this disk
		WORD nodw; // number of the disk with the start of the central directo
		WORD tned; // total number of entries in the central directory on this disk 
		WORD tne; // total number of entries in the central directory
		DWORD d_size; // size of the central directory 
		DWORD offset; // offset of start of central directory

		WORD comment; // filename-len
	};
	// 圧縮メソッドの種類
	enum ZipMethod
	{
		Stored,		// 0
		Shrunk,		// 1
		Reduced1,	// 2-5
		Reduced2,
		Reduced3,
		Reduced4,
		Imploded,	// 6
		Tokenized,	// 7 ( not supported )
		Deflated,	// 8
		EnhDeflated,// 9 ( not supported )
		DclImploded,//10 ( not supported )

		Err=178     // this value is used by xacrett (^^;
	};
	bool doHeader( ZipLocalHeader* hdr,unsigned char* buf );
	bool read_header( ZipLocalHeader* hdr,unsigned char* buf );
	long get_cdstructure_offset();
	void get_cdhead(long offset);
	std::vector<ZipCDHeader> cdhead;
	std::vector<ZipCDHeader>::iterator cur_cdhead_it;
};

#endif