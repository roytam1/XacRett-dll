
#include "stdafx.h"
#include "Bz2Tool.h"
#include "libbzip2/bzlib.h"

bool CBz2Tool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<3 || hdr[0]!='B' || hdr[1]!='Z' || hdr[2]!='h' )
		return false;

	BZFILE* fp=BZ2_bzopen( fname,"rb" );
	if( fp==NULL )
		return false;
	BYTE dummy[20];
	bool ans=(-1!=BZ2_bzread(fp,dummy,20));
	BZ2_bzclose( fp );
	return ans;
}

bool CBz2Tool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	BZFILE* fp=BZ2_bzopen( fname,"rb" );
	if( fp==NULL )return false;

	char outname[256];
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"bz2") ? NULL : "unbzip2" );

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(outname))//追加 By Bonty
	{
		BZ2_bzclose(fp);
		return true;
	}

	FILE* fwp=fopen( outname,"wb" );
	if( fwp!=NULL )
	{
		size_t s;
		while( s=BZ2_bzread( fp,buf,XACR_BUFSIZE ) )
		{
			if( s==-1 )
				break;
			fwrite( buf,1,s,fwp );
		}
		fclose(fwp);
	}
	BZ2_bzclose( fp );
	return true;
}
