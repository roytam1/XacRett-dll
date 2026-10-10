// DmgTool.cpp: CDmgTool クラスのインプリメンテーション
//  # 数年ぶりにXacRettのToolクラスを書いたので
//  # 昔の自分とのコーディングスタイルの違いが…(^^;
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "DmgTool.h"
#include "zlib/zlib.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
typedef unsigned __int64 UINT64;
using namespace std;



//////////////////////////////////////////////////////////////////////
// base64
//////////////////////////////////////////////////////////////////////

static BYTE b64_table[256];
static void base64Decode( const BYTE* beg, const BYTE* end, vector<BYTE>& buf )
{
	unsigned long outbuf=0, i=0;
	for( const BYTE* p=beg; p!=end; ++p )
		if( b64_table[*p] != 0xff )
		{
			outbuf |= (((DWORD)b64_table[*p])<<(18-6*i));
			if( ++i == 4 )
			{
				buf.push_back( BYTE(outbuf>>16) );
				buf.push_back( BYTE(outbuf>>8)  );
				buf.push_back( BYTE(outbuf>>0)  );
				outbuf=i=0;
			}
		}
}

static void base64Init()
{
	int i;
	for( i=0; i!=256; i++ ) b64_table[    i] = 0xff;
	for( i=0; i!=26; i++ )  b64_table['A'+i] = i;
	for( i=0; i!=26; i++ )  b64_table['a'+i] = 26+i;
	for( i=0; i!=10; i++ )  b64_table['0'+i] = 52+i;
	b64_table['+'] = 62;
	b64_table['/'] = 63;
}

// # All multi-byte integer values are stored in big-endian format. 
#define UInt16(ptr) (((ptr)[0]<< 8)+((ptr)[1]<< 0))
#define UInt32(ptr) (((ptr)[0]<<24)+((ptr)[1]<<16)+((ptr)[2]<< 8)+((ptr)[3]<< 0))
#define UInt64(ptr) ((UINT64)(UInt32(ptr))<<32) + UInt32(ptr+4)

//////////////////////////////////////////////////////////////////////
// CDmgTool 実装
//////////////////////////////////////////////////////////////////////

static const char PropListDTD[] =
	"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">";

CDmgTool::CDmgTool()
	: CTool( "Dmg 展開" )
{
	base64Init();
}

bool CDmgTool::Check( const char* fname, unsigned long fsize )
{
	MemmapFile mf;
	if( !mf.Open(fname) )
		return false;
	const BYTE* beg = mf.base();
	const BYTE* end = beg + mf.size();
//--------------------------------------------------------------------//

	if( fsize <= 0x400 )
		return false;

	return end != std::search( beg, end, PropListDTD, PropListDTD+sizeof(PropListDTD)-1 );
}

bool CDmgTool::Extract( const char* fname,const char* ddir )
{
	MemmapFile mf;
	if( !mf.Open(fname) )
		return false;
	const BYTE* beg = mf.base();
	const BYTE* end = beg + mf.size();
//--------------------------------------------------------------------//

	struct tmp
	{
		char path[MAX_PATH];
		 tmp() { path[0]=0; }
		~tmp() { if(path[0]) ::DeleteFile(path); }
	} tmp;

	// "ER"で始まってなければzlib圧縮済みdmgと仮定。一時ファイルへ展開する。
	if( !(beg[0]=='E' && beg[1]=='R' && beg[2]==0x02 && beg[3]==0x00) )
	{
		// 一時ファイル作成
		char tmpDirPath[MAX_PATH];
		GetTempPath( MAX_PATH, tmpDirPath );
		GetTempFileName( tmpDirPath, "Xac", 0, tmp.path );

		// zlib展開
		FILE* fp = fopen(tmp.path,"wb");
		bool b = zlibDecode( beg, end, fp );
		fclose(fp);
		if( !b ) return false;

		// 開き直し
		if( !mf.Open(tmp.path) )
			return false;
		beg = mf.base();
		end = beg + mf.size();
		if( mf.size()<=0x400 || !(beg[0]=='E' && beg[1]=='R' && beg[2]==0x02 && beg[3]==0x00) )
			return false;
	}

	// HFS+ in DMG の展開処理
	bool b = hfsExpand( beg, end, ddir );
	mf.Close();
	return b;
}

bool CDmgTool::zlibDecode( const BYTE* beg, const BYTE* end, FILE* fp )
{
	// ブロック情報がこんな感じのXMLで途中に埋め込まれている。
	//
	// <!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
	// <plist version="1.0">
	// <dict>
	//		<key>resource-fork</key>
	//		<dict>
	//			<key>blkx</key>
	//			<array>
	//				<dict>
	//					...
	//					<data>
	//						bWlzaAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAA
	//						AAII/////wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	//						AAIAAAAgQMOcU5AadDS//+NQRAJCRJAadnyUG7mgv//j
	//						UEQCQkKQG/rAkB2z2L//41AkAiJCkB20eJQb8vwAAAAA
	//						AAAAEwAAAAD/////v//jcAAAAAMAAAABAAAAAQAAAAAA
	//						AAATAAAACgAAAAG//+SQADQmYL//5ZwAM6OwgAAABQAA
	//						AAAAAAACgAAABQAAAAAAAAAAAAAAAAAAAAAAAAABAAAA
	//						AAAAAAAAAAAAAAAAFv////8AAAAAAAAAAAAAAAEAAAAA
	//						AAAAAAAAAAAAAAAAAAAAAAAAAAA=
	//					</data>
	//					...
	//				</dict>
	//				繰り返し...
	//		...
	//	</plist>
	//
	// data要素の中身をbase64デコードすると、
	//		UInt64 block_type;   // 圧縮or非圧縮orゼロブロック? etc
	//		UInt64 output_offs;
	//		UInt64 output_size;
	//		UInt64 input_offset;
	//		UInt64 input_size;
	//	の繰り返しなので、それに従ってコピーや展開すればok.


	// <key>blxk</key> ～ </array> を抽出
	const BYTE* plist_beg = std::search( beg, end, PropListDTD, PropListDTD+sizeof(PropListDTD)-1 );
	static const char keyblkx[]   = "<key>blkx</key>";
	static const char plist_end[] = "</array>";
	const BYTE* blkx_beg = std::search( plist_beg, end, keyblkx, keyblkx+sizeof(keyblkx)-1 );
	const BYTE* blkx_end = std::search(  blkx_beg, end, plist_end, plist_end+sizeof(plist_end)-1 );
	if( blkx_beg == end )
		return false;

	// 全ての <data> ～ </data> について…
	static const char dataB[] = "<data>";
	static const char dataE[] = "</data>";
	const BYTE* data_beg = std::search( blkx_beg, blkx_end, dataB, dataB+sizeof(dataB)-1 );

	unsigned long zoffs=0, tempzoffs=0;
	while( data_beg != blkx_end )
	{
		data_beg += sizeof(dataB);
		const BYTE* data_end = std::search( data_beg, blkx_end, dataE, dataE+sizeof(dataE)-1 );
		{
			// base64 decode
			vector<BYTE> buf;
			base64Decode( data_beg, data_end, buf );
			BYTE* p = (&buf[0]) + 204;
			BYTE* e = (&buf[0]) + buf.size();

			for( ; p+40<=e; p+=40 )
			{
				const unsigned long blockType = UInt32(p+ 0);
				const unsigned long outOffs   = UInt32(p+12) * 0x200;
				const unsigned long outSiz    = UInt32(p+20) * 0x200;
				const unsigned long inOffs    = UInt32(p+28);
				const unsigned long inSiz     = UInt32(p+36);

				// ほんとはoutOffsを見た方がいいの鴨
				if( blockType == 0x80000005 )
				{
					// zlib展開
					inflateBlock( beg+inOffs+zoffs, inSiz, fp, outSiz );
				}
				else if( blockType == 0x00000001 )
				{
					// 生
					fwrite( beg+inOffs+zoffs, 1, inSiz, fp );
					if( inSiz < outSiz )
						for(int i=0; i!=outSiz-inSiz; ++i)
							fputc(0, fp);
				}
				else if( blockType == 0x00000002 )
				{
					// ゼロブロック
					for(int i=0; i!=outSiz; ++i)
						fputc(0, fp);
				}
				else if( blockType == 0xffffffff )
				{
					// 終端ブロック
				}
				tempzoffs = inOffs + inSiz;
			}
			zoffs += tempzoffs;
		}
		data_beg = std::search( data_end, blkx_end, dataB, dataB+sizeof(dataB)-1 );
	}

	return true;
}

void CDmgTool::inflateBlock( const BYTE* buf, size_t siz, FILE* fp, size_t outs )
{
	// zlib準備
	z_stream_s zs;
	zs.zalloc   = NULL;
	zs.zfree    = NULL;
	zs.next_in  = const_cast<BYTE*>(buf);
	zs.avail_in = siz;

	// 出力バッファ
	BYTE* outbuf = CTool::common_buf;
	int   outsiz = XACR_BUFSIZE;
	zs.next_out = outbuf;
	zs.avail_out= outsiz;

	// スタート
	inflateInit2( &zs, 15 );

	// 伸張
	int err = Z_OK;
	while( err!=Z_STREAM_END )
	{
		err = inflate( &zs, Z_PARTIAL_FLUSH );
		if( err!=Z_STREAM_END && err!=Z_OK )
			break;

		fwrite( outbuf, 1, outsiz-zs.avail_out, fp );
		outs        -= outsiz-zs.avail_out;
		zs.next_out  = outbuf;
		zs.avail_out = outsiz;
	}

	// 終了
	inflateEnd(&zs);
	if( outs>0 )
		for(int i=0; i!=outs; ++i)
			fputc(0, fp);
}

bool CDmgTool::hfsExpand( const BYTE* beg, const BYTE* end, const char* destDir )
{
	// HFS+のヘッダを探す
	beg += 0x400;
	for( ; beg+0x200<=end; beg+=0x100 )
		if( beg[0]=='H' && beg[1]=='+' )
			break;
	if( beg+0x200 >= end )
		return false;

	// ヘッダの読み込み
	//	struct HFSPlusVolumeHeader {
	//	0x000
	//		UInt16              signature;
	//		UInt16              version;
	//		UInt32              attributes;
	//		UInt32              lastMountedVersion;
	//		UInt32              journalInfoBlock;
	//	0x010
	//		UInt32              createDate;
	//		UInt32              modifyDate;
	//		UInt32              backupDate;
	//		UInt32              checkedDate;
	//	0x020 
	//		UInt32              fileCount;
	//		UInt32              folderCount;
	//		UInt32              blockSize;
	//		UInt32              totalBlocks;
	//	0x030
	//		UInt32              freeBlocks;
	//		UInt32              nextAllocation;
	//		UInt32              rsrcClumpSize;
	//		UInt32              dataClumpSize;
	//	0x040
	//		UInt32              nextCatalogID;
	//		UInt32              writeCount;
	//		UInt64              encodingsBitmap;
	//	0x050
	//		UInt32              finderInfo[8];
	//	0x070
	//		HFSPlusForkData     allocationFile;
	//	0x0c0
	//		HFSPlusForkData     extentsFile;
	//	0x110
	//		HFSPlusForkData     catalogFile;
	//	0x160
	//		HFSPlusForkData     attributesFile;
	//	0x1b0
	//		HFSPlusForkData     startupFile;
	//			struct HFSPlusForkData {
	//			    UInt64                  logicalSize;
	//			    UInt32                  clumpSize;
	//			    UInt32                  totalBlocks;
	//			    HFSPlusExtentRecord     extents;
	//					struct HFSPlusExtentDescriptor {
	//						UInt32                  startBlock;
	//						UInt32                  blockCount;
	//					};
	//					typedef HFSPlusExtentDescriptor HFSPlusExtentRecord[8];
	//			};
	//	};

	const BYTE* hdr = beg; beg -= 0x400;
	const unsigned long BlockSize   = UInt32( hdr + 0x28 );
	const unsigned long CatalogSize = UInt64( hdr + 0x110 + 0 );
	const unsigned long CatalogPos  = BlockSize * UInt32(hdr+0x110+0x10);
	const unsigned long FileCount   = UInt32( hdr + 0x20 );
	const unsigned long FolderCount = UInt32( hdr + 0x24 );

	// カタログファイルの読み込み
	const BYTE* catalog_beg = beg + CatalogPos;
	const BYTE* catalog_end = beg + CatalogPos + CatalogSize;
	if( catalog_end >= end )
		return false;

	// カタログファイルの中身はB木。
	//	B木のノードは
	//	  [BTNodeDescripter]
	//	  [中身]
	//	    ...
	//	  [レコード2へのオフセット]
	//	  [レコード1へのオフセット]
	//	  [レコード0へのオフセット]
	//	の形に配置されている。特にヘッダノード(先頭にあるノード)は
	//	  [BTNodeDescripter]
	//	  [BTHeaderRec]
	//	  [他]
	//	    ...
	//	の形。
	//
	//	struct BTNodeDescriptor {
	//	0x000
	//		UInt32    fLink;
	//		UInt32    bLink;
	//		SInt8     kind;
	//		UInt8     height;
	//		UInt16    numRecords;
	//		UInt16    reserved;
	//	};
	//	struct BTHeaderRec {
	//		UInt16    treeDepth;
	//	0x010
	//		UInt32    rootNode;
	//		UInt32    leafRecords;
	//		UInt32    firstLeafNode;
	//		UInt32    lastLeafNode;
	//	0x020
	//		UInt16    nodeSize;
	//		UInt16    maxKeyLength;
	//		UInt32    totalNodes;
	//		UInt32    freeNodes;
	//		UInt16    reserved1;
	//	0x02e
	//		UInt32    clumpSize;      // misaligned
	//	0x032
	//		UInt8     btreeType;
	//		UInt8     keyCompareType;
	//		UInt32    attributes;     // long aligned again
	//	0x038
	//		UInt32    reserved3[16];
	//	};
	//
	// カタログファイルのB木の各レコードは、次の形。フォルダなら
	//	  [HFSPlusCatalogKey]
	//	  [HFSPlusCatalogFolder]
	//	あるいはファイルなら
	//	  [HFSPlusCatalogKey]
	//	  [HFSPlusCatalogFile]
	//
	//	struct HFSPlusCatalogKey {
	//		UInt16              keyLength;  // }サイズ2
	//		HFSCatalogNodeID    parentID;   // } +
	//		HFSUniStr255        nodeName;   // } + サイズkeyLength
	//	};
	//	struct HFSPlusCatalogFolder {
	//		SInt16              recordType;
	//		UInt16              flags;
	//		UInt32              valence;
	//		HFSCatalogNodeID    folderID;
	//		UInt32              createDate;
	//		UInt32              contentModDate;
	//		UInt32              attributeModDate;
	//		UInt32              accessDate;
	//		UInt32              backupDate;
	//		HFSPlusBSDInfo      permissions;
	//		FolderInfo          userInfo;
	//		ExtendedFolderInfo  finderInfo;
	//		UInt32              textEncoding;
	//		UInt32              reserved;
	//	};
	//	struct HFSPlusCatalogFile {
	//	0x00
	//		SInt16              recordType;
	//		UInt16              flags;
	//		UInt32              reserved1;
	//		HFSCatalogNodeID    fileID;
	//		UInt32              createDate;
	//	0x10
	//		UInt32              contentModDate;
	//		UInt32              attributeModDate;
	//		UInt32              accessDate;
	//		UInt32              backupDate;
	//	0x20
	//		HFSPlusBSDInfo      permissions;
	//		FileInfo            userInfo;
	//		ExtendedFileInfo    finderInfo;
	//		UInt32              textEncoding;
	//		UInt32              reserved2;
	//	0x58
	//		HFSPlusForkData     dataFork;
	//	0xa8
	//		HFSPlusForkData     resourceFork;
	//	};

	const unsigned long NodeSize  = UInt16( catalog_beg+0x20 );
	const unsigned long FirstLeaf = UInt32( catalog_beg+0x18 );

	// １パス目。「ID==>ファイル名/フォルダ名」のマッピングを作成
	map<unsigned long, unsigned long> id2parent_id;
	map<unsigned long, string>        id2name;
	{
		for( unsigned long Leaf=FirstLeaf; Leaf; )
		{
			// node
			const BYTE* node = catalog_beg+Leaf*NodeSize;
			if( node>=catalog_end || node[0x8]!=0xff ) // not valid leaf node
				continue;
			// for each record ...
			const unsigned long record_num = UInt16( node+0xa );
			for( unsigned long i=0; i!=record_num; ++i )
			{
				const BYTE* record = node + UInt16(node+NodeSize-(i+1)*2);
				const unsigned long keyLength  = UInt16(record+0x00);
				const unsigned long parentID   = UInt32(record+0x02);
				const unsigned long uniNameLen = UInt16(record+0x06);
				const BYTE*         uniName    = record+8;

				const BYTE* desc   = record + keyLength+2;
				const int   type   = UInt16(desc+0x00);
				const int   id     = UInt32(desc+0x08);
				if( type==1 || type==2 ) // 1:folder 2:file
				{
					// ファイル名(UCS2-BE)読みとり
					WCHAR* wbuf = new WCHAR[uniNameLen+1];
					for(int i=0; i!=uniNameLen; ++i)
						wbuf[i] = (uniName[i*2]<<8) + (uniName[i*2+1]);
					wbuf[uniNameLen] = 0;

					// 文字コード変換
					int siz =
						WideCharToMultiByte( CP_ACP, WC_COMPOSITECHECK, wbuf, -1, NULL, 0, NULL, NULL );
					char* buf = new char[siz+1];
					WideCharToMultiByte( CP_ACP, WC_COMPOSITECHECK, wbuf, -1, buf, siz+1, NULL, NULL );

					// 記憶
					id2name[id]      = buf;
					id2parent_id[id] = parentID;
					delete [] buf;
					delete [] wbuf;
				}
			}

			Leaf = UInt32(node); // follow fLink
		}
	}

	// ２パス目。展開処理。
	{
		for( unsigned long Leaf=FirstLeaf; Leaf; )
		{
			// node
			const BYTE* node = catalog_beg+Leaf*NodeSize;
			if( node>=catalog_end || node[0x8]!=0xff ) // not valid leaf node
				continue;
			// for each record ...
			const unsigned long record_num = UInt16( node+0xa );
			for( unsigned long i=0; i!=record_num; ++i )
			{
				const BYTE* record  = node + UInt16(node+NodeSize-(i+1)*2);
				const BYTE* uniName = record+8;
				const unsigned long keyLength  = UInt16(record+0x00);
				const BYTE* desc   = record + keyLength+2;
				const int   type   = UInt16(desc+0x00);
				int id             = UInt32(desc+0x08);
				if( ((uniName[0]<<8)+uniName[1]) && type==1 || type==2 ) // 1:folder 2:file
				{
					// ファイル名生成
					string name;
					for( ; id2parent_id.count(id); id=id2parent_id[id] )
						if( name.size()==0 ) name = id2name[id];
						else                 name = id2name[id] + '/' + name;
					if( type == 1 )
						name += '/';

					// ディレクトリ生成
					char outname[1000];
					if( name.size()>=1000 )
						continue;
					strcpy( outname, name.c_str() );
					if (m_Endflg) return true;//追加 By Bonty
					//ワイルドカード処理
					if (!CheckStr(outname))//追加 By Bonty
					{
						continue;
					}
					kiutil::pathMake( outname );

					// 展開！
					if( type == 2 )
					{
						FILE* fp = fopen( outname, "wb" );
						UINT64 dsize       = UInt64(desc+0x58);
						const unsigned long createDate     = UInt32(desc+0x0c);
						const unsigned long contentModDate = UInt32(desc+0x10);
						const unsigned long accessDate     = UInt32(desc+0x18);

						const unsigned long totalBlockNum  = UInt32(desc+0x64);
						for(int i=0; i!=totalBlockNum; ++i)
						{
							// 8個目以降のblockはextent overflow fileに書かれているはず。
							// だが、そこまで断片化したdmgはとりあえず考えないことにする。
							if( i >= 8 )
								break;
							const unsigned long startBlock = UInt32(desc+0x68+8*i);
							const unsigned long blockCount = UInt32(desc+0x6c+8*i);
							for(int j=0; j!=blockCount; ++j)
							{
								unsigned long len = min(BlockSize, dsize);
								fwrite( beg+BlockSize*(startBlock+j), 1, len, fp );
								dsize -= len;
							}
							if( dsize==0 )
								break;
						}

						fclose(fp);

						// HFS Plus stores dates in several data structures, including the volume header
						// and catalog records. These dates are stored in unsigned 32-bit integers (UInt32)
						// containing the number of seconds since midnight, January 1, 1904, GMT. This is
						// slightly different from HFS, where the value represents local time.
						SYSTEMTIME st = {1904, 1, 5, 1, 0, 0, 0, 0}; // 5=FriDay
						FILETIME ft, lft;
						SystemTimeToFileTime( &st, &ft );
						*reinterpret_cast<UINT64*>(&ft)
							+= ((UINT64)contentModDate)*10*1000*1000;
						FileTimeToLocalFileTime( &ft, &lft );
						kiutil::timeSet( outname, &lft );
					}
				}
			}

			Leaf = UInt32(node); // follow fLink
		}
	}
	return true;
}
