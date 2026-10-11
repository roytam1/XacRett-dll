
#include "stdafx.h"
#include "ArgTool.h"

bool CArgTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<26 )
		return false;

	// "最初のエントリのサイズ＜arg書庫全体のサイズ" なはず
	unsigned long entsiz=
		(hdr[22])+(hdr[23]<<8)+(hdr[24]<<16)+(hdr[25]<<24);
	if( hdr[0]!=0 || fsize<entsiz+26 )
		return false;
	if(hdr[1]==0)
		return false;

	// 次のエントリのスタートは 0 (ARG_START) か
	// 1 (ARG_END) なはず。
	int c=2;
	if( entsiz+26 < siz )
		c = hdr[entsiz+26];
	else
	{
		FILE* fp = fopen( fname,"rb" );
		fseek( fp, entsiz+26, SEEK_SET );
		c = fgetc( fp );
		fclose( fp );
	}

	return (c==(c&1));
}

// 展開用構造体
struct TREE_ELEM
{
	bool bStop;
	unsigned char data;
	struct TREE_ELEM *pChild[2];
};
struct FLOOR_ELEM
{
	BYTE byChar;
	int	 iFloor;
};
// ツリー構成
static void NextFloor( int floor,TREE_ELEM *p,const FLOOR_ELEM*& ptrFl )
{
	if( ptrFl->iFloor!=floor )
	{
		p->bStop=false; // 下へ参りま～す(爆)
		floor++;
		NextFloor( floor,p->pChild[0]=new TREE_ELEM,ptrFl );
		NextFloor( floor,p->pChild[1]=new TREE_ELEM,ptrFl );
	}
	else
	{
		p->bStop=true; // 終点～
		p->data =(ptrFl++)->byChar;
	}
}
// ソート用
static int CompFloorElem(const void* f1,const void* f2)
{
	if(((const FLOOR_ELEM*)f1)->iFloor<((const FLOOR_ELEM*)f2)->iFloor)
		return -1;
	if(((const FLOOR_ELEM*)f1)->iFloor>((const FLOOR_ELEM*)f2)->iFloor)
		return  1;
	if(((const FLOOR_ELEM*)f1)->byChar<((const FLOOR_ELEM*)f2)->byChar)
		return -1;
	return (((FLOOR_ELEM*)f1)->byChar>((FLOOR_ELEM*)f2)->byChar);
}
// 再帰でツリーを解放
static void DeleteTree( TREE_ELEM *p )
{
	if( !p->bStop )
		DeleteTree( p->pChild[0] ),DeleteTree( p->pChild[1] );
	delete p;
}

bool CArgTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	FILE* arg = fopen( fname,"rb" );
	if( !arg )
		return false;

	int c;

	while( fgetc(arg)==0 ) // ARG_START
	{
		// ヘッダ読み込み
		// ARG_START
		// name[13byte]
		// sec(5)min(6)hour(5)day(5)month(4)year(7) [4byte]
		// orglen[4byte]
		// cmplen[4byte]
		if( 25!=fread( buf,1,25,arg ) )
			break;
		char outname[MAX_PATH];
		buf[12]='\0', strcpy( outname,(char*)buf );

		WORD  time   = (buf[13])+(buf[14]<<8);
		WORD  date   = (buf[15])+(buf[16]<<8);
		DWORD orglen = (buf[17])+(buf[18]<<8)+(buf[19]<<16)+(buf[20]<<24);
		DWORD cmplen = (buf[21])+(buf[22]<<8)+(buf[23]<<16)+(buf[24]<<24);

		if (m_Endflg) return true;//追加 By Bonty
		if (!CheckStr(outname))//追加 By Bonty
		{
			fseek( arg,cmplen,SEEK_CUR );
			continue;
		}

		// 解凍
		FILE* out=fopen( kiutil::pathMake(outname),"wb" );
		if( !out )
		{
			fseek( arg,cmplen,SEEK_CUR );
			continue;
		}

		if( orglen!=0 )
		{
			if( orglen==cmplen )
			{
				while( (orglen--) && (-1!=(c=fgetc(arg))) )
					fputc( c,out );
			}
			else
			{
				int iStatus = fgetc(arg);
				if( iStatus!=-1 )
				{
					if( iStatus==0 )
					{
						if( -1!=(c=fgetc(arg)) )
							while( orglen-- )
								fputc( c,out );
					}
					else
					{
						bits_begin();

						// floor(iStatus bitずつ)＊256 のテーブル読み込み
						FLOOR_ELEM feElem[256],*ptrFl=feElem;
						int i,t,tmp;
						for( i=0,t=0; i!=256; i++ )
						{
							tmp=getbits(iStatus,arg);
							if( tmp )
							{
								feElem[t].iFloor=tmp;
								feElem[t].byChar=i;
								t++;
							}
						}
						// ツリーを構成
						qsort(feElem,t,sizeof(FLOOR_ELEM),CompFloorElem);
						TREE_ELEM *pTree = new TREE_ELEM;
						NextFloor(0,pTree,ptrFl);

						// Decode
						for( i=0; (unsigned)i!=orglen; i++ )
						{
							TREE_ELEM* p=pTree;
							while( !p->bStop ) // ツリーの一番下まで降りる
								p=p->pChild[ getbit(arg) ];
							fputc( p->data,out ); // 出力
						}

						DeleteTree(pTree);
					}
				}
			}
		}

		fclose( out );
		kiutil::timeSet( outname,date,time );
	}

	fclose( arg );
	return true;
}
