
#include "stdafx.h"
#include "DzTool.h"

/*
Dz-Format ( intel-byte-order )

  DZ      : 2byte  00
  maj_ver : 1byte  02
  min_ver : 1byte  03
  indexPos:  uint  04
  fileNum :   int  08
  data    :        0c
  direntry:        index_Pos

DirEntry

  ptr     :  uint  00 // start of file inside dz
  csz     :  uint  04 // v1: intermediate size. v2: compressed size
  usz     :  uint  08 // uncompressed size
  fnlen   : short  0c // length of name (includes '\0')
  pak     :ushort  0e //
  crc     :  uint  10 // 32bitCRC
  typ     :  uint  14 //
  date    :   int  18 // v2only
  isz     :  uint  2c // v2only: intermediate size (demフィルタをかける前のサイズ)
  fname   :        20(18)

*/

bool CDzTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	if( siz<12 || hdr[0]!='D' || hdr[1]!='Z' || hdr[2]>2 || (hdr[11]&0x70) )
		return false;

	unsigned int indexPos = hdr[4]+(hdr[5]<<8)+(hdr[ 6]<<16)+(hdr[ 7]<<24);
	unsigned int fileNum  = hdr[8]+(hdr[9]<<8)+(hdr[10]<<16)+(hdr[11]<<24);
	if( indexPos<12 || fileNum==0 || (indexPos + 24*fileNum)>=fsize )
		return false;

	return true;
}

bool CDzTool::Extract( const char* fname,const char* ddir )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	if( !(dz = fopen( fname, "rb" )) ) return false;

	bool ans = false;
	if( 12 == fread( buf, 1, 12, dz ) )
	{
		// ヘッダ情報読みとり
		unsigned int ver1 = (buf[2]==1 ? 8 : 0);
		unsigned int indexPos = buf[4]+(buf[5]<<8)+(buf[ 6]<<16)+(buf[ 7]<<24);
		unsigned int fileNum  = buf[8]+(buf[9]<<8)+(buf[10]<<16)+(buf[11]<<24);

		// 格納ファイル情報読みとり
		if( 0==fseek( dz, indexPos, SEEK_SET ) )
		{
			unsigned int i, fnlen;
			fileInf* fi = new fileInf[ fileNum ];

			for( i=0; i<fileNum; i++ )
			{
				if( 32-ver1 != fread( buf, 1, 32-ver1, dz ) )
					break;
				fi[i].ptr = buf[ 0]+(buf[ 1]<<8)+(buf[ 2]<<16)+(buf[ 3]<<24);
				fi[i].csz = buf[ 4]+(buf[ 5]<<8)+(buf[ 6]<<16)+(buf[ 7]<<24);
				fi[i].usz = buf[ 8]+(buf[ 9]<<8)+(buf[10]<<16)+(buf[11]<<24);
				fi[i].pak = buf[14]+(buf[15]<<8);
				fi[i].type= buf[20]+(buf[21]<<8)+(buf[22]<<16)+(buf[23]<<24);
				if( ver1 )
				{
					fi[i].date = 0;
					fi[i].isz  = fi[i].csz;
				}
				else
				{
					fi[i].date= buf[24]+(buf[25]<<8)+(buf[26]<<16)+(buf[27]<<24);
					fi[i].date += 0x00200000;
					fi[i].isz = buf[28]+(buf[29]<<8)+(buf[30]<<16)+(buf[31]<<24);
				}
				fnlen = buf[12]+(buf[13]<<8);
				fread( buf, 1, fnlen, dz );
				fnlen = (fnlen>MAX_PATH-1 ? MAX_PATH-1 : fnlen);
				memcpy( fi[i].nam, buf, fnlen );
				fi[i].nam[fnlen] = '\0';
			}

			// 解凍！
			if( i==fileNum && 0==fseek( dz, 12, SEEK_SET ) )
				ans = decodeAll( fi, fileNum, ver1!=0, indexPos );

			delete [] fi;
		}
	}


	fclose( dz );
	return ans;
}

void CDzTool::init_zlib( unsigned long flen )
{
	ztotal      = flen;
	zs.zalloc   = NULL;
	zs.zfree    = NULL;
	zs.opaque   = 0;

	fp_to_zlbuf();
	inflateInit( &zs );
}

void CDzTool::end_zlib()
{
	inflateEnd( &zs );
}

void CDzTool::fp_to_zlbuf()
{
	if( ztotal==0 )
		zs.avail_in = 0;
	else
	{
		zs.next_in  = CTool::common_buf + (XACR_BUFSIZE/2);

		unsigned int x = (ztotal>(XACR_BUFSIZE/2) ? (XACR_BUFSIZE/2) : ztotal);
		int rd = fread( zs.next_in, 1, x, dz );
		if( rd==-1 ) rd=0;
		ztotal     -= rd;
		zs.avail_in = rd;
	}
}

int CDzTool::read_from_zlib( int siz, unsigned char* ob, unsigned int obs )
{
	obs = (ob ? obs : XACR_BUFSIZE/2);

	// CTool::common_buf に 返値byte入る
	zs.next_out  = (ob ? ob : CTool::common_buf);
	zs.avail_out = ((unsigned)siz>obs ? obs : (unsigned)siz);

	int err, szfirst = zs.avail_out;

	while( zs.avail_out )
	{
		err = inflate( &zs, Z_NO_FLUSH );
		if( err!=Z_STREAM_END && err!=Z_OK )
			break;
		if( zs.avail_in<=0 )
		{
			fp_to_zlbuf();
			if( zs.avail_in<=0 )
				break;
		}
	}

	return (szfirst - zs.avail_out);
}

void CDzTool::writeOut( void* p, unsigned int s )
{
	if( wp )
		fwrite( p, 1, s, wp );
}

FILE* CDzTool::createPak( fileInf& fi )
{
	closePak();
	pak    = fopen( fi.nam, "wb" );
	pakinf = &fi;

	if( pak )
	{
		// ヘッダ
		struct {
			int id;
			int offset;
			int size;
		} pakheader;
		pakheader.id     = *(int*)("PACK");
		pakheader.size   = fi.pak * sizeof(pakentry);
		pakheader.offset = fi.usz - pakheader.size;
		fwrite( &pakheader, 1, 12, pak );

		// entryメモリ確保
		pakEnt = new pakentry[fi.pak];
		memset( pakEnt, 0, fi.pak * sizeof(pakentry) );
	}
	return pak;
}

FILE* CDzTool::addPak( fileInf& fi )
{
	if( pak )
	{
		strcpy( pakEnt[fi.pak-1].name, fi.nam );
		pakEnt[fi.pak-1].len = fi.usz;
		pakEnt[fi.pak-1].ptr = ftell(pak);
	}
	return pak;
}

void CDzTool::closePak()
{
	if( pak && pakinf )
	{
		fwrite( pakEnt, pakinf->pak, sizeof(pakentry), pak );	
		fclose( pak );
		if( pakinf->date )
			kiutil::timeSet( pakinf->nam, WORD(pakinf->date>>16), WORD(pakinf->date&0xffff) );
		delete [] pakEnt;
		pak    = NULL;
		pakinf = NULL;
		pakEnt = NULL;
	}
}

bool CDzTool::decodeAll( fileInf* fi, unsigned int fn, bool ver1, unsigned long datend )
{
	unsigned char* buf = CTool::common_buf;
//--------------------------------------------------------------------//

	wp = pak = NULL;
	if( ver1 )
		init_zlib( datend-12 );

	for( unsigned int i=0; i<fn; i++ )
	{
		if( !ver1 )
			fseek( dz, fi[i].ptr, SEEK_SET );

		if (m_Endflg) return true;//追加 By Bonty
		//ワイルドカード処理
		if (!CheckStr(fi[i].nam)) continue;//追加 By Bonty

		char* path = (fi[i].pak ? fi[i].nam : kiutil::pathMake( fi[i].nam ));

		// ディレクトリ
		if( fi[i].type == TYPE_Dir )
			{ ::CreateDirectory( path, NULL ); continue; }
		// 新規Pak書庫
		if( fi[i].type == TYPE_Pak )
			{ createPak( fi[i] ); continue; }
		// Pak内ファイル
		else if( fi[i].pak )
			wp = addPak( fi[i] );
		// ノーマルファイル
		else
			{ closePak(); wp = fopen( path, "wb" ); }

		// err!!
		if( !wp && !ver1 )
			continue;

		// 無圧縮
		if( fi[i].type == TYPE_Store )
		{
			int x=0;
			for( unsigned int tmp, sss=fi[i].csz; sss; sss-=x )
			{
				tmp = (sss>XACR_BUFSIZE ? XACR_BUFSIZE : sss);
				x = fread( CTool::common_buf, 1, tmp, dz );
				if( x<=0 )
					break;
				writeOut( CTool::common_buf, x );
			}
		}
		// deflate圧縮
		else
		{
			if( !ver1 ) init_zlib( fi[i].csz );

			int demType =
				(fi[i].type==TYPE_Dem || fi[i].type==TYPE_Demv1 || fi[i].type==TYPE_Nehahra)
				? fi[i].type : 0;
			if( demType==0 )
			{
				for( int x; x=read_from_zlib(fi[i].usz); fi[i].usz -= x )
					writeOut( CTool::common_buf, x );
			}
			else
				demDecode( fi[i].usz, demType );

			if( !ver1 )  end_zlib();
		}

		// 閉じる
		if( !pak && wp )
		{
			fclose( wp );
			if( fi[i].date )
				kiutil::timeSet( path, WORD(fi[i].date>>16), WORD(fi[i].date&0xffff) );
		}
	}

	closePak();
	if( ver1 )
		end_zlib();
	return true;
}

//--------------------------------------------------

static unsigned char *inptr,*inblk,*outptr,*outblk;
static int outlen;
static int uemask;
static int cdmask;

static void copy_msg(int i)
{
	memcpy( outblk+outlen, inptr, i );
	outlen += i;
	inptr  += i;
}

static void insert_msg(void *msg, int i)
{
	memcpy( outblk+outlen, msg, i );
	outlen += i;
}

#define discard_msg(x) inptr += x

#define getshort(x) (*(short*)(x))
#define getlong(x)  (*(long*)(x))
#define getfloat(x) (*(float*)(x))
#define cnvlong(x)  (x)

enum {
	DEM_bad, DEM_nop, DEM_disconnect, DEM_updatestat, DEM_version,
	DEM_setview, DEM_sound, DEM_time, DEM_print, DEM_stufftext,
	DEM_setangle, DEM_serverinfo, DEM_lightstyle, DEM_updatename,
	DEM_updatefrags, DEM_clientdata, DEM_stopsound, DEM_updatecolors,
	DEM_particle, DEM_damage, DEM_spawnstatic, DEM_spawnbinary,
	DEM_spawnbaseline, DEM_temp_entity, DEM_setpause, DEM_signonnum,
	DEM_centerprint, DEM_killedmonster, DEM_foundsecret,
	DEM_spawnstaticsound, DEM_intermission, DEM_finale,
	DEM_cdtrack, DEM_sellscreen, DEM_cutscene, DZ_longtime,
	DEM_showlmp = 35, DEM_hidelmp, DEM_skybox, DZ_showlmp
};
struct cdata_t {
	char voz, pax;
	char ang0, ang1, ang2;
	char vel0, vel1, vel2;
	long items;
	char uk10, uk11, invbit;
	char wpf, av, wpm;
	int health;
	char am, sh, nl, rk, ce, wp;
	int force;
};
struct ent_t {
	char modelindex, frame;
	char colormap, skin;
	char effects;
	char ang0, ang1, ang2;
	char newbit, present, active;
	char fullbright;	// nehahra
	int org0, org1, org2;
	int od0, od1, od2;
	int force;
	float alpha;		// nehahra
};

#define MAX_ENT 1024
static cdata_t oldcd, newcd;
static ent_t base[MAX_ENT], oldent[MAX_ENT], newent[MAX_ENT];
static int entlink[MAX_ENT];
static char dem_updateframe;
static char copybaseline;
static long dem_gametime;
static int maxent, lastent, sble;
static int dem_decode_type;
static long cam0, cam1, cam2;
static char cdstring_char;

//-- デコーダメッセージテーブル -----------------------------------

void demx_nop()				{copy_msg(1);}
void demx_disconnect()		{copy_msg(1);}
void demx_updatestat()		{copy_msg(6);}
void demx_version()			{copy_msg(5);}
void demx_setview()			{copy_msg(3);}
void demx_setangle()		{copy_msg(4);}
void demx_updatefrags()		{copy_msg(4);}
void demx_stopsound()		{copy_msg(3);}
void demx_updatecolors()	{copy_msg(3);}
void demx_particle()		{copy_msg(12);}
void demx_damage()			{copy_msg(9);}
void demx_spawnstatic()		{copy_msg(14);}
void demx_spawnbinary()		{copy_msg(1);}
void demx_setpause()		{copy_msg(2);}
void demx_signonnum()		{copy_msg(2);}
void demx_killedmonster()	{copy_msg(1);}
void demx_foundsecret()		{copy_msg(1);}
void demx_spawnstaticsound(){copy_msg(10);}
void demx_intermission()	{copy_msg(1);}
void demx_cdtrack()			{copy_msg(3);}
void demx_sellscreen()		{copy_msg(1);}

void demx_string()
{
	unsigned char *ptr = inptr + 1;
	while (*ptr++);
	copy_msg(ptr-inptr);
}

void demx_sound()
{
	int c, len = 11 - (*inptr > DEM_sound);
	unsigned int entity;
	char mask = inptr[1];
	char channel;
	if (len == 10) mask = *inptr & 3;
	if (mask & 0x01) len++;
	if (mask & 0x02) len++;
	if (dem_decode_type == TYPE_Demv1) { copy_msg(len); return; }
	*inptr = DEM_sound;
	insert_msg(inptr,1);
	*inptr = mask;
	channel = inptr[len-9] & 7;
	inptr[len-9] = (inptr[len-9] & 0xf8) + ((2 - channel) & 7);
	if ((entity = getshort(inptr+len-9) >> 3) < MAX_ENT)
	{
		c = getshort(inptr+len-6); c += newent[entity].org0;
		c = cnvlong(c); memcpy(inptr+len-6,&c,2);
		c = getshort(inptr+len-4); c += newent[entity].org1;
		c = cnvlong(c); memcpy(inptr+len-4,&c,2);
		c = getshort(inptr+len-2); c += newent[entity].org2;
		c = cnvlong(c); memcpy(inptr+len-2,&c,2);
	}
	copy_msg(len);
}

void demx_longtime()
{
	long tmp = getlong(inptr+1);
	dem_gametime += tmp;
	tmp = cnvlong(dem_gametime);
	*inptr = DEM_time;
	memcpy(inptr+1,&tmp,4);
	copy_msg(5);
}

void demx_time()
{
	char buf[5];
	long tmp = getshort(inptr+1) & 0xffff;
	if (dem_decode_type == TYPE_Demv1) { demx_longtime(); return; }
	dem_gametime += tmp;
	tmp = cnvlong(dem_gametime);
	buf[0] = DEM_time;
	memcpy(buf+1,&tmp,4);
	insert_msg(buf,5);
	discard_msg(3);
}

void demx_serverinfo()
{
	unsigned char *ptr = inptr + 7;
	unsigned char *start_ptr;
	while (*ptr++);
	do {
		start_ptr = ptr;
		while (*ptr++);
	} while (ptr - start_ptr > 1);
	do {
		start_ptr = ptr;
		while (*ptr++);
	} while (ptr - start_ptr > 1);
	copy_msg(ptr-inptr);
	sble = 0;
}

void demx_lightstyle()
{
	unsigned char *ptr = inptr + 2;
	while (*ptr++);
	copy_msg(ptr-inptr);
}

void demx_updatename()
{
	unsigned char *ptr = inptr + 2;
	while (*ptr++);
	copy_msg(ptr-inptr);
}

int bplus(int x, int y)
{
	if (x >= 128) x -= 256;
	return y + x;
}

void create_clientdata_msg()
{
	char buf[32];
	char *ptr = buf+3;
	int mask = newcd.invbit? 0 : 0x0200;
	long tmp;
	buf[0] = DEM_clientdata;
	#define CMADD(x,def,bit,bit2) if (newcd.x != def || newcd.force & bit2)\
		{ mask |= bit; *ptr++ = newcd.x; }
	CMADD(voz,22,0x0001,0x0800);
	CMADD(pax,0,0x0002,0x1000);
	CMADD(ang0,0,0x0004,0x0100);
	CMADD(vel0,0,0x0020,0x0001);
	CMADD(ang1,0,0x0008,0x0200);
	CMADD(vel1,0,0x0040,0x0002);
	CMADD(ang2,0,0x0010,0x0400);
	CMADD(vel2,0,0x0080,0x0004);
	tmp = cnvlong(newcd.items); memcpy(ptr,&tmp,4); ptr += 4;
	if (newcd.uk10) mask |= 0x0400;
	if (newcd.uk11) mask |= 0x0800;
	CMADD(wpf,0,0x1000,0x2000);
	CMADD(av,0,0x2000,0x4000);
	CMADD(wpm,0,0x4000,0x8000);
	tmp = cnvlong(newcd.health); memcpy(ptr,&tmp,2); ptr += 2;
	*ptr++ = newcd.am;
	*ptr++ = newcd.sh;
	*ptr++ = newcd.nl;
	*ptr++ = newcd.rk;
	*ptr++ = newcd.ce;
	*ptr++ = newcd.wp;
	mask = cnvlong(mask);
	memcpy(buf+1,&mask,2);
	insert_msg(buf,ptr-buf);
	oldcd = newcd;
}

#define CPLUS(x,bit) if (mask & bit) { newcd.x = bplus(*ptr++,oldcd.x); }

void demv1_clientdata()
{
	unsigned char *ptr = inptr;
	int mask = *ptr++;
	if (mask & 0x01) mask += *ptr++ << 8;
	if (mask & 0x0100) mask += *ptr++ << 16;
	if (mask & 0x010000) mask += *ptr++ << 24;
	CPLUS(voz,0x01000000);
	CPLUS(pax,0x00100000);
	CPLUS(ang0,0x08000000);
	CPLUS(ang1,0x04000000);
	CPLUS(ang2,0x02000000);
	CPLUS(vel0,0x00000008);
	CPLUS(vel1,0x00000004);
	CPLUS(vel2,0x00000002);
	if (mask & 0x00008000) newcd.uk10 = !oldcd.uk10;
	if (mask & 0x00400000) newcd.uk11 = !oldcd.uk11;
	if (mask & 0x10000000) newcd.invbit = !oldcd.invbit;
	if (mask & 0x00200000)
	{
		newcd.items += getlong(ptr);
		ptr += 4;
	}
	CPLUS(wpf,0x00004000);
	CPLUS(av,0x00080000);
	CPLUS(wpm,0x00020000);
	if (mask & 0x00040000)
	{
		newcd.health += getshort(ptr);
		ptr += 2;
	}
	CPLUS(am,0x00002000);
	CPLUS(sh,0x00001000);
	CPLUS(nl,0x00000800);
	CPLUS(rk,0x00000400);
	CPLUS(ce,0x00800000);
	CPLUS(wp,0x00000200);
	discard_msg(ptr-inptr);
	if ((*ptr & 0xf0) == 0xe0)
	{
		mask = *ptr++;
		if (mask & 0x08) mask |= *ptr++ << 8;
		newcd.force ^= mask & 0xff07;
		discard_msg(ptr-inptr);
	}
	create_clientdata_msg();
}

void demv1_updateentity()
{
	unsigned char *ptr = inptr+1;
	char code = *inptr;
	int mask, entity;
	ent_t n, o;
	dem_updateframe = 1;
	if (code == 0x82) { discard_msg(1); return; }
	if (code == 0x83)
	{
		while ((entity = getshort(ptr)))
		{
			ptr += 2;
			memcpy(newent+entity,base+entity,sizeof(ent_t));
			memcpy(oldent+entity,base+entity,sizeof(ent_t));

		}
		discard_msg(ptr-inptr+2);
		return;
	}
	if (code == 0x84)
	{
		while ((mask = getshort(ptr)))
		{
			ptr += 2;
			mask &= 0xffff;
			if (mask & 0x8000) mask |= *ptr++ << 16;
			entity = mask & 0x3ff;
			if (entity > maxent) maxent = entity;
			newent[entity].force ^= mask & 0xfffc00;
		}
		discard_msg(ptr-inptr+2);
		return;
	}
	for (;;)
	{
		if (code == 0x81)
		{
			mask = (*ptr++ << 8) + 1;
			code = (char)0x80;
		}
		else
		{
			mask = getshort(ptr) & 0xffff;
			ptr += 2;
		}
		if (mask & 0x8000) mask += (*ptr++) << 16;
		if (mask & 0x800000) mask += (*ptr++) << 24;
		entity = mask & 0x1ff;
		if (mask & 0x08000000) entity += 0x200; 
		if (entity > maxent) maxent = entity;
		if (!entity) break;
		n = newent[entity];
		o = oldent[entity];
		n.present = 1;
		if (mask & 0x010000) n.modelindex = *ptr++;
		if (mask & 0x0200) n.frame = o.frame+1;
		if (mask & 0x080000) n.frame = bplus(*ptr++,o.frame);
		if (mask & 0x01000000) n.colormap = *ptr++;
		if (mask & 0x02000000) n.skin = *ptr++;
		if (mask & 0x04000000) n.effects = *ptr++;
		if (mask & 0x0400) n.org0 = bplus(*ptr++,o.org0);
		if (mask & 0x100000) { n.org0 = getshort(ptr); ptr += 2; }
		if (mask & 0x2000) n.ang0 = bplus(*ptr++,o.ang0);
		if (mask & 0x0800) n.org1 = bplus(*ptr++,o.org1);
		if (mask & 0x200000) { n.org1 = getshort(ptr); ptr += 2; }
		if (mask & 0x4000) n.ang1 = bplus(*ptr++,o.ang1);
		if (mask & 0x1000) n.org2 = bplus(*ptr++,o.org2);
		if (mask & 0x400000) { n.org2 = getshort(ptr); ptr += 2; }
		if (mask & 0x020000) n.ang2 = bplus(*ptr++,o.ang2);
		if (mask & 0x040000) n.newbit = !o.newbit;
		newent[entity] = n;
	}
	discard_msg(ptr-inptr);
}

void demv1_dxentities()
{
	char buf[32];
	char *ptr;
	long tmp;
	int i, mask;
	for (i = 1; i <= maxent; i++)
	{
		ent_t n = newent[i], b = base[i];
		if (!n.present) continue;
		ptr = buf+2;
		mask = 0x80;
		if (i > 0xff || (n.force & 0x400000))
		{
			tmp = cnvlong(i);
			memcpy(ptr,&tmp,2);
			ptr += 2;
			mask |= 0x4000;
		}
		else
			*ptr++ = i;
		#define BDIFF(x,bit,bit2) \
			if (n.x != b.x || n.force & bit2) \
				{ *ptr++ = n.x; mask |= bit; }
		BDIFF(modelindex,0x0400,0x040000);
		BDIFF(frame,0x0040,0x4000);
		BDIFF(colormap,0x0800,0x080000);
		BDIFF(skin,0x1000,0x100000);
		BDIFF(effects,0x2000,0x200000);
		if (n.org0 != b.org0 || n.force & 0x010000)
		    { mask |= 0x0002; tmp = cnvlong(n.org0); 
		      memcpy(ptr,&tmp,2); ptr += 2; }
		BDIFF(ang0,0x0100,0x0800);
		if (n.org1 != b.org1 || n.force & 0x0400)
		    { mask |= 0x0004; tmp = cnvlong(n.org1); 
		      memcpy(ptr,&tmp,2); ptr += 2; }
		BDIFF(ang1,0x0010,0x1000);
		if (n.org2 != b.org2 || n.force & 0x020000)
		    { mask |= 0x0008; tmp = cnvlong(n.org2); 
		      memcpy(ptr,&tmp,2); ptr += 2; }
		BDIFF(ang2,0x0200,0x2000);
		if (n.newbit) mask |= 0x20;
		if (mask & 0xff00) mask |= 0x01;
		buf[0] = mask & 0xff;
		buf[1] = (mask & 0xff00) >> 8;
		if (!(mask & 0x01)) { memcpy(buf+1,buf+2,ptr-buf-2); ptr--; }
		insert_msg(buf,ptr-buf);
		memcpy(oldent+i,newent+i,sizeof(ent_t));
	}
}

void demx_clientdata()
{
	unsigned char *ptr = inptr;
	int mask = *ptr++;
	newcd = oldcd;
	if (dem_decode_type == TYPE_Demv1) { demv1_clientdata(); return; }
	if (mask & 0x08) mask += *ptr++ << 8;
	if (mask & 0x8000) mask += *ptr++ << 16;
	if (mask & 0x800000) mask += *ptr++ << 24;
	CPLUS(vel2,0x00000001);
	CPLUS(vel0,0x00000002);
	CPLUS(vel1,0x00000004);
	CPLUS(wpf,0x00000100);
	if (mask & 0x00000200) newcd.uk10 = !oldcd.uk10;
	CPLUS(ang0,0x00000400);
	CPLUS(am,0x00000800);
	if (mask & 0x00001000) { newcd.health += getshort(ptr); ptr += 2; }
	if (mask & 0x00002000) { newcd.items ^= getlong(ptr); ptr += 4; }
	CPLUS(av,0x00004000);
	CPLUS(pax,0x00010000);
	CPLUS(sh,0x00020000);
	CPLUS(nl,0x00040000);
	CPLUS(rk,0x00080000);
	CPLUS(wpm,0x00100000);
	CPLUS(wp,0x00200000);
	if (mask & 0x00400000) newcd.uk11 = !oldcd.uk11;
	CPLUS(voz,0x01000000);
	CPLUS(ce,0x02000000);
	CPLUS(ang1,0x04000000);
	CPLUS(ang2,0x08000000);
	if (mask & 0x10000000) newcd.invbit = !oldcd.invbit;
	discard_msg(ptr-inptr);
	if ((*ptr & 0xf0) == 0x50)
	{
		mask = *ptr++;
		if (mask & 0x08) mask |= *ptr++ << 8;
		newcd.force ^= mask & 0xff07;
		discard_msg(ptr-inptr);
	}
	create_clientdata_msg();
}

void demx_spawnbaseline()
{
	char buf[16];
	unsigned char *ptr = inptr+3;
	ent_t ent;
	int mask = getshort(inptr+1);
	sble = (sble + (mask & 0x03ff)) % 0x400;
	memset(&ent,0,sizeof(ent_t));
	ent.modelindex = *ptr++;
	if (mask & 0x0400) ent.frame = *ptr++;
	if (mask & 0x0800) ent.colormap = *ptr++;
	if (mask & 0x1000) ent.skin = *ptr++;
	if (mask & 0x2000)
	{
		ent.org0 = getshort(ptr); ptr += 2;
		ent.org1 = getshort(ptr); ptr += 2;
		ent.org2 = getshort(ptr); ptr += 2;
	}
	if (mask & 0x4000) ent.ang1 = *ptr++;
	if (mask & 0x8000) { ent.ang0 = *ptr++; ent.ang2 = *ptr++; }
	discard_msg(ptr-inptr);
	buf[0] = DEM_spawnbaseline;
	mask = cnvlong(sble); memcpy(buf+1,&mask,2);
	buf[3] = ent.modelindex;
	buf[4] = ent.frame;
	buf[5] = ent.colormap;
	buf[6] = ent.skin;
	mask = cnvlong(ent.org0); memcpy(buf+7,&mask,2);
	buf[9] = ent.ang0;
	mask = cnvlong(ent.org1); memcpy(buf+10,&mask,2);
	buf[12] = ent.ang1;
	mask = cnvlong(ent.org2); memcpy(buf+13,&mask,2);
	buf[15] = ent.ang2;
	insert_msg(buf,16);
	base[sble] = ent;
	copybaseline = 1;
}

void demx_temp_entity()
{
	static const char te_size[] = {8, 8,  8,  8, 8, 16, 16, 8, 8, 16, 8, 8, 10, 16, 8,  8, 14};
	if (inptr[1] == 17)
		copy_msg(strlen((char*)inptr + 2) + 17);
	else
		copy_msg(te_size[inptr[1]]);
}

void demx_showlmp()
{
	unsigned char *ptr = inptr + 1;
	while (*ptr++);
	while (*ptr++);
	ptr += 2;
	*inptr = DEM_showlmp;
	copy_msg(ptr-inptr);
}

void demx_updateentity()
{
	unsigned char buf[32], *ptr;
	int mask, i, entity;
	int baseval = 0, prev;
	ent_t n, o;
	long tmp;
	if (dem_decode_type == TYPE_Demv1) { demv1_updateentity(); return; }
	lastent = 0;
	for (ptr = inptr+1; *ptr; ptr++)
	{
		if (*ptr == 0xff) { baseval += 0xfe; continue; }
		entity = baseval + *ptr;
		newent[entity].active = 1;
		while (entlink[lastent] <= entity) lastent = entlink[lastent];
		if (lastent < entity)
		{
			entlink[entity] = entlink[lastent];
			entlink[lastent] = entity;
		}
	}
	for (prev = 0, i = entlink[0], ptr++; i < MAX_ENT; i = entlink[i])
	{
		newent[i].org0 += newent[i].od0;
		newent[i].org1 += newent[i].od1;
		newent[i].org2 += newent[i].od2;
		if (!newent[i].active) { prev = i; continue; }
		mask = *ptr++;
		if (mask == 0x80)
		{
			oldent[i] = newent[i] = base[i];
			entlink[prev] = entlink[i];
			continue;
		}
		prev = i;
		if (mask == 0x00) { newent[i].active = 0; continue; }
		if (mask & 0x80) mask += (*ptr++) << 8;
		if (mask & 0x8000) mask += (*ptr++) << 16;
		n = newent[i];
		o = oldent[i];
		if (mask & 0x000001) { n.od2 = bplus(*ptr++,o.od2);
				       n.org2 = o.org2 + n.od2; }
		if (mask & 0x000800) { n.org2 = getshort(ptr); ptr += 2;
				       n.od2 = n.org2 - o.org2; }
		if (mask & 0x000002) { n.od1 = bplus(*ptr++,o.od1);
				       n.org1 = o.org1 + n.od1; }
		if (mask & 0x000400) { n.org1 = getshort(ptr); ptr += 2;
				       n.od1 = n.org1 - o.org1; }
		if (mask & 0x000004) { n.od0 = bplus(*ptr++,o.od0);
				       n.org0 = o.org0 + n.od0; }
		if (mask & 0x000200) { n.org0 = getshort(ptr); ptr += 2;
				       n.od0 = n.org0 - o.org0; }
		if (mask & 0x000008) n.ang0 = bplus(*ptr++,o.ang0);
		if (mask & 0x000010) n.ang1 = bplus(*ptr++,o.ang1);
		if (mask & 0x000020) n.ang2 = bplus(*ptr++,o.ang2);
		if (mask & 0x000040) n.frame = o.frame+1;
		if (mask & 0x000100) n.frame = bplus(*ptr++,o.frame);
		if (mask & 0x001000) n.effects = *ptr++;
		if (mask & 0x002000) n.modelindex = *ptr++;
		if (mask & 0x004000) n.newbit = !o.newbit;
		if (mask & 0x010000) n.colormap = *ptr++;
		if (mask & 0x020000) n.skin = *ptr++;
	// nehahra
		if (mask & 0x040000) { n.alpha = getfloat(ptr); ptr += 4; }
		if (mask & 0x080000) n.fullbright = *ptr++;
		newent[i] = n;
	}
	if (*ptr == 0x31)
	{
		ptr++;
		while ((mask = getshort(ptr)))
		{
			ptr += 2;
			mask &= 0xffff;
			if (mask & 0x8000) mask |= *ptr++ << 16;
			entity = mask & 0x3ff;
			newent[entity].force ^= mask & 0xfffc00;
		}
		ptr += 2;
	}
	discard_msg(ptr-inptr);
	for (i = entlink[0]; i < MAX_ENT; i = entlink[i])
	{
		ent_t n = newent[i], b = base[i];
		ptr = buf+2;
		mask = 0x80;
		if (i > 0xff || (n.force & 0x400000))
		{
			tmp = cnvlong(i);
			memcpy(ptr,&tmp,2);
			ptr += 2;
			mask |= 0x4000;
		}
		else
			*ptr++ = i;
		#define BDIFF(x,bit,bit2) \
			if (n.x != b.x || n.force & bit2) \
				{ *ptr++ = n.x; mask |= bit; }
		BDIFF(modelindex,0x0400,0x040000);
		BDIFF(frame,0x0040,0x4000);
		BDIFF(colormap,0x0800,0x080000);
		BDIFF(skin,0x1000,0x100000);
		BDIFF(effects,0x2000,0x200000);
		if (n.org0 != b.org0 || n.force & 0x010000)
		    { mask |= 0x0002; tmp = cnvlong(n.org0); 
		      memcpy(ptr,&tmp,2); ptr += 2; }
		BDIFF(ang0,0x0100,0x0800);
		if (n.org1 != b.org1 || n.force & 0x0400)
		    { mask |= 0x0004; tmp = cnvlong(n.org1); 
		      memcpy(ptr,&tmp,2); ptr += 2; }
		BDIFF(ang1,0x0010,0x1000);
		if (n.org2 != b.org2 || n.force & 0x020000)
		    { mask |= 0x0008; tmp = cnvlong(n.org2); 
		      memcpy(ptr,&tmp,2); ptr += 2; }
		BDIFF(ang2,0x0200,0x2000);
// nehahra
		if (n.force & 0x800000)
		{
			float f = 1;
			if (n.fullbright)
				f = 2;
			tmp = cnvlong(*(int *)&f);
			memcpy(ptr, &tmp, 4);
			tmp = cnvlong(*(int *)&n.alpha);
			memcpy(ptr + 4, &tmp, 4);
			ptr += 8;
			if (f == 2)
			{
				f = (char)(n.fullbright - 1);
				tmp = cnvlong(*(int *)&f);
				memcpy(ptr, &tmp, 4);
				ptr += 4;
			}
			mask |= 0x8000;
		}
		if (n.newbit) mask |= 0x20;
		if (mask & 0xff00) mask |= 0x01;
		buf[0] = mask & 0xff;
		buf[1] = (mask & 0xff00) >> 8;
		if (!(mask & 0x01)) { memcpy(buf+1,buf+2,ptr-buf-2); ptr--; }
		insert_msg(buf,ptr-buf);
		oldent[i] = newent[i];
	}
}

void dem_copy_ue()
{
	char mask = inptr[0] & 0x7f;
	char topmask;
	int len = 1;
	topmask = (mask & 0x01)? inptr[len++] : 0x00;
	if (topmask & 0x40) len += 2; else len++;
	if (topmask & 0x04) len++;
	if (mask & 0x40) len++;
	if (topmask & 0x08) len++;
	if (topmask & 0x10) len++;
	if (topmask & 0x20) len++;
	if (mask & 0x02) len += 2;
	if (topmask & 0x01) len++;
	if (mask & 0x04) len += 2;
	if (mask & 0x10) len++;
	if (mask & 0x08) len += 2;
	if (topmask & 0x02) len++;
	copy_msg(len);
}

static void (* const demx_message[])() = {
	NULL, demx_nop, demx_disconnect, demx_updatestat, demx_version,
	demx_setview, demx_sound, demx_time, demx_string, demx_string,
	demx_setangle, demx_serverinfo, demx_lightstyle, demx_updatename,
	demx_updatefrags, demx_clientdata, demx_stopsound, demx_updatecolors,
	demx_particle, demx_damage, demx_spawnstatic, demx_spawnbinary,
	demx_spawnbaseline, demx_temp_entity, demx_setpause, demx_signonnum,
	demx_string, demx_killedmonster, demx_foundsecret,
	demx_spawnstaticsound, demx_intermission, demx_string,
	demx_cdtrack, demx_sellscreen, demx_string, demx_longtime,
	demx_string, demx_string, demx_showlmp	// nehahra
};

//--------------------------------------------------------

int CDzTool::decBlock()
{
	if( !cdstring_char )
		while( cdstring_char != '\n' )
		{
			cdstring_char = *inptr++;
			writeOut( &cdstring_char, 1 );
		}

	char cfields = *inptr++;
	if( cfields == 0xff )
	{
		long len = getlong(inblk+1);
		writeOut( inblk+5, len );
		return len+5;
	}
	if( cfields&1 ) { cam0 += getlong(inptr); inptr += 4; }
	if( cfields&2 ) { cam1 += getlong(inptr); inptr += 4; }
	if( cfields&4 ) { cam2 += getlong(inptr); inptr += 4; }

	long a1;
	outlen = 0;
	insert_msg(&a1,4);
	a1 = cnvlong(cam0); insert_msg(&a1,4);
	a1 = cnvlong(cam1); insert_msg(&a1,4);
	a1 = cnvlong(cam2); insert_msg(&a1,4);

	dem_updateframe = 0;
	while( *inptr )
	{		
		if( (*inptr&0xf8) == uemask )
			demx_updateentity();
		else
		{
			if( dem_updateframe )
				{ demv1_dxentities(); dem_updateframe = 0; }
			if( *inptr <= DZ_showlmp )
				demx_message[(int)(*inptr)]();
			else if( (*inptr&0xf0)==cdmask )
				demx_clientdata();
			else if( (*inptr&0xf8)==0x38 )
				demx_sound();
			else if( *inptr>=0x80 )
				dem_copy_ue();
		}
	}
	if( dem_updateframe )
		demv1_dxentities();

	outlen -= 16;
	outlen = cnvlong(outlen);
	memcpy( outblk, &outlen, 4 );
	writeOut( outblk, cnvlong(outlen)+16 );

	if( copybaseline )
	{
		copybaseline = 0;
		memcpy( oldent, base, sizeof(ent_t)*MAX_ENT );
		memcpy( newent, base, sizeof(ent_t)*MAX_ENT );
	}

	return inptr-inblk;//+1;
}

void CDzTool::demDecode( unsigned int siz, int type )
{
//-- dem_uncompress_init()
	cdstring_char   = 0;
	dem_decode_type = type;
	memset( &base,   0, sizeof(ent_t)*MAX_ENT );
	memset( &oldent, 0, sizeof(ent_t)*MAX_ENT );
	memset( &oldcd,  0, sizeof(cdata_t) );
	oldcd.voz    = 22;
	oldcd.items  = 0x4001;
	entlink[0]   = MAX_ENT;
	cam0  = cam1 = cam2 = 0;
	copybaseline = 0;
	dem_gametime = 0;
	maxent       = 0;
	sble         = 0;

//-- original
	uemask = (type==TYPE_Demv1 ? 0x80 : 0x30);
	cdmask = (type==TYPE_Demv1 ? 0xf0 : 0x40);
	 inptr =  inblk = CTool::common_buf;
	outptr = outblk = new unsigned char[32768];

//-- loop
	unsigned char* const b_base = CTool::common_buf;
	const unsigned int   b_size = (XACR_BUFSIZE/2);
	unsigned int             vs = 0;

	for( int x; x=read_from_zlib( siz, b_base+vs, b_size-vs ); )
	{
		vs += x;

		unsigned int ptr=0;
		while( ptr<(b_size-20000) && ptr<vs-1 )
		{
			ptr = decBlock();
			inptr++;
		}

		if( vs>ptr )
		{
			memcpy( b_base, b_base+ptr, vs-ptr );
			vs -= ptr;
		}
		else
			vs = 0;
	}

	// 残ってる分をflush
	unsigned int ptr=0;
	while( ptr<vs-1 )
	{
		ptr = decBlock();
		inptr++;
	}

//-- original
	delete [] outblk;
}
