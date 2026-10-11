
#include "stdafx.h"
#include "QPakTool.h"

bool CQPakTool::Open( const char* fname )
{
// なんか気分的に面倒だったので
// このルーチンは Byte Order Dependent です！！！！

	pak = fopen( fname,"rb" );
	if( !pak ) return false;

	char dummy[8];
	DWORD sign;
	if( 1!=fread( &sign,4,1,pak ) )
	{Close();return false;}

	switch( sign )
	{
	// Quake1, Quake2, Half-Life, SiN( PACK/SPAK )
	case 0x4b434150:
	case 0x4b415053:
		{
			DWORD offset,contents;
			if( 1!=fread( &offset,4,1,pak )
			 || 1!=fread( &contents,4,1,pak ) )
			{Close();return false;}

			fseek( pak,0,SEEK_END );
			if( (unsigned)ftell(pak) < offset+contents )
			{Close();return false;}

			count = contents / ((sign==0x4b434150?0x38:0x78)+8);

			fseek( pak,offset,SEEK_SET );
			entry = new PakIndex[count];
			for( DWORD i=0; i!=count; i++ )
			{
				fread( entry[i].fname, 1,
					sign==0x4b434150 ? 0x38 : 0x78, pak );
				fread( &entry[i].offset, 4, 1, pak );
				fread( &entry[i].size, 4, 1, pak );
			}
		}break;

	// Quake, Half-life( WAD2/WAD3 )
	case 0x32444157:
	case 0x33444157:
		{
			DWORD offset;
			if( 1!=fread( &count,4,1,pak )
			 || 1!=fread( &offset,4,1,pak ) )
			{Close();return false;}

			fseek( pak,0,SEEK_END );
			if( (unsigned)ftell(pak) < offset+count*0x20 )
			{Close();return false;}

			fseek( pak,offset,SEEK_SET );
			entry = new PakIndex[count];
			for( DWORD i=0; i!=count; i++ )
			{
				fread( &entry[i].offset, 4, 1, pak );
				fread( &entry[i].size, 4, 1, pak );
				fread( dummy, 1, 8, pak );
				fread( entry[i].fname, 1, 0x10, pak );
			}
		}break;

	// Doom, Half-Life( IWAD/PWAD )
	case 0x44415749:
	case 0x44415750:
		{
			DWORD offset;
			if( 1!=fread( &count,4,1,pak )
			 || 1!=fread( &offset,4,1,pak ) )
			{Close();return false;}

			fseek( pak,0,SEEK_END );
			if( (unsigned)ftell(pak) < offset+count*0x10 )
			{Close();return false;}

			fseek( pak,offset,SEEK_SET );
			entry = new PakIndex[count];
			for( DWORD i=0; i!=count; i++ )
			{
				fread( &entry[i].offset, 4, 1, pak );
				fread( &entry[i].size, 4, 1, pak );
				fread( entry[i].fname, 1, 0x08, pak );
			}
		}break;

	// ELSE
	default:
		if( 1!=fread( &sign,4,1,pak ) )
		{Close();return false;}
		// Delta Force ( PFF )
		if( (sign&0xffffff)!=0x464650 )
		{Close();return false;}
		{
			DWORD recsize,offset;
			if( 1!=fread( &count,4,1,pak )
			 || 1!=fread( &recsize,4,1,pak )
			 || 1!=fread( &offset,4,1,pak ) )
			{Close();return false;}

			fseek( pak,0,SEEK_END );
			if( (unsigned)ftell(pak) < offset+count*recsize )
			{Close();return false;}

			fseek( pak,offset,SEEK_SET );
			entry = new PakIndex[count];
			for( DWORD i=0; i!=count; i++ )
			{
				fread( dummy, 1, 4, pak );
				fread( &entry[i].offset, 4, 1, pak );
				fread( &entry[i].size, 4, 1, pak );
				fread( dummy, 1, 4, pak );// date
				fread( entry[i].fname, 1, 0x10, pak );
			}
		}break;
	}
	return true;
}

void CQPakTool::Close()
{
	if( pak!=NULL )
	{fclose(pak);pak=NULL;}
	if( entry!=NULL )
	{delete [] entry;entry=NULL;}
}

bool CQPakTool::Check( const char* fname, unsigned long fsize )
{
	const DWORD* head = (const DWORD*)CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<20 )
		return false;

	// 形式ごとに分岐
	switch( head[0] )
	{
	case 0x4b434150:
	case 0x4b415053:
		if( fsize < head[1]+head[2] )
			return false;
		break;
	case 0x32444157:
	case 0x33444157:
		if( fsize < head[2]+head[1]*0x20 )
			return false;
		break;
	case 0x44415749:
	case 0x44415750:
		if( fsize < head[2]+head[1]*0x10 )
			return false;
		break;
	default:
		if( (head[1]&0xffffff)!=0x464650 )
			return false;
		if( fsize < head[4]+head[3]*head[2] )
			return false;
		break;
	}
	return true;
}

bool CQPakTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	if( !Open(fname) )
		return false;

	for( DWORD i=0; i!=count; i++ )
	{
		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(entry[i].fname))//追加 By Bonty
		{
			continue;
		}
		// 開く
		char* outname = kiutil::pathMake( entry[i].fname );
		FILE* out=fopen(outname,"wb");
		if( !out ) continue;

		fseek( pak,entry[i].offset,SEEK_SET );

		// 出力
		unsigned long fsize=entry[i].size;
		while( fsize && !feof(pak) )
		{
			if( fsize<=XACR_BUFSIZE )
			{
				fwrite( buf,1,fread(buf,1,fsize,pak),out );
				fsize=0;
			}
			else
			{
				fwrite( buf,1,fread(buf,1,XACR_BUFSIZE,pak),out );
				fsize-=XACR_BUFSIZE;
			}
		}
		fclose(out);
	}

	Close();
	return true;
}
