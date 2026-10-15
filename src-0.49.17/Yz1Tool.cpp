
#include "stdafx.h"
#include "Yz1Tool.h"
#include "yz32lib/yz32lib.h"
#include "XacRett.h"
bool CYz1Tool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<20 )
		return false;

	const BYTE *head = NULL;
	BYTE temp[20];

	if( memcmp( hdr,"yz01",4 )==0 )
		head = hdr;
	else // SFX ?
	{
		if( fsize==siz )
		{
			DWORD yz1siz = (hdr[fsize-4]<<24)+(hdr[fsize-3]<<16)
						  +(hdr[fsize-2]<<8)+(hdr[fsize-1]);
			if( yz1siz+4<fsize && yz1siz>=20 )
			{
				if( memcmp( hdr+fsize-(yz1siz+4),"yz01",4 )==0 )
					head = hdr+fsize-(yz1siz+4);
			}
		}
		else
		{
			FILE* yz1 = fopen( fname,"rb" );
			if( yz1 )
			{
				fseek( yz1, -4, SEEK_END );
				DWORD yz1siz = (fgetc(yz1)<<24)+(fgetc(yz1)<<16)
							  +(fgetc(yz1)<<8)+(fgetc(yz1));
				if( yz1siz+4<fsize && yz1siz>=20 )
				{
					fseek( yz1, fsize-(yz1siz+4), SEEK_SET );
					fread( temp, 1, 20, yz1 );
					if( memcmp( temp,"yz01",4 )==0 )
						head = temp;
				}
				fclose( yz1 );
			}
		}
	}

	if( !head )
		return false;

	DWORD alsz = (head[ 8]<<24)+(head[ 9]<<16)+(head[10]<<8)+(head[11]);
	DWORD hdsz = (head[12]<<24)+(head[13]<<16)+(head[14]<<8)+(head[15]);
	DWORD fnum = (head[16]<<24)+(head[17]<<16)+(head[16]<<8)+(head[19]);
	return (alsz>hdsz && hdsz>fnum);
}

#if 1 //----------------------------------------------------------------------------

#pragma warning( disable: 4786 )
#include "yz32lib/yzFileDe.cxx"
#include "yz32lib/yzDecHad.hpp"
#include "yz32lib/YZ_MsgTbl.h"
#include "yz32lib/yzDecode.cxx"

YzMessage::language_type YzMessage::language = YzMessage::language_type::language_japanese;
#define isdblb(c) (kiutil::lb[(unsigned char)(c)]==2)
#define step(p) (p+=kiutil::lb[(unsigned char)*(p)])

class CKiYzEv : public yzFileEv
{
public:
	CKiYzEv( CYz1Tool* p ) : decoder(0),tool(p) {}
	yzFileDecode* decoder;
private:
	CYz1Tool* tool;
	void MessEX   ( const char*, const int& ) {}
	void MessINF  ( const char*, const int& ) {}
	int MessYN   ( const char*, const int& ) {return 0;}
	char* MessINPUT( const char*, const int& ) {return tool->GetPassword();}
	int gauge_EncodeInit( const char*, FileName&, FileTbl&, FileTbl& ) {return 0;}
	int gauge_DecodeInit( const char*, const yzDecodeHeader& ) {return 0;}
	int gauge_String( const char* ) {return 0;}
	int gauge_Value( const int&, const int& ) {return 0;}
	int gauge_WriteInfo( const yzFileStat& in_yzfstat, const int&, const int& )
	{
		/* `no` was 1-based (the engine calls GaugeSetOutputValue(i+1))
		   while the body indexed m_file_name_tbl[no] directly, so the LAST
		   member of EVERY archive read one past the end.  That produced a
		   garbage std::string whose c_str() reached kiutil::StrMatch and
		   faulted (0xC0000005) after the file had already been written.
		   The bridge hands us a stat built from the correct 0-based
		   index; copy it, because the code below rewrites the name. */
		string ext_name = in_yzfstat.m_file_name;

		if (tool->m_Endflg) return 2;//追加 By Bonty
		//ワイルドカード処理
		if (!tool->CheckStr(ext_name.c_str()))//追加 By Bonty
		{
			return 1;
		}
		
		// pathMake
		const char* st = ext_name.c_str();
		while( *st=='/' || *st=='\\' || *st=='?' ) st++;
		if( st[0]!='\0' && st[1]==':' )//            st+=2;
		{
			ext_name[1] = '_';
		}
		while( *st=='/' || *st=='\\' || *st=='?' ) st++;
		for( const char *p=st,*q=st; *p!='\0'; step(p) )
		{
			if( isdblb(*p) )
				continue;
			if( *p=='\\' || *p=='/' )
			{
				// q-pが全部.だったら_に変換
				#define DO_DTV_CHECK
				#ifdef DO_DTV_CHECK
					bool all_dots = true;
					for(const char* r=q; r!=p; ++r)
						if( *r != '.' )
							{ all_dots = false; break; }
					if( all_dots && p-q>=2 )
						for(const char* r=q; r!=p; ++r)
							ext_name[r-st] = '_';
				#endif
				// qを更新
				q = p+1;
			}
		}
		if(tool->arcproc)//SetOwnerWindowのための処理
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,ext_name.c_str());
			if(tool->arcproc(tool->hWnd,tool->Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
				return 1;
		}

		return 0;
	}
};
bool CYz1Tool::Extract( const char* fname,const char* ddir )
{
	CKiYzEv ev( this );
	InEv	in(fname);
	yzFileDecode decoder(in, &ev, ddir );
	ev.decoder = &decoder;
	return 0<=decoder.DecodeFile( 0 );
}

#else //----------------------------------------------------------------------------

#pragma comment( lib, "yz32lib/yz32_vc42m.lib" )

class CKiYzEv : public yzFileEv
{
public:
	CYz1Tool* tool;
	CKiYzEv( CYz1Tool* p ){tool=p;}

	void  MessEX(char*){}
	void  MessINF(char*){}
	int   MessYN(char*){return 0;}
	char* MessINPUT(char*){return tool->GetPassword();}
	void  GaugeInit(int){}
	void  GaugeSetText(char*){}
	int   GaugeSetInputValue(int){return 0;}
	int   GaugeSetOutputValue(int){return 0;}
};

bool CYz1Tool::Extract( const char* fname,const char* ddir )
{
	CKiYzEv ev( this );
	yzFileDecode dec( &ev,const_cast<char*>(ddir) );
	return (0<=dec.DecodeFile( const_cast<char*>(fname),0 ));
}

#endif
