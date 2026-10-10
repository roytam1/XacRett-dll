#include "stdafx.h"
#include "aPLibTool.h"

//Interface.
bool CaPLibTool::Check( const char* fname, unsigned long fsize ){
	const unsigned char* hdr = CTool::common_buf;
	unsigned long siz = (fsize>XACR_BUFSIZE ? XACR_BUFSIZE : fsize);
//--------------------------------------------------------------------//

	return ( siz>=8 && hdr[0]==0x41 && 
				hdr[1]==0x50 && hdr[2]==0x33 && 
				hdr[3]==0x32 && hdr[4]==0x18 &&
				hdr[5]==0x00 && hdr[6]==0x00 &&
				hdr[7]==0x00);
}

bool CaPLibTool::Extract( const char* packedname, const char* ddir ){
	char szOutFile[MAX_PATH];
	kiutil::getOriginalName( szOutFile,packedname,
		!strcmp(kiutil::pathExt(packedname),"apk") ? NULL : "unapk" );
	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(szOutFile))//追加 By Bonty
		return true;
	kiutil::pathMake(szOutFile);

	APKHEAD hdr;
	FILE* in=fopen(packedname,"rb");
	if(!in) return false;
	FILE* out=fopen(szOutFile,"wb");
	if(!out)
	{
		fclose(in);
		return false;
	}
	fread(&hdr,sizeof(APKHEAD),1,in);
	//一応チェック
	if(!(hdr.sig[0]==0x41 && 
				hdr.sig[1]==0x50 && hdr.sig[2]==0x33 && 
				hdr.sig[3]==0x32))
	{
		fclose(in);
		fclose(out);
		return false;
	}
	unsigned int srclen=filelength(fileno(in))-sizeof(APKHEAD);
	char *source=new char[srclen+1];
	if(!source)
	{
		fclose(in);
		fclose(out);
		return false;
	}
	fread(source,1,srclen,in);
	unsigned int dstlen=hdr.o_size;
	void *destination=new char[dstlen];
	if(!destination)
	{
		fclose(in);
		fclose(out);
		delete[] source;
		return false;
	}
	unsigned int ret=aP_depack_safe(source,srclen,destination,dstlen);
	fwrite(destination,dstlen,1,out);
	fclose(in);
	fclose(out);
	delete[] source;
	delete[] destination;
	return true;
}
int CaPLibTool::aP_getbit_safe(APDEPACKSAFEDATA *ud, unsigned int *result)
{
    unsigned int bit;

    /* check if tag is empty */
    if (!ud->bitcount--)
    {
        if (!ud->srclen--) return 0;
        
        /* load next tag */
        ud->tag = *ud->source++;
        ud->bitcount = 7;
    }

    /* shift bit out of tag */
    bit = (ud->tag >> 7) & 0x01;
    ud->tag <<= 1;

    *result = bit;

    return 1;
}
int CaPLibTool::aP_getgamma_safe(APDEPACKSAFEDATA *ud, unsigned int *result)
{
    unsigned int bit;
    unsigned int v = 1;

    /* input gamma2-encoded bits */
    do {

        if (!aP_getbit_safe(ud, &bit)) return 0;

        v = (v << 1) + bit;

        if (!aP_getbit_safe(ud, &bit)) return 0;

    } while (bit);

    *result = v;

    return 1;
}

unsigned int CaPLibTool::aP_depack_safe(const void *source,
                            unsigned int srclen,
                            void *destination,
                            unsigned int dstlen)
{
    APDEPACKSAFEDATA ud;
    unsigned int offs, len, R0, LWM, bit;
    int done;
    int i;

    if (!source || !destination) return APLIB_ERROR;

    ud.source = (const unsigned char *) source;
    ud.srclen = srclen;
    ud.destination = (unsigned char *) destination;
    ud.dstlen = dstlen;
    ud.bitcount = 0;

    LWM = 0;
    done = 0;

    /* first byte verbatim */
    if (!ud.srclen-- || !ud.dstlen--) return APLIB_ERROR;
    *ud.destination++ = *ud.source++;

    /* main decompression loop */
    while (!done)
    {
        if (!aP_getbit_safe(&ud, &bit)) return APLIB_ERROR;

        if (bit)
        {
            if (!aP_getbit_safe(&ud, &bit)) return APLIB_ERROR;

            if (bit)
            {
                if (!aP_getbit_safe(&ud, &bit)) return APLIB_ERROR;

                if (bit)
                {
                    offs = 0;

                    for (i = 4; i; i--)
                    {
                        if (!aP_getbit_safe(&ud, &bit)) return APLIB_ERROR;
                        offs = (offs << 1) + bit;
                    }

                    if (offs)
                    {
                        if (offs > (dstlen - ud.dstlen)) return APLIB_ERROR;

                        if (!ud.dstlen--) return APLIB_ERROR;

                        *ud.destination = *(ud.destination - offs);
                        ud.destination++;

                    } else {

                        if (!ud.dstlen--) return APLIB_ERROR;

                        *ud.destination++ = 0x00;
                    }

                    LWM = 0;

                } else {

                    if (!ud.srclen--) return APLIB_ERROR;

                    offs = *ud.source++;

                    len = 2 + (offs & 0x0001);

                    offs >>= 1;

                    if (offs)
                    {
                        if (offs > (dstlen - ud.dstlen)) return APLIB_ERROR;

                        if (len > ud.dstlen) return APLIB_ERROR;

                        ud.dstlen -= len;

                        for (; len; len--)
                        {
                            *ud.destination = *(ud.destination - offs);
                            ud.destination++;
                        }
                    } else done = 1;

                    R0 = offs;
                    LWM = 1;
                }

            } else {

                if (!aP_getgamma_safe(&ud, &offs)) return APLIB_ERROR;

                if ((LWM == 0) && (offs == 2))
                {
                    offs = R0;

                    if (!aP_getgamma_safe(&ud, &len)) return APLIB_ERROR;

                    if (offs > (dstlen - ud.dstlen)) return APLIB_ERROR;

                    if (len > ud.dstlen) return APLIB_ERROR;

                    ud.dstlen -= len;

                    for (; len; len--)
                    {
                        *ud.destination = *(ud.destination - offs);
                        ud.destination++;
                    }

                } else {

                    if (LWM == 0) offs -= 3; else offs -= 2;

                    if (!ud.srclen--) return APLIB_ERROR;

                    offs <<= 8;
                    offs += *ud.source++;

                    if (!aP_getgamma_safe(&ud, &len)) return APLIB_ERROR;

                    if (offs >= 32000) len++;
                    if (offs >= 1280) len++;
                    if (offs < 128) len += 2;

                    if (offs > (dstlen - ud.dstlen)) return APLIB_ERROR;

                    if (len > ud.dstlen) return APLIB_ERROR;

                    ud.dstlen -= len;

                    for (; len; len--)
                    {
                        *ud.destination = *(ud.destination - offs);
                        ud.destination++;
                    }

                    R0 = offs;
                }

                LWM = 1;
            }

        } else {

            if (!ud.srclen-- || !ud.dstlen--) return APLIB_ERROR;
            *ud.destination++ = *ud.source++;
            LWM = 0;
        }
    }

    return ud.destination - (unsigned char *) destination;
}
