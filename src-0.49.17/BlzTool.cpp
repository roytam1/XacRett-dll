
#include "stdafx.h"
#include "BlzTool.h"


/*
 * The block-size used to process data.
 */
#define BLOCK_SIZE (56u*1024u)

/*
 * The size of the block header.
 */
#define HEADER_SIZE (6*4)

/*
 * Unsigned char type.
 */
typedef unsigned char byte;

/*
 * Get the low-order 8 bits of a value.
 */
#if CHAR_BIT == 8
# define octet(v) ((byte) (v))
#else
# define octet(v) ((v) & 0x00ffu)
#endif




bool CBlzTool::Check( const char* fname, unsigned long fsize )
{
	const unsigned char* header = CTool::common_buf;
	if (get_uint32(header + 0*4) != 0x626C7A1Aul   || /* "blz\x1A" */
            get_uint32(header + 1*4) != 1              ||
            get_uint32(header + 2*4) > blz_max_packed_size(BLOCK_SIZE) ||
            get_uint32(header + 4*4) > BLOCK_SIZE)
    {
        //printf("ERR: invalid header in compressed file\n");
        return false;
    }
	return true;
}

bool CBlzTool::Extract( const char* packedname, const char* ddir )
{
	byte header[HEADER_SIZE];
    FILE *newfile;
    FILE *packedfile;
    unsigned long insize = 0, outsize = 0;
    static const char rotator[] = "-\\|/";
    unsigned int counter = 0;
	char newname[MAX_PATH];

    byte *data, *packed;
    size_t max_packed_size;

    max_packed_size = blz_max_packed_size(BLOCK_SIZE);

    /* allocate memory */
    if ((data   = (byte *) malloc(BLOCK_SIZE))      == NULL ||
        (packed = (byte *) malloc(max_packed_size)) == NULL)
    {
        //printf("ERR: not enough memory\n");
        return false;
    }

    /* open input file */
    if ((packedfile = fopen(packedname, "rb")) == NULL)
    {
        //printf("ERR: unable to open input file\n");
    	free(packed);
    	free(data);
        return false;
    }

	kiutil::getOriginalName( newname,packedname,
			0==stricmp(kiutil::pathExt(packedname),"blz") ? NULL : "unblz" );

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(kiutil::pathName(newname)))//追加 By Bonty
	{
		free(packed);
    	free(data);
		fclose(packedfile);
		return true;
	}
    /* create output file */
    if ((newfile = fopen(newname, "wb")) == NULL)
    {
    	free(packed);
    	free(data);
		fclose(packedfile);
        //printf("ERR: unable to open output file\n");
        return false;
    }

	bool ret=false;
    /* while we are able to read a header from input file .. */
    while (fread(header, 1, sizeof(header), packedfile) == sizeof(header))
    {
        size_t hdr_packedsize, hdr_depackedsize, depackedsize;

        /* verify values in header */
        if (get_uint32(header + 0*4) != 0x626C7A1Aul   || /* "blz\x1A" */
            get_uint32(header + 1*4) != 1              ||
            get_uint32(header + 2*4) > max_packed_size ||
            get_uint32(header + 4*4) > BLOCK_SIZE)
        {
            //printf("ERR: invalid header in compressed file\n");
            goto endline;
        }

        /* get compressed and original size from header */
        hdr_packedsize   = (size_t) get_uint32(header + 2*4);
        hdr_depackedsize = (size_t) get_uint32(header + 4*4);

        /* read compressed data */
        if (fread(packed, 1, hdr_packedsize, packedfile) != hdr_packedsize)
        {
            //printf("ERR: error reading block from compressed file\n");
            goto endline;
        }

        /* check CRC32 of compressed data */
        if (get_uint32(header + 3*4) != blz_crc32(packed, hdr_packedsize, 0))
        {
            //printf("ERR: compressed data crc error\n");
            goto endline;
        }

        /* decompress data */
        depackedsize = blz_depack(packed, data, hdr_depackedsize);

        /* check for decompression error */
        if (depackedsize != hdr_depackedsize)
        {
            //printf("ERR: an error occured while decompressing\n");
            goto endline;
        }

        /* check CRC32 of decompressed data */
        if (get_uint32(header + 5*4) != blz_crc32(data, depackedsize, 0))
        {
            //printf("ERR: decompressed file crc error\n");
            goto endline;
        }

        /* write decompressed data */
        fwrite(data, 1, depackedsize, newfile);

        /* sum input and output size */
        insize  += hdr_packedsize + sizeof(header);
        outsize += depackedsize;
    }
	ret=true;

endline:
    /* close files */
    fclose(packedfile);
    fclose(newfile);

    /* free memory */
    free(packed);
    free(data);

    return ret;
}


const unsigned int CBlzTool::blz_crctab_n[16] = {
   0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190,
   0x6b6b51f4, 0x4db26158, 0x5005713c, 0xedb88320, 0xf00f9344,
   0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278,
   0xbdbdf21c
};

unsigned int CBlzTool::blz_crc32(const void *source,
                             unsigned int length,
                             unsigned int initial_crc32)
{
   const unsigned char *buf = (const unsigned char *)source;
   unsigned int crc = initial_crc32 ^ 0xffffffffu;
   unsigned int i;

   if (length == 0) return 0;

   for (i = 0; i < length; ++i)
   {
      crc ^= buf[i];
      crc = blz_crctab_n[crc & 0x0f] ^ (crc >> 4);
      crc = blz_crctab_n[crc & 0x0f] ^ (crc >> 4);
   }

   return crc ^ 0xffffffffu;
}
unsigned long CBlzTool::get_uint32(const byte *p)
{
    return ((unsigned long) octet(p[0]) << 24) |
           ((unsigned long) octet(p[1]) << 16) |
           ((unsigned long) octet(p[2]) <<  8) |
           ((unsigned long) octet(p[3])      );
}
unsigned int CBlzTool::blz_max_packed_size(unsigned int length)
{
   /* return max compressed size */
   return length + length/8 + 64;
}
int CBlzTool::blz_getbit(BLZDEPACKDATA *ud)
{
   unsigned int bit;

   /* check if tag is empty */
   if (!ud->bitcount--)
   {
      /* load next tag */
      ud->tag = ud->source[0] + ((unsigned int)ud->source[1] << 8);
      ud->source += 2;
      ud->bitcount = 15;
   }

   /* shift bit out of tag */
   bit = (ud->tag >> 15) & 0x01;
   ud->tag <<= 1;

   return bit;
}

unsigned int CBlzTool::blz_getgamma(BLZDEPACKDATA *ud)
{
   unsigned int result = 1;

   /* input gamma2-encoded bits */
   do {
      result = (result << 1) + blz_getbit(ud);
   } while (blz_getbit(ud));

   return (result);
}

unsigned int CBlzTool::blz_depack(const void *source,
                              void *destination,
                              unsigned int depacked_length)
{
   BLZDEPACKDATA ud;
   unsigned int length = 1;

   /* check for length == 0 */
   if (depacked_length == 0) return 0;

   ud.source = (const unsigned char *)source;
   ud.destination = (unsigned char *)destination;
   ud.bitcount = 0;

   /* first byte verbatim */
   *ud.destination++ = *ud.source++;

   /* main decompression loop */
   while (length < depacked_length)
   {
      if (blz_getbit(&ud))
      {
	 /* input match length and position */
	 unsigned int len = blz_getgamma(&ud) + 2;
	 unsigned int pos = blz_getgamma(&ud) - 2;

	 pos = (pos << 8) + *ud.source++ + 1;

	 /* copy match */
	 {
	    const unsigned char *ppos = ud.destination - pos;
	    int i;
	    for (i = len; i > 0; --i) *ud.destination++ = *ppos++;
	 }

	 length += len;

      } else {

	 /* copy literal */
	 *ud.destination++ = *ud.source++;

	 length++;
      }
   }

   /* return decompressed length */
   return length;
}
