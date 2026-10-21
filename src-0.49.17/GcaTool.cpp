
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
		{
			CGca* gca=(CGca*)ptr;
			CGcaTool* tool = gca ? (CGcaTool*)gca->GetUser() : 0;
			if( tool ) tool->m_cryptoError = true;
		}
		return GCAM_CANCEL;

	case GCAM_FILE_CRYPTO:
	case GCAM_ARC_CRYPTO:{
		CGca* gca=(CGca*)ptr;
		CGcaTool* tool = (CGcaTool*)gca->GetUser();
		if( tool )
		{
			string s = GcaSpellFromArg( tool->pwd() );
			gca->SetSpell( s );
			gca->SetCrypto(TRUE);
			gca->SetUser( 0 );
		}}break;
	}
	return GCAM_OK;
}

std::string GcaSpellFromArg( const char* arg )
{
	string s;
	if( !arg || !*arg ) return s;
	if( *arg=='#' )
	{
		if( arg[1]=='#' ) return string(arg+1);
		if( GetSpellFromKeyFile( s, string(arg+1) ) ) return s;
		return string();
	}
	return string(arg);
}

bool CGcaTool::Check( const char* fname, unsigned long fsize )
{
	CGca gca;
	gca.SetArcFilePath(fname);

	// a password that was handed in on the command line is used
	// straight away, so that an encrypted archive is recognised and
	// can be listed.  The dialog is not opened here: the check runs
	// while the archive is being routed.
	gca.SetSpell( GcaSpellFromArg( CTool::m_pPassArg ) );
	gca.SetCrypto( TRUE );

	if(gca.GetNumFiles())
		return true;

	// an encrypted archive without a password has no file list at
	// all, so the format is recognised by its signature instead
	{
		FILE* fp = fopen( fname, "rb" );
		unsigned char hdr[3];
		bool ans = false;
		if( !fp ) return false;
		if( 3==fread( hdr,1,3,fp ) &&
			hdr[0]=='G' && hdr[1]=='C' && hdr[2]=='A' ) ans = true;
		fclose( fp );
		return ans;
	}
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

	m_cryptoError = false;

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
		// the first read is without a password: a plain archive has to
		// work without ever opening the password dialog
		int filenum =gca.GetNumFiles();
		string filename;

		if( !filenum )
		{
			// an encrypted archive cannot be read before the password
			// is known, and the SDK does not ask for it: it reports
			// that the archive needs a key and stops.  So the password
			// is taken here, but only now, when the archive has shown
			// that it needs one.  The one from the command line is used
			// first (it never opens the dialog), and only when that is
			// missing or wrong is the user asked -- the same order the
			// YZ1 provider uses.
			string spell = GcaSpellFromArg( CTool::m_pPassArg );
			if( !spell.empty() )
			{
				gca.SetSpell( spell );
				gca.SetCrypto( TRUE );
				filenum = gca.GetNumFiles();
			}
			if( !filenum )
			{
				spell = GcaSpellFromArg( pwd() );
				if( !spell.empty() )
				{
					gca.SetSpell( spell );
					gca.SetCrypto( TRUE );
					filenum = gca.GetNumFiles();
				}
			}
		}

		if( !filenum && m_cryptoError )
		{
			// the archive is encrypted and the password did not open it
			gca.Free();
			return false;
		}
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
