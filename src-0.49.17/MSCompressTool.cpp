
#include "stdafx.h"
#include "MSCompressTool.h"
#include "MsCompress.h"
#include "XacRett.h"

bool CMSCompressTool::Check( const char* fname, unsigned long fsize )
{
	// the signature is what decides: the plain SZDD, the QBasic
	// variant of it and the KWAJ format all start with their own
	// eight bytes, and the compression method is only known after
	// that, so it must not be part of the test
	FILE* fp;
	MsCompressHeader hdr;
	int ret;

	if( fsize<12 )
		return false;

	fp = fopen( fname, "rb" );
	if( !fp )
		return false;
	ret = MsCompressReadHeader( fp, &hdr );
	fclose( fp );
	if( ret!=MSCOMPRESS_OK )
		return false;

// version 6 header。未確認(^^;
// SZDD芋'3A[char ext_3rd][DWORD original_size]
// ↓原理的に、１６倍という圧縮率は出ない。
	// the file has to be big enough to hold the unpacked data
	if( hdr.origsize && (fsize<<4) < hdr.origsize )
		return false;

	return true;
}

bool CMSCompressTool::Extract( const char* fname, const char* ddir )
{
	// ヘッダ読み込み
	FILE* msc = fopen( fname,"rb" );
	MsCompressHeader hdr;
	if( !msc )
		return false;
	// the header knows the format, the method, the length of the
	// unpacked file and the character that COMPRESS.EXE replaced
	if( MSCOMPRESS_OK!=MsCompressReadHeader( msc, &hdr ) )
	{
		fclose( msc );
		return false;
	}

		// ファイル名復元までの長い道のり(笑)
		// 最後が '_' なら消しておく。
		// ver 5 ヘッダなら３文字目を付加
		// 拡張子自動補完
	// the name of the unpacked file, worked out the same way as the
	// listing does it, so that the two can never disagree
	char oname[MAX_PATH];
	if( MsCompressMakeName( fname, hdr.missingchar, oname, (int)sizeof(oname) )<=0 )
	{
		fclose( msc );
		return false;
	}

	if (m_Endflg) { fclose( msc ); return true; }//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(oname))//追加 By Bonty
	{
		fclose( msc );
		return true;
	}	
	if(arcproc)//SetOwnerWindowのための処理
	{
		static EXTRACTINGINFO	info;
		::memset(&info,0,sizeof(EXTRACTINGINFO));
		lstrcpy(info.szSourceFileName,oname);
		if(arcproc(hWnd,Msg,ARCEXTRACT_WRITEFILE_OPEN,(WPARAM)&info))
		{
			fclose( msc );
			return true;
		}
	}
	// 解凍
	// the COMPRESS.EXE formats are unpacked by our own decoder,
	// so that no system DLL is needed: lz32.dll only implements the
	// LZ/SZDD variant, is not present on the older Windows targets
	// and copies unknown input through unchanged.
	{
		FILE* op = fopen( oname, "wb" );
		int ret = MSCOMPRESS_ERR_WRITE;
		if( op )
		{
			// the file is still positioned where the header left it
			ret = MsCompressDecode( msc, op, &hdr );
			fclose( op );
		}
		fclose( msc );
		if( ret!=MSCOMPRESS_OK )
		{
			DeleteFile( oname );
			return false;
		}
	}

	//日付を書庫に合わせる
	WIN32_FIND_DATA find_f;
	HANDLE h = FindFirstFile( fname,&find_f );
	if( h==INVALID_HANDLE_VALUE )
		return false;
	FindClose( h );
	kiutil::timeSet(oname,&find_f.ftLastWriteTime);

	return true;
}

