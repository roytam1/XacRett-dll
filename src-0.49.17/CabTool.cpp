
#include "stdafx.h"
#include "CabTool.h"
#include "fdi/fdi.h"
#include "CabDecode.h"
// fdi.cpp is compiled straight into XacRett.dll (see XacRett.dsp), so the
// prebuilt fdi/fdi.lib is neither shipped nor needed. The original pragma is
// dropped so the linker does not try to pull the binary in.
//#pragma comment( lib, "fdi/fdi.lib" )
#include "XacRett.h"

// 無理矢理SFXも解凍
static int seeker=0;
static int offhan=-1;
static int offhan2=-1;

// CallBack 定義
static FNALLOC(mymalloc)
{
	return (void*)new BYTE[cb];
}
static FNFREE(mymfree)
{
	delete [] (BYTE*)pv;
}
static FNOPEN(myfopen)
{
	int han=_open( pszFile, oflag, pmode );
	if( han!=-1 && seeker!=0 && !(oflag&_O_WRONLY) )
	{
		_lseek( han,seeker,SEEK_SET );
		if( offhan==-1 )offhan=han;
		else if( offhan2==-1 )offhan2=han;
	}
	return han;
}
static FNREAD(myfread)
{
	return _read( hf, pv, cb );
}
static FNWRITE(myfwrite)
{
	return _write( hf, pv, cb );
}
static FNCLOSE(myfclose)
{
	if( hf==offhan )offhan=-1;
	else if( hf==offhan2 )offhan2=-1;
	return _close(hf);
}
static FNSEEK(myfseek)
{
	if( hf==offhan || hf==offhan2 )
	{
		if( seektype==SEEK_SET )
			return _lseek( hf, dist+seeker, SEEK_SET ) - seeker;
		return _lseek( hf, dist, seektype ) - seeker;
	}
	return _lseek( hf, dist, seektype );
}
static FNFDINOTIFY(mynotif)
{
	switch( fdint )
	{
	case fdintCOPY_FILE:
		if (((CCabTool*)pfdin->pv)->m_Endflg) return -1;//追加 By Bonty
		//ワイルドカード処理
		if (!((CCabTool*)pfdin->pv)->CheckStr(pfdin->psz1))//追加 By Bonty
		{
			return 0;
		}
		//SetOwnerWindowのための処理
		if(((CCabTool*)pfdin->pv)->arcproc)
		{
			static EXTRACTINGINFO	info;
			::memset(&info,0,sizeof(EXTRACTINGINFO));
			lstrcpy(info.szSourceFileName,pfdin->psz1);
			if(((CCabTool*)pfdin->pv)->arcproc(((CCabTool*)pfdin->pv)->hWnd,((CCabTool*)pfdin->pv)->Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
				return 0;
		}
		return myfopen( kiutil::pathMake(pfdin->psz1),
			_O_BINARY|_O_CREAT|_O_TRUNC|_O_WRONLY|_O_SEQUENTIAL,
			_S_IREAD|_S_IWRITE );
		break;
	case fdintCLOSE_FILE_INFO:
		myfclose( pfdin->hf );
		kiutil::timeSet( pfdin->psz1, pfdin->date, pfdin->time );
		SetFileAttributes( pfdin->psz1,
			pfdin->attribs&(_A_RDONLY| _A_HIDDEN|_A_SYSTEM|_A_ARCH) );
		return TRUE;
		break;

	case fdintNEXT_CABINET:{
		char str[256];
		wsprintf( str,"%s%s",pfdin->psz3,pfdin->psz1 );
		if( GetFileAttributes( str ) & FILE_ATTRIBUTE_DIRECTORY )
			return -1;
		seeker = 0;
		break;}
	}
	return 0;
}

int CCabTool::FindHeader( const char* fname, const BYTE* hdr, DWORD siz )
{
	// FDI初期化
	FDICABINETINFO info;
	ERF erf;
	HFDI fdi=FDICreate( mymalloc,mymfree,myfopen,myfread,
					myfwrite,myfclose,myfseek,cpuUNKNOWN,&erf );
	if( fdi==NULL )
		return false;

	// 開く
	int ans=-1, han=myfopen( const_cast<char*>(fname),
					_O_BINARY|_O_RDONLY|_O_SEQUENTIAL,0 );

	// ヘッダ検索
	if( -1!=han )
	{
		if( siz>10 && hdr[0]=='M' && hdr[1]=='Z' )
		{
			for( DWORD i=2; i<siz; i++ )
				if( hdr[i+0]=='M' && hdr[i+1]=='S' && hdr[i+2]=='C' &&
					hdr[i+3]=='F' && hdr[i+4]== 0  && hdr[i+5]== 0  &&
					hdr[i+6]== 0  && hdr[i+7]==0 )
				{
					myfseek( han,i,SEEK_SET );
					if( FDIIsCabinet(fdi,han,&info) )
					{
						ans=i;
						break;
					}
				}
		}
		else
		{
			if( FDIIsCabinet( fdi,han,&info ) )
				ans = 0;
		}
		myfclose(han);
	}

	FDIDestroy( fdi );
	return ans;
}

bool CCabTool::Check( const char* fname, unsigned long fsize )
{
//--------------------------------------------------------------------//

	// the signature and the header are what decide, and our own
	// reader does that without any system DLL
	FILE* fp = fopen( fname, "rb" );
	CabArchive cab;
	long base;
	bool ok = false;

	if( !fp )
		return false;
	if( fsize==0 ) {
		fseek( fp, 0, SEEK_END );
		fsize = (unsigned long)ftell(fp);
	}
	base = CabFindHeader( fp, fsize );
	if( base>=0 && CabReadHeader( fp, base, &cab ) ) {
		ok = true;
		CabFreeHeader( &cab );
	}
	fclose( fp );

	seeker = 0;
	return ok;
}

// The files of a cabinet are unpacked by our own decoder, so that no
// system DLL is needed.  The name of the file is worked out here, the
// same way the old code did it with the system decoder.
static bool cabWriteFile( void *user, const CabFileEntry *entry,
						  char *path, int pathSize )
{
	CCabTool* tool = (CCabTool*)user;
	char* name;
	int n = lstrlen( entry->name );

	if( n<1 || n>=pathSize ) return false;
	lstrcpy( path, entry->name );

	if( tool->m_Endflg ) return false;
	if( !tool->CheckStr(path) ) return false;
	if( tool->arcproc )
	{
		static EXTRACTINGINFO info;
		::memset(&info,0,sizeof(EXTRACTINGINFO));
		lstrcpy(info.szSourceFileName,path);
		if( tool->arcproc(tool->hWnd,tool->Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info) )
			return false;
	}
	// the directories of the name are made, and a name that is too
	// long for the file system is shortened
	name = kiutil::pathMake( path );
	if( !name ) return false;
	if( name!=path ) lstrcpy( path, name );
	return true;
}

bool CCabTool::Extract( const char* fname,const char* ddir )
{
	FILE* fp = fopen( fname,"rb" );
	CabArchive cab;
	long base;
	bool ok = false;

//--------------------------------------------------------------------//

	if( !fp )
		return false;
	fseek( fp, 0, SEEK_END );
	base = CabFindHeader( fp, (unsigned long)ftell(fp) );
	if( base>=0 && CabReadHeader( fp, base, &cab ) )
	{
		if( CabCanDecode( &cab ) )
			ok = CabExtractAll( fp, &cab, this, cabWriteFile );
		CabFreeHeader( &cab );
	}
	fclose( fp );
	if( ok )
		return true;

	// the Quantum and the LZX methods are not decoded here, so a
	// cabinet that uses one of them is left to the system, when the
	// target has one at all
	// ヘッダを探す
	unsigned char* buf = CTool::common_buf;
	unsigned long siz;

	// the system decoder wants the head of the file in one piece
	{
		FILE* fp2 = fopen( fname,"rb" );
		if( !fp2 ) return false;
		siz = fread( buf, 1, XACR_BUFSIZE, fp2 );
		fclose( fp2 );
	}
	siz = (siz>150*1024 ? 150*1024 : siz);

	seeker = 0;
	seeker = FindHeader( fname,buf,siz );
	if( seeker==-1 )
		return false;

	// FDI初期化
	ERF erf;
	HFDI fdi=FDICreate( mymalloc,mymfree,myfopen,myfread,
					myfwrite,myfclose,myfseek,cpuUNKNOWN,&erf );
	if( fdi==NULL )
		return false;

	// FDIに渡すためにファイル名分割
	int y,d;
	kiutil::pathSplit( fname,&y,&d );

	char fbody[MAX_PATH], fdir[MAX_PATH]={0};
	strcpy( fbody,&fname[y+1] );
	if( y!=-1 )
		strncpy( fdir,fname,y+1 );

	// 展開
	
	BOOL ans=FDICopy( fdi,fbody,fdir,0,mynotif,NULL,this);

	// 終了
	FDIDestroy(fdi);
	return ans!=FALSE;
}
