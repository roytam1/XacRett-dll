
#include "stdafx.h"
#include "IsTool.h"
#include "ZData/zdatas.h"


//
// 解析結果MEMO by K.INABA
//
/*
 ### .hdr ###

  0x00 : iscab_header

  hdr.ofsCabDesc : iscab_desc
  hdr.ofsCabDesc + desc.ofsXXXX : XXXX

  hdr.ofsCabDesc + desc.ofsDFT : DFT //sizeof(DFT)==desc.cbDFT

  DFT
    0               : dword[desc.cDirs]     //"offset from DFT" to dirname
	desc.ofsFilesDFT: filedesc[desc.cFiles] //

  filedesc.ofsName  //"offset from DFT" to filename
*/



#if defined(__BORLANDC__)
#pragma option -a-
#else
#pragma pack(1)
#endif

// ヘッダ ( .cab .hdr )
enum { LINK_NONE=0, LINK_PREV=1, LINK_NEXT=2, LINK_BOTH=3 };
struct iscab_header {
	DWORD   Signature;		// 00 ( []= 0x49, 0x53, 0x63, 0x28 )
	DWORD   Version;		// 04
	DWORD   LinkFlags;		// 08 (cab: マルチボリューム関係)
	DWORD   ofsCabDesc;		// 0c (hdr: iscab_descのファイル先頭からの位置)
	DWORD   cbCabDesc;		// 10 (hdr: iscab_descのサイズ)
	__int64 ofsCompData;	// 14 (cab: 圧縮データの位置)
	DWORD   FirstFile;		// 1c
	DWORD   LastFile;		// 20
	__int64 ofsFirstData;	// 24
	__int64 cbFirstExpanded;// 2c
	__int64 cbFirstData;	// 34
	__int64 ofsLastData;	// 3c
	__int64 cbLastExpanded;	// 44
	__int64 cbLastData;		// 4c
};

// 情報( .hdr )
static const int CFG_MAX=0x47;
struct iscab_desc {
	DWORD ofsStrings;				// 0
	DWORD junk11;					// 4
	DWORD ofsCompList;				// 8
	DWORD ofsDFT;					// c
	DWORD junk21;					// 10
	DWORD cbDFT;					// 14
	DWORD cbDFT2;					// 18
	WORD  cDirs;					// 1c
	DWORD phpDirs;					// 1e
	WORD  Pad01;					// 22
	DWORD cbDirs;					// 24
	DWORD cFiles;					// 28
	DWORD ofsFilesDFT;				// 2c
	WORD  cCompTabInfo;				// 30
	DWORD ofsCompTab;				// 32
	DWORD junk51, junk52;			// 36
	DWORD ofsFileGroup[CFG_MAX];	// 3e
	DWORD ofsComponent[CFG_MAX];	// 15a
	DWORD ofsSTypes;				// 276
	DWORD ofsSTab;					// 27a
};

// 格納ファイル情報( .hdr )
#define DESC_SPLIT			1L
#define DESC_ENCRYPTED		2L
#define DESC_COMPRESSED		4L
#define DESC_INVALID		8L
struct iscab_file5 {
	DWORD ofsName;
	DWORD DirIndex;
	WORD  DescStatus;
	DWORD cbExpanded;
	DWORD cbCompressed;
	DWORD Attrs;
	WORD  FatDate;
	WORD  junk1;
	WORD  FatTime;
	WORD  junk2;
	DWORD junk3, junk4;
	DWORD ofsData;
};
struct iscab_file6 {
	WORD    DescStatus;
	__int64 cbExpanded;
	__int64 cbCompressed;
	__int64 ofsData;
	BYTE    MD5Sig[16];
	DWORD   dwVerMS;
	DWORD   dwVerLS;
	DWORD   junk1[2];
	DWORD   ofsName;
	WORD    DirIndex;
	DWORD   Attrs;
	WORD    FatDate, FatTime;
	DWORD   junk2;
	DWORD   PrevDupe;
	DWORD   NextDupe;
	BYTE    LinkFlags;
	WORD    Volume;
};







bool CIsTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz < sizeof(iscab_header) )
		return false; // Size
	if( hdr[0]!=0x49 || hdr[1]!=0x53 || hdr[2]!=0x63 || hdr[3]!=0x28 )
		return false; // Signature
	if( !(hdr[7] & 0x01) )
		return false; // Version
	return true;
}

bool CIsTool::Extract( const char* fname, const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	setupNamePattern( fname );
	return ( readHeader() && extractAll() );
}



void CIsTool::setupNamePattern( const char* fname )
{
	fp         = NULL;
	m_FileList = NULL;

	strcpy( m_arcNamePat, fname );

	char *d, *p;
	for( d=NULL,p=m_arcNamePat; *p; p=CharNext(p) )
		if( *p=='.' )
			d = p;
	if( !d )
		d = p;
	while( m_arcNamePat<=(d-1) && '0'<=*(d-1) && *(d-1)<='9' )
		d--;
	*d = '\0';
}

bool CIsTool::openArc( int vol, const char* ext )
{
	closeArc();

	char tmp[MAX_PATH*3];
	sprintf( tmp, "%s%d.%s", m_arcNamePat, vol, ext );
	return (NULL != (fp=fopen( tmp, "rb" )));
}

void CIsTool::closeArc()
{
	if( fp )
		fclose( fp ), fp=NULL;
}

bool CIsTool::readHeader()
{
	bool ans = false;
	if( openArc( 1, "hdr" ) )
	{
		// Header
		iscab_header hdr;
		if( sizeof(hdr) == fread( &hdr, 1, sizeof(hdr), fp ) )
			if( hdr.Signature == 0x28635349 )
				if( hdr.Version & 0x01000000 )
				{
					if( (hdr.Version&0x0000ff00) < 0x6000 ) // Old Header
						hdr.ofsCompData &=0x00000000ffffffff;

					// Descriptor
					char* dsc_buffer;
					iscab_desc* pdsc;
					pdsc = (iscab_desc*)(dsc_buffer = new char[hdr.cbCabDesc]);
					fseek( fp, hdr.ofsCabDesc, SEEK_SET );
					if( (int)hdr.cbCabDesc == fread( pdsc, 1, hdr.cbCabDesc, fp ) )
					{
						// DFT; Dirs & Files Table
						char* dft = new char[pdsc->cbDFT];
						fseek( fp, hdr.ofsCabDesc + pdsc->ofsDFT, SEEK_SET );
						if( (int)pdsc->cbDFT == fread( dft, 1, pdsc->cbDFT, fp ) )
						{
#define DirName(_i)   (dft+((DWORD*)dft)[(_i)])
#define FileDesc5(_i) ((iscab_file5*)(dft+((DWORD*)(dft+pdsc->ofsFilesDFT))[(_i)]))
#define FileDesc6(_i) (((iscab_file6*)(dft+pdsc->ofsFilesDFT))+(_i))
#define FileName(_fd) (dft+(_fd)->ofsName)

							ans        = true;
							m_FileList = new fileInf[ pdsc->cFiles ];

							if( (hdr.Version&0x0000ff00) >= 0x6000 ) // New Header
							{
								iscab_file6* f;
								int j=0;
								for( unsigned int i=0; i<pdsc->cFiles; i++ )
								{
									f = FileDesc6(i);
									if( (f->DescStatus & DESC_INVALID)
									 || (f->LinkFlags & LINK_PREV)
									 || (f->ofsData==0) )
										continue;

									m_FileList[j].cbExpanded   = f->cbExpanded;
									m_FileList[j].cbCompressed = f->cbCompressed;
									m_FileList[j].ofsData      = f->ofsData;
									m_FileList[j].attr         = f->Attrs;
									m_FileList[j].date         = f->FatDate;
									m_FileList[j].time         = f->FatTime;
									m_FileList[j].cmprs        = (f->DescStatus&DESC_COMPRESSED)!=0;
									m_FileList[j].crypt        = (f->DescStatus&DESC_ENCRYPTED)!=0;
									strcpy( m_FileList[j].name, DirName(f->DirIndex) );
									if( m_FileList[j].name[0] )
										strcat( m_FileList[j].name, "\\" );
									strcat( m_FileList[j].name, FileName(f) );

									j++;
								}
								m_FileNum = j;
							}
							else // Old Header
							{
								iscab_file5* f;
								int j=0;
								for( unsigned int i=0; i<pdsc->cFiles; i++ )
								{
									f = FileDesc5(i);
									if( (f->DescStatus & DESC_INVALID) 
									 || (f->ofsData==0) )
										continue;

									m_FileList[j].cbExpanded   = f->cbExpanded;
									m_FileList[j].cbCompressed = f->cbCompressed;
									m_FileList[j].ofsData      = f->ofsData;
									m_FileList[j].attr         = f->Attrs;
									m_FileList[j].date         = f->FatDate;
									m_FileList[j].time         = f->FatTime;
									m_FileList[j].cmprs        = (f->DescStatus&DESC_COMPRESSED)!=0;
									m_FileList[j].crypt        = (f->DescStatus&DESC_ENCRYPTED)!=0;
									strcpy( m_FileList[j].name, DirName(f->DirIndex) );
									if( m_FileList[j].name[0] )
										strcat( m_FileList[j].name, "\\" );
									strcat( m_FileList[j].name, FileName(f) );

									j++;
								}
								m_FileNum = j;
							}

#undef FileName
#undef FileDesc6
#undef FileDesc5
#undef DirName
						}
						delete [] dft;
					}
					delete [] dsc_buffer;
				}
		closeArc();
	}

	return ans;
}

int __stdcall CIsTool::_w_(void* b, DWORD* s, CIsTool* p)
{
	if( p->wp )
		*s = fwrite( b, 1, *s, p->wp );
	return 0;
}

int __stdcall CIsTool::_r_(void* b, DWORD* s, CIsTool* p)
{
	*s = p->readArc( b, *s );
	if( *s==0xffffffff )
	{
		*s=0;
		return -1;
	}
	return 0;
}

bool CIsTool::extractAll()
{
	if( !readInit() )
		return false;

	ZDataSetup( 2, 0xfa372dea, 0 );
	ZDataSetInfo( INFO_BUFFER_PTR,     (DWORD)CTool::common_buf );
	ZDataSetInfo( INFO_BUFFER_SIZE,    XACR_BUFSIZE );
	ZDataSetInfo( INFO_CALLBACK_PARAM, (DWORD)this );
	ZDataSetInfo( INFO_WRITE_CALLBACK, (DWORD)_w_ );
	ZDataSetInfo( INFO_READ_CALLBACK,  (DWORD)_r_ );

	for( int i=0; i<m_FileNum; i++ )
	{
		if (m_Endflg) return true;//追加 By Bonty
		if(!CheckStr(m_FileList[i].name))
		{
			if( (unsigned int)m_FileList[i].ofsData < (unsigned int)ftell(fp) )
				if( !openArc( m_nextVol++, "cab" ) )
					break;
			fseek( fp, (int)m_FileList[i].ofsData, SEEK_SET );
			continue;
		}
		char* outpath = kiutil::pathMake( m_FileList[i].name );
		wp=fopen( outpath, "wb" );

		m_sizeLimit = (int)m_FileList[i].cbCompressed;
		if( (unsigned int)m_FileList[i].ofsData < (unsigned int)ftell(fp) )
			if( !openArc( m_nextVol++, "cab" ) )
				break;
		fseek( fp, (int)m_FileList[i].ofsData, SEEK_SET );

		if( m_FileList[i].cmprs )
			decompress();
		else
			directCopy( m_FileList[i].crypt );

		if( wp )
			fclose( wp );
		kiutil::timeSet( outpath, m_FileList[i].date, m_FileList[i].time );
		SetFileAttributes( outpath, m_FileList[i].attr );
	}

	delete [] m_FileList;
	closeArc();
	return true;
}

void CIsTool::directCopy( bool crypt )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	int rd;
	while( 0 >= (rd=readArc( buf, 262144 )) )
	{
		if( crypt ) // 復号
		{
			int cS=rd;
			unsigned char* cP=buf;
			int key=0;
			unsigned char ts;

			for( ; cS>0; cS--,cP++,key++ )
			{
				ts = (*cP ^ 0xd5);
				__asm { ror byte ptr ts, 2 };
				*cP = ts - (unsigned char)(key%0x47);
			}
		}
		if( wp )
			fwrite( buf, 1, rd, wp );
	}
}

void CIsTool::decompress()
{
	ZDataStart( 0 );
	ZDataDecompress( 0 );
	ZDataEnd();
}

bool CIsTool::readInit()
{
	m_nextVol = 2;
	return openArc( 1, "cab" );
}

int CIsTool::readArc( void* buf, int siz )
{
	// 圧縮後サイズ分に既に読み出している
	if( m_sizeLimit<=0 )
		return 0;

	// 書庫が開けなかったらエラー
	if( fp==NULL )
	{
		iscab_header hdr;
		if( !openArc( m_nextVol++, "cab" )
		 ||   sizeof(hdr) != fread( &hdr, 1, sizeof(hdr), fp )
		 || hdr.Signature != 0x28635349
		 || !(hdr.Version & 0x01000000) )
			return -1;
		fseek( fp, (int)hdr.ofsCompData, SEEK_SET );
	}

	// リミットを越えないように、出来るだけ読んでみる
	int gRd = (m_sizeLimit>siz ? siz : m_sizeLimit);
	int rRd = fread( buf, 1, gRd, fp );

	// エラー
	if( rRd<0 )
		return -1;

	// 全部読めた場合
	m_sizeLimit -= rRd;
	if( rRd == gRd )
		return rRd;

	// EOFが来たっぽいので次のボリュームへ
	closeArc();

	// 足りない分を読んでみる
	int nRd = readArc( ((char*)buf)+rRd, siz-rRd );

	// エラー
	if( nRd<0 )
		return -1;

	// 読めた分だけ返す
	return rRd + nRd;
}
