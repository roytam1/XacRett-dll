#include "stdafx.h"
#include "OpenIs.h"

#pragma pack(1)

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

bool COpenIs::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	i=0;
	setupNamePattern(szArcFileName);
	return ( readHeader() && FindNext(lpSubInfo) );
}

bool COpenIs::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	if(i==m_FileNum)
	{
		if(m_FileList) delete[] m_FileList;
		return false;
	}

	lstrcpy(lpSubInfo->szFileName,m_FileList[i].name);
	lpSubInfo->dwCompressedSize=m_FileList[i].cbCompressed;
	lpSubInfo->dwOriginalSize=m_FileList[i].cbExpanded;
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	lpSubInfo->wDate=m_FileList[i].date;
	lpSubInfo->wTime=m_FileList[i].time;

	lstrcpy(lpSubInfo->szAttribute ,"----");
	if (m_FileList[i].attr & FILE_ATTRIBUTE_READONLY)
		lpSubInfo->szAttribute[2] = 'R';
	if (m_FileList[i].attr & FILE_ATTRIBUTE_HIDDEN)
		lpSubInfo->szAttribute[1] = 'H';
	if (m_FileList[i].attr & FILE_ATTRIBUTE_SYSTEM)
		lpSubInfo->szAttribute[0] = 'S';
	if (m_FileList[i].attr & FILE_ATTRIBUTE_DIRECTORY)
		lpSubInfo->szAttribute[3] = 'D';
	if (m_FileList[i].attr & FILE_ATTRIBUTE_ARCHIVE)
		lpSubInfo->szAttribute[3] = 'A';

	i++;
	return true;
}




void COpenIs::setupNamePattern( const char* fname )
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
bool COpenIs::readHeader()
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
							if(m_FileList) delete[] m_FileList;
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
bool COpenIs::openArc( int vol, const char* ext )
{
	closeArc();

	char tmp[MAX_PATH*3];
	sprintf( tmp, "%s%d.%s", m_arcNamePat, vol, ext );
	return (NULL != (fp=fopen( tmp, "rb" )));
}

void COpenIs::closeArc()
{
	if( fp )
		fclose( fp ), fp=NULL;
}