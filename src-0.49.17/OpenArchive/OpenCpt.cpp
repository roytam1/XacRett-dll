#include "stdafx.h"
#include "OpenCpt.h"

bool COpenCpt::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	if(!COpenCpt::ReadCptHdr())
		return false;

	return Find_sub(lpSubInfo);
}

bool COpenCpt::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}

bool COpenCpt::Find_sub(INDIVIDUALINFO *lpSubInfo)
{
	static int now=0;
	unsigned char* hdr = &COpenCpt::common_buf[now*FILEHDRSIZE];

	if(++now>entries)
	{now=0;return false;}
	if( hdr[32] )
	{
		memcpy(path,hdr+1,hdr[0]);
		path[ hdr[0] ]=0x5C;
		path[ hdr[0] +1]=0;
		return Find_sub(lpSubInfo);
	}
	lstrcpy(lpSubInfo->szFileName,path);
	for(int i = 0;lpSubInfo->szFileName[i]!=0x00;i++);

	memcpy(&lpSubInfo->szFileName[i],hdr+1,hdr[0]);
	lpSubInfo->szFileName[ hdr[0]+i ]=0;
	//lstrcpy(lpSubInfo->szFileName,&hdr[1]);

	lpSubInfo->dwOriginalSize=(hdr[68]<<24) + (hdr[69]<<16) + (hdr[70]<<8) + hdr[71];
	lpSubInfo->dwCompressedSize=(hdr[76]<<24) + (hdr[77]<<16) + (hdr[78]<<8) + hdr[79];

	lstrcpy(lpSubInfo->szAttribute,"A---W");
	lstrcpy(lpSubInfo->szMode,
		lpSubInfo->dwCompressedSize<lpSubInfo->dwOriginalSize?"packed":"stored");
	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	//lstrcpy(lpSubInfo->szMode,"");
	return true;
}


bool COpenCpt::ReadCptHdr()
{
	BYTE tmp[300];
	isMacBin=false;
	if( 8!=fread(tmp,1,8,COpenCpt::fp) )
		return false;
	if( *tmp!=1 )
	{
		fseek(COpenCpt::fp,128,SEEK_SET);
		if( 8!=fread(tmp,1,8,COpenCpt::fp) )
			return false;
		if( *tmp!=1 )
			return false;
		isMacBin=true;
	}

	// INDEXまで跳ぶ
	fseek( COpenCpt::fp, 
		((tmp[4]<<24)|(tmp[5]<<16)|(tmp[6]<<8)|tmp[7])+(isMacBin?128:0),
		SEEK_SET );
	if( 7!=fread(tmp+8,1,7,COpenCpt::fp) )
		return false;

	DWORD hdrcrc = ((tmp[8]<<24) | (tmp[9]<<16) | (tmp[10]<<8) | tmp[11]);
	entries = ((tmp[12]<<8) | tmp[13]);
	BYTE commentsize = tmp[14];
	DWORD cpt_crc = updcrc(0xffffffff,tmp+12,3);

	if( commentsize!=fread(tmp,1,commentsize,COpenCpt::fp) )
		return false;
	cpt_crc = updcrc(cpt_crc,tmp,commentsize);

	/* INDEX用データ格納場所を確保
	delete [] cptindex;
	cptindex = new BYTE[entries*FILEHDRSIZE];
	if( cptindex==0 )return false;

	// INDEX取得
	BYTE* cptptr=cptindex;*/
	BYTE* cptptr=(BYTE*)COpenCpt::common_buf;
	for( WORD i=0; i!=entries; i++ )
	{
		*cptptr = fgetc(COpenCpt::fp);
		cpt_crc = updcrc(cpt_crc,cptptr,1);
		if( *cptptr&0x80 )
		{
			cptptr[F_FOLDER] = 1;
			*cptptr &= 0x3f;
		}
		else
			cptptr[F_FOLDER] = 0;

		if( *cptptr!=fread(cptptr+1,1,*cptptr,COpenCpt::fp) )
			return false;
		cpt_crc = updcrc(cpt_crc,cptptr+1,*cptptr);

		if( cptptr[F_FOLDER] )
		{
			if( 2!=fread(cptptr+F_FOLDERSIZE,1,2,COpenCpt::fp) )
				return false;
			cpt_crc = updcrc(cpt_crc,cptptr+F_FOLDERSIZE,2);
		}
		else
		{
			if( FILEHDRSIZE-F_VOLUME
				!= fread(cptptr+F_VOLUME,1,FILEHDRSIZE-F_VOLUME,COpenCpt::fp) )
				return false;
			cpt_crc = updcrc(cpt_crc,cptptr+F_VOLUME,FILEHDRSIZE-F_VOLUME);
		}
		cptptr += FILEHDRSIZE;
    }

	return hdrcrc==cpt_crc;
}