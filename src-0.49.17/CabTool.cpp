
#include "stdafx.h"
#include "CabTool.h"
#include "fdi/fdi.h"
// fdi.cpp is compiled straight into XacRett.dll (see XacRett.dsp), so the
// prebuilt fdi/fdi.lib is neither shipped nor needed. The original pragma is
// dropped so the linker does not try to pull the binary in.
//#pragma comment( lib, "fdi/fdi.lib" )
#include "XacRett.h"

// �����SFX����
static int seeker=0;
static int offhan=-1;
static int offhan2=-1;

// CallBack ��`
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
		if (((CCabTool*)pfdin->pv)->m_Endflg) return -1;//�ǉ� By Bonty
		//���C���h�J�[�h����
		if (!((CCabTool*)pfdin->pv)->CheckStr(pfdin->psz1))//�ǉ� By Bonty
		{
			return 0;
		}
		//SetOwnerWindow�̂��߂̏���
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
	// FDI������
	FDICABINETINFO info;
	ERF erf;
	HFDI fdi=FDICreate( mymalloc,mymfree,myfopen,myfread,
					myfwrite,myfclose,myfseek,cpuUNKNOWN,&erf );
	if( fdi==NULL )
		return false;

	// �J��
	int ans=-1, han=myfopen( const_cast<char*>(fname),
					_O_BINARY|_O_RDONLY|_O_SEQUENTIAL,0 );

	// �w�b�_����
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
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	seeker = 0;
	return (-1!=FindHeader(fname,hdr,siz>150*1024?150*1024:siz));
}

bool CCabTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	// �w�b�_��T��
	FILE* fp = fopen( fname,"rb" );
	if( !fp )
		return false;
	DWORD siz = fread( buf, 1, XACR_BUFSIZE, fp );
	fclose( fp );

	seeker = 0;
	seeker = FindHeader( fname,buf,siz );
	if( seeker==-1 )
		return false;

	// FDI������
	ERF erf;
	HFDI fdi=FDICreate( mymalloc,mymfree,myfopen,myfread,
					myfwrite,myfclose,myfseek,cpuUNKNOWN,&erf );
	if( fdi==NULL )
		return false;

	// FDI�ɓn�����߂Ƀt�@�C��������
	int y,d;
	kiutil::pathSplit( fname,&y,&d );

	char fbody[MAX_PATH], fdir[MAX_PATH]={0};
	strcpy( fbody,&fname[y+1] );
	if( y!=-1 )
		strncpy( fdir,fname,y+1 );

	// �W�J
	
	BOOL ans=FDICopy( fdi,fbody,fdir,0,mynotif,NULL,this);

	// �I��
	FDIDestroy(fdi);
	return ans!=FALSE;
}
