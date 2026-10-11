#ifndef AFX_OPENYZ1_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENYZ1_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include "../yz32lib/yz32lib.h"
#include "../yz32lib/yzFileDe.cxx"

class COpenYz1 : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenYz1() : COpenArchive( )
	{COpenYz1::fp=NULL;}

	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	class CYzEv : public yzFileEv
	{
	public:

		void MessEX   ( const char*, const int& ) {}
		void MessINF  ( const char*, const int& ) {}
		int MessYN   ( const char*, const int& ) {return 0;}
		char* MessINPUT( const char*, const int& ) {return "";}
		int gauge_EncodeInit( const char*, FileName&, FileTbl&, FileTbl& ) {return 0;}
		int gauge_DecodeInit( const char*, const yzDecodeHeader& ) {return 0;}
		int gauge_String( const char* ) {return 0;}
		int gauge_Value( const int&, const int& ) {return 0;}
		int gauge_WriteInfo( const yzFileStat&, const int& no, const int& ){return 0;}
	};
	void FindHeader(yzFileDecode* dec);
	bool FindSub(INDIVIDUALINFO *lpSubInfo);
	friend class CYzEv;
	int File_Count;
	INDIVIDUALINFO *lpInfo;//不本意ながら全部の情報を一度に読み込む^^;
};

#endif