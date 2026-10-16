/*********************************************************
UARDE.C


C/C++ version Copyright (c) 1996 - 2002
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

*********************************************************/

#if !defined(__CX_TYPE_H)   
#include "cx_type.h"
#endif

#if !defined(__CX_ERR_H)   
#include "cx_err.h"
#endif

#if !defined(__ARCIO_H)   
#include "arcio.h"
#endif

#if !defined(__CTAB_H)   
#include "ctab.h"
#endif

#if !defined(__SQX_VARS_H)
#include "sqx_vars.h"
#endif

#if !defined(__UARDE_H)   
#include "uarde.h"
#endif

//- in sqx_main.c
extern void SetProgress(int iFinished);

UWORD32	CUR_MAX_DIC_SIZE;
UWORD32 uCurrPos;			
UWORD32 uWritePos;			
UBYTE	*SWindow = NULL;
UBYTE	*pImageBuf = NULL;

int InitDecodeLz(void)
{
	SWindow = malloc(MIN_MAX_DIC_SIZE);
	if( SWindow == NULL )
		return (int)FALSE;

	pImageBuf = malloc(MIN_MAX_DIC_SIZE);
	if( pImageBuf == NULL )
	{
		free(SWindow);
		return (int)FALSE;
	}
	return (int)TRUE;
}

void FreeDecodeLz(void)
{
	if( SWindow != NULL )
		free(SWindow);
	if( pImageBuf != NULL )
		free(pImageBuf);
}

void CopyString(unsigned uLen,unsigned uDist)
{
	PrevDists[uPrevDistIndex++ & 3] = uDist;
	
	while( uLen-- )
	{
		SWindow[uCurrPos] = SWindow[(uCurrPos - uDist) & (CUR_MAX_DIC_SIZE - 1)];
		if( ++uCurrPos >= CUR_MAX_DIC_SIZE )
			WriteDictionary();
		uCurrPos &= (CUR_MAX_DIC_SIZE - 1);
	}
}

/* Format-2.0 distance code: low 4 extra bits come from the per-block
   Low16 Huffman table when present (mirror of reference read_dist). */
static UWORD32 ReadDistCode20(void)
{
	UWORD32 uCode;
	UWORD32 uDist;
	UWORD32 uEBits;

	uCode = PEEKCODE(DistCTab);
	DUMPBITS(DistCTab.uLens[uCode]);
	uDist = BaseDTable[uCode];
	uEBits = BaseDBits[uCode];
	if( uEBits > 0 )
	{
		if( uHave16 != OPTION_NONE && uEBits >= 4 )
		{
			UWORD32 uHi = 0;
			UWORD32 uNumBits;
			if( uEBits > 4 )
			{
				uNumBits = uEBits - 4;
				uHi = NEEDBITS(uNumBits);
								DUMPBITS(uNumBits);
			}
			uCode = PEEKCODE(Low16CTab);
			DUMPBITS(Low16CTab.uLens[uCode]);
			uDist += (uHi << 4) + uCode;
					}
		else
		{
			uDist += NEEDBITS(uEBits);
			DUMPBITS(uEBits);
		}
	}

	return uDist + 1;
}

void DecodeFile(long lCompFlags,long lSolidFlag,UWORD32 uDecDicSize,long lArcerVer)
{
UWORD32			uCodeNum;
UWORD32			uLen;
UWORD32			uEBits;
UWORD32			uDist;
UWORD32			uMMBlock;
UWORD32			uMaxTreeCode;
UWORD32			uFourTrees;
UWORD32			uLastCode;
UBYTE			*pData;
UWORD32			uCount;
SQX_DELTA_HEAD	SqxDeltaHead;
SLONG32			lOneBlock;
SLONG64			lLastProgr;
SLONG64			l64OutStart;

	InitBitStream();
	uBTBlockPtr = 0;

	CUR_MAX_DIC_SIZE = uDecDicSize;
	SetBaseDistTabs((CUR_MAX_DIC_SIZE >> 10));

	uFmt20 = (lArcerVer >= 20) ? OPTION_SET : OPTION_NONE;

	/* The window is MIN_MAX_DIC_SIZE bytes; a larger (or empty, or
	   non-power-of-2) dictionary would index outside it. Refuse. */
	if( uDecDicSize == 0 || uDecDicSize > MIN_MAX_DIC_SIZE ||
	    (uDecDicSize & (uDecDicSize - 1)) != 0 )
	{
		ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
		return;
	}

	if( lSolidFlag == OPTION_NONE )
	{
		uLastLen = uLastDist = uPrevDistIndex = uWritePos = uCurrPos = 0;
		memset(PrevDists,0,sizeof(PrevDists));
	}

	//- ...guard...
	memset(&DeltaCtrl,0,sizeof(DELTA_CTRL));
		
	if( lCompFlags != OPTION_NONE )
	{
		pData = (UBYTE *)&IoStruct.usExtCompressor;

		for( uCount = 0; uCount < 2; uCount++ )
		{
			pData[uCount] = NEEDBITS(8);
			DUMPBITS(8);
			if( ArcStruct.ArchiveStatus != SQX_EC_OK )
				break;
		}
			
		if( (IoStruct.usExtCompressor & SQX_DELTA_TYPE_FLAG) != OPTION_NONE )
		{
			pData = (UBYTE *)&SqxDeltaHead;

			for( uCount = 0; uCount < sizeof(SQX_DELTA_HEAD); uCount++ )
			{
				pData[uCount] = NEEDBITS(8);
				DUMPBITS(8);
				if( ArcStruct.ArchiveStatus != SQX_EC_OK )
					break;
			}

			DeltaCtrl.lOffset = SqxDeltaHead.lStartOffset;
			DeltaCtrl.lOffsetSoFar = 0;
			DeltaCtrl.lNeedOfs = OPTION_SET;
			DeltaCtrl.uCodeDeltas = OPTION_SET;
			DeltaCtrl.uCoderNum = (UWORD32)SqxDeltaHead.cCoderNum;
			DeltaCtrl.uCoderType = (UWORD32)SqxDeltaHead.cCoderType;
		}
	}

	lLastProgr = 0;
	l64OutStart = ProgressStruct.l64Processed;

	lOneBlock = (SLONG32)(IoStruct.l64OrigSize / 100);
	if( lOneBlock < (1 << 15) )
		lOneBlock = (1 << 15);
		

	while( (IoStruct.l64OrigSize > 0) && (ArcStruct.ArchiveStatus == SQX_EC_OK)  )
	{
		
		if( ProgressStruct.l64Processed - lLastProgr >= lOneBlock )
		{
			SetProgress(0);
			lLastProgr = ProgressStruct.l64Processed;
		}
		
		if( uBTBlockPtr == 0 )
		{
			uMMBlock = 0;
			if( ReadTabs(&uMaxTreeCode,&uFourTrees,&SWindow[uCurrPos],&uMMBlock) == (int)FALSE )
			{
				if( ArcStruct.ArchiveStatus != SQX_EC_OK )
					ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
				break;
			}
			
			if( uMMBlock == OPTION_SET )
			{
				uCurrPos += uBTBlockPtr;
				IoStruct.l64OrigSize -= uBTBlockPtr;
				ProgressStruct.l64Processed += uBTBlockPtr;
				uBTBlockPtr = 0;
				if( uCurrPos >= CUR_MAX_DIC_SIZE )
					WriteDictionary();
				uCurrPos &= (CUR_MAX_DIC_SIZE - 1);
				continue;
			}
			uLastCode = uMaxTreeCode;
		}
		
		if( uFourTrees == OPTION_NONE )
		{
			uCodeNum = PEEKCODE(LitLenCTab[0]);
			DUMPBITS(LitLenCTab[0].uLens[uCodeNum]);
		}
		else
		{
			if( uLastCode < uMaxTreeCode + 1 )
			{
				uLastCode = uCodeNum = PEEKCODE(LitLenCTab[0]);
				DUMPBITS(LitLenCTab[0].uLens[uCodeNum]);
			}
			else
			{
				uLastCode = uCodeNum = PEEKCODE(LitLenCTab[1]);
				DUMPBITS(LitLenCTab[1].uLens[uCodeNum]);
			}
		}

		uBTBlockPtr--;

		if( uFmt20 != OPTION_NONE )
		{
						if( uCodeNum < 256 )
			{
				SWindow[uCurrPos] = (UBYTE)uCodeNum;
				if( ++uCurrPos >= CUR_MAX_DIC_SIZE )
					WriteDictionary();
				uCurrPos &= (CUR_MAX_DIC_SIZE - 1);
				IoStruct.l64OrigSize--;
				ProgressStruct.l64Processed++;
				continue;
			}
			else if( uCodeNum == 256 )
			{
				uDistState = uLastDist;
				CopyString(uLastLen,uLastDist);
				IoStruct.l64OrigSize -= uLastLen;
				uLen = uLastLen;
				ProgressStruct.l64Processed += uLen;
				continue;
			}
			else if( uCodeNum == SYM_COPY1 )
			{
				uDist = uDistState;
				if( uDist == 0 ||
				    (SLONG64)uDist > (ProgressStruct.l64Processed - l64OutStart) +
				                      (SLONG64)(uCurrPos - uWritePos) )
				{
					ArcStruct.ArchiveStatus = SQX_EC_BAD_FILE_FORMAT;
					break;
				}
				SWindow[uCurrPos] = SWindow[(uCurrPos - uDist) & (CUR_MAX_DIC_SIZE - 1)];
				if( ++uCurrPos >= CUR_MAX_DIC_SIZE )
					WriteDictionary();
				uCurrPos &= (CUR_MAX_DIC_SIZE - 1);
				IoStruct.l64OrigSize--;
				ProgressStruct.l64Processed++;
				continue;
			}
			else if( uCodeNum < LEN2_START2 )
			{
				uCodeNum -= 257;
				uLastDist = PrevDists[(uPrevDistIndex - uCodeNum) & 3];
				
				uCodeNum = PEEKCODE(RepCTab);
				DUMPBITS(RepCTab.uLens[uCodeNum]);

				if( uCodeNum == LEN_CODES - 1 )
				{
					uLastLen = 257 + NEEDBITS(14);
					DUMPBITS(14);
				}
				else
				{
					uLastLen = LenOffsets[uCodeNum];
					uEBits = LenEBits[uCodeNum];
					if( uEBits > 0 )
					{
						uLastLen += NEEDBITS(uEBits);
						DUMPBITS(uEBits);
					}
				
					uLastLen += 2;

					if( uLastDist > 0x7FFF )
						uLastLen++;
				}
				
				uDistState = uLastDist;
				IoStruct.l64OrigSize -= uLastLen;
				CopyString(uLastLen,uLastDist);
				uLen = uLastLen;
				ProgressStruct.l64Processed += uLen;
				continue;
			}
			else if( uCodeNum < LEN3_START2 )
			{
				uLen = 2;
				uCodeNum -= LEN2_START2;
				uDist = Len2Offsets[uCodeNum];
				uEBits = Len2EBits[uCodeNum];
				if( uEBits > 0 )
				{
					uDist += NEEDBITS(uEBits);
					DUMPBITS(uEBits);
				}

				uDist++;
				uDistState = uDist;
				IoStruct.l64OrigSize -= uLen;
				CopyString(uLen,uDist);
				ProgressStruct.l64Processed += uLen;
				continue;
			}
			else if( uCodeNum < LEN_START2 )
			{
				uLen = 3;
				uCodeNum -= LEN3_START2;
				uDist = Len3Offsets2[uCodeNum];
				uEBits = Len3EBits2[uCodeNum];
				if( uEBits > 0 )
				{
					uDist += NEEDBITS(uEBits);
					DUMPBITS(uEBits);
				}
				uDist++;
				uDistState = uDist;
				IoStruct.l64OrigSize -= uLen;
				CopyString(uLen,uDist);
				ProgressStruct.l64Processed += uLen;
				continue;
			}
			else
			{
				if( uCodeNum == NC2 - 1 )
				{
					uLen = 257 + NEEDBITS(14);
					DUMPBITS(14);
					uDist = ReadDistCode20();
				}
				else
				{
					uCodeNum -= LEN_START2;
					uLen = LenOffsets[uCodeNum];
					uEBits = LenEBits[uCodeNum];
					if( uEBits > 0 )
					{
						uLen += NEEDBITS(uEBits);
						DUMPBITS(uEBits);
					}
					uLen += 4;
					uDist = ReadDistCode20();
				}

				uDistState = uDist;
				IoStruct.l64OrigSize -= uLen;
				CopyString(uLen,uDist);
				ProgressStruct.l64Processed += uLen;
				continue;
			}
		}

		if( uCodeNum < 256 )
		{
			SWindow[uCurrPos] = (UBYTE)uCodeNum;
			if( ++uCurrPos >= CUR_MAX_DIC_SIZE )
				WriteDictionary();
			uCurrPos &= (CUR_MAX_DIC_SIZE - 1);
			IoStruct.l64OrigSize--;
			ProgressStruct.l64Processed++;
			continue;
		}
		else if( uCodeNum == 256 )
		{
			CopyString(uLastLen,uLastDist);
			IoStruct.l64OrigSize -= uLastLen;
			uLen = uLastLen;
		}
		else if( uCodeNum < 261 )
		{
			uCodeNum -= 256;
			uLastDist = PrevDists[(uPrevDistIndex - uCodeNum) & 3];
			
			uCodeNum = PEEKCODE(RepCTab);
			DUMPBITS(RepCTab.uLens[uCodeNum]);

			if( uCodeNum == LEN_CODES - 1 )
			{
				uLastLen = 257 + NEEDBITS(14);
				DUMPBITS(14);
			}
			else
			{
				uLastLen = LenOffsets[uCodeNum];
				uEBits = LenEBits[uCodeNum];
				if( uEBits > 0 )
				{
					uLastLen += NEEDBITS(uEBits);
					DUMPBITS(uEBits);
				}
			
				uLastLen += 2;

				if( uLastDist > MIN_LEN3 )
					uLastLen++;
			
				if( uLastDist > MIN_LEN4 )
					uLastLen++;
			}
			
			IoStruct.l64OrigSize -= uLastLen;
			CopyString(uLastLen,uLastDist);
			uLen = uLastLen;
		}
		else if( uCodeNum < 269 )
		{
			uLen = 2;
			uCodeNum -= LEN2_START;
			uDist = Len2Offsets[uCodeNum];
			uEBits = Len2EBits[uCodeNum];
			if( uEBits > 0 )
			{
				uDist += NEEDBITS(uEBits);
				DUMPBITS(uEBits);
			}

			uDist++;
			IoStruct.l64OrigSize -= uLen;
			CopyString(uLen,uDist);
		}
		else if( uCodeNum < 284 )
		{
			uLen = 3;
			uCodeNum -= LEN3_START;
			uDist = Len3Offsets[uCodeNum];
			uEBits = Len3EBits[uCodeNum];
			if( uEBits > 0 )
			{
				uDist += NEEDBITS(uEBits);
				DUMPBITS(uEBits);
			}
			uDist++;
			IoStruct.l64OrigSize -= uLen;
			CopyString(uLen,uDist);
		}
		else
		{
			if( uCodeNum == NC - 2 )
			{
				uLen = 257 + NEEDBITS(14);
				DUMPBITS(14);
				uCodeNum = PEEKCODE(DistCTab);
				DUMPBITS(DistCTab.uLens[uCodeNum]);
				uDist = BaseDTable[uCodeNum];
				uEBits = BaseDBits[uCodeNum];
				if( uEBits > 0 )
				{
					uDist += NEEDBITS(uEBits);
					DUMPBITS(uEBits);
				}

				uDist++;
			}
			else
			{
				uCodeNum -= LEN_START;
				uLen = LenOffsets[uCodeNum];
				uEBits = LenEBits[uCodeNum];
				if( uEBits > 0 )
				{
					uLen += NEEDBITS(uEBits);
					DUMPBITS(uEBits);
				}
				uLen += 4;
				uCodeNum = PEEKCODE(DistCTab);
				DUMPBITS(DistCTab.uLens[uCodeNum]);
				uDist = BaseDTable[uCodeNum];
				uEBits = BaseDBits[uCodeNum];
				if( uEBits > 0 )
				{
					uDist += NEEDBITS(uEBits);
					DUMPBITS(uEBits);
				}

				uDist++;

				if( uDist > MIN_LEN4 )
					uLen++;
			}

			IoStruct.l64OrigSize -= uLen;
			CopyString(uLen,uDist);
		}
		ProgressStruct.l64Processed += uLen;
	}
	WriteDictionary();
	SetProgress(1);
}
