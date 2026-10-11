
#include "stdafx.h"
#include "GcaTool.h"
#include "gcasdk/GcaSDK.h"
#include "XacRett.h"

using namespace std;
using namespace nsGcaSDK;

static int dummy_msg_proc( int msg,string,void* ptr )
{
	switch( msg )
	{
	case GCAM_FILE_CRC_ERROR:
	case GCAM_ARC_CRC_ERROR:
	case GCAM_ARC_UNKNOWN_FORMAT:
	case GCAM_ARC_WRITE_ERROR:
	case GCAM_FILE_WRITE_ERROR:
	case GCAM_ARC_CRYPTO_ERROR:
	case GCAM_FILE_CRYPTO_ERROR:
		return GCAM_CANCEL;

	case GCAM_FILE_CRYPTO:
	case GCAM_ARC_CRYPTO:{
		CGca* gca=(CGca*)ptr;
		CGcaTool* tool = (CGcaTool*)gca->GetUser();
		if( tool )
		{
			string s;
			const char* x = tool->pwd();
			if( *x=='#' )
				if( x[1]=='#' )
					x++;
				else
				{
					GetSpellFromKeyFile( s, string(x+1) );
					x = s.c_str();
				}
			gca->SetSpell( string(x) );
			gca->SetCrypto(TRUE);
			gca->SetUser( 0 );
		}}break;
	}
	return GCAM_OK;
}

bool CGcaTool::Check( const char* fname, unsigned long fsize )
{
	CGca gca;
	gca.SetArcFilePath(fname);

	if(gca.GetNumFiles())
		return true;
	return false;
/*	FILE* fp = fopen(fname,"rb");
	if( !fp )
		return false;
	int x;
	fseek( fp, x=GetStartOffset( string(fname) ), SEEK_SET );
	bool ans=false;

	if( x!=0 || (fgetc(fp)=='G' && fgetc(fp)=='C' && fgetc(fp)=='A') )
		ans = true;

	fclose( fp );
	return ans;*/
}

bool CGcaTool::Extract( const char* fname,const char* ddir )
{
	CGca gca;
	if( !gca.Init() )
		return false;

	string nm = fname, dst = ddir;
	gca.SetStartOffset( GetStartOffset( nm ) );
	gca.SetArcFilePath( nm );
	gca.SetOutputPath( dst );
	gca.SetMessageProc( dummy_msg_proc );
	gca.SetUser( (DWORD)this );

	/*if(strcmp(wildcard[1],"")==0)
	{
		if( !gca.Extract() )
		{
			gca.Free();
			return false;
		}
	}
	else*/
	{
		int filenum =gca.GetNumFiles();
		string filename;
		for (int i = -1 ; ++i!=filenum;)
		{
			filename = gca.GetFileName(i);
			if (m_Endflg) return true;//追加 By Bonty
			//ワイルドカード処理
			if (!CheckStr(filename.c_str()))//追加 By Bonty
			{
				continue;
			}
			if(arcproc)//SetOwnerWindowのための処理
			{
				static EXTRACTINGINFO	info;
				::memset(&info,0,sizeof(EXTRACTINGINFO));
				lstrcpy(info.szSourceFileName,filename.c_str());
				if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
					continue;
			}
			gca.ExtractFile(i);
		}
	}
	gca.Free();
	return true;
}
