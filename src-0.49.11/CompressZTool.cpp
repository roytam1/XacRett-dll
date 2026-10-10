
#include "stdafx.h"
#include "CompressZTool.h"
#include "complib.h"

bool CCompressZTool::Check(  const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return ( siz>2 && hdr[0]==0x1f && hdr[1]==0x9d ); 
}

bool CCompressZTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	ZFILE* fp=Z_open( fname,"rb" );
	if( fp==NULL )return false;

	char outname[256];
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"Z") ? NULL : "uncompress" );

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(outname)){Z_close(fp);return true;}//追加 By Bonty

	FILE* fwp=fopen(outname,"wb");
	if( fwp!=NULL )
	{
		size_t s;
		while( s=Z_read(buf,XACR_BUFSIZE,fp) )
		{
			if( s==-1 )
				break;
			fwrite( buf,1,s,fwp );
		}
		fclose(fwp);
	}
	Z_close( fp );
	return true;
}
