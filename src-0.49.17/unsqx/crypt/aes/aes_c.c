/*********************************************************
AES_C.C


C/C++ version Copyright (c) 2002
Rainer Nausedat
Bahnhofstrasse 32
26316 Varel

All rights reserved.

-------------------------------------------------------------------------

 License Terms

 The free use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. you comply with the End-User License Agreement for
      this product. Please refer to lincense.txt for the
      complete End-User License Agreement.
   
   2. distributions of this source code include the above copyright 
      notice, this list of conditions, the complete End-User License 
      Agreement and the following disclaimer;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 

 Disclaimer

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.

***************************************************************************/

#if !defined(__CX_TYPE_H)   
#include "../../cx_type.h"
#endif

#if !defined(__SQX_VARS_H)   
#include "../../sqx_vars.h"
#endif

#if !defined(__RMD5_H)   
#include "../md5.h"
#endif

#if !defined(_AES_H)   
#include "aes.h"
#endif

#define	RIJNDAEL_DEF_128_BLOCKLEN	128

void InitRijndaelDecrypt128(void)
{
char	szBinKey[16];

	memset(&ArcStruct.AesCtx,0,sizeof(aes_ctx));
	memset(szBinKey,0,sizeof(szBinKey));
	GenerateMD5Key(ArcStruct.szPswd,szBinKey);
	aes_dec_key(szBinKey,16,&ArcStruct.AesCtx);
	memset(szBinKey,0,sizeof(szBinKey));
}

void RijndaelDecrypt128(UBYTE *pBuffer,UWORD32 uBufSize)
{
UWORD32	uCount;

	for( uCount = 0; uCount < uBufSize; uCount += RIJNDAEL_DEF_128_BLOCKLEN / 8)
		aes_dec_blk(&pBuffer[uCount],&pBuffer[uCount],&ArcStruct.AesCtx);
}

