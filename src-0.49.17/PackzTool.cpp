
#include "stdafx.h"
#include "PackzTool.h"
#include "complib.h"

bool CPackzTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return ( siz>2 && hdr[0]==0x1f && hdr[1]==0x1e ); 
}

bool CPackzTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	PFILE* fp=P_open( fname,"rb" );
	if( fp==NULL )return false;

	char outname[256];
	kiutil::getOriginalName( outname,fname,
		0==stricmp(kiutil::pathExt(fname),"z") ? NULL : "unpack" );
	
	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(outname))//追加 By Bonty
	{
		P_close( fp );
		return true;
	}
	kiutil::pathMake83(outname);
	FILE* fwp=fopen(outname,"wb");
	if( fwp!=NULL )
	{
		// The header carries the original length (PFILE::orig_len,
		// read big-endian by read_tree).  Bounding the loop by it is
		// exact for a valid stream, and it is the only thing that
		// stops a corrupt one: P_read keeps handing back dictionary
		// expansions forever because the end-of-block symbol never
		// decodes (m_eob is only reached on a well-formed tree), so an
		// unbounded loop writes until the disk fills -- measured at
		// ~6 MB/s.  Official 0.49.17 has this defect too; this is a
		// deliberate divergence, cf. the ARC and YZ1 notes.
		DWORD cap = fp->orig_len;
		DWORD total = 0;
		while( total<cap )
		{
			DWORD want = cap-total;
			if( want>(DWORD)XACR_BUFSIZE )
				want = (DWORD)XACR_BUFSIZE;
			int s = P_read( buf,(int)want,fp );
			if( s<=0 )
				break;
			fwrite( buf,1,(size_t)s,fwp );
			total += (DWORD)s;
		}
		fclose(fwp);
		if( total!=cap )
			remove( outname );  // never leave a truncated file behind
	}
	P_close( fp );
	return true;
}
