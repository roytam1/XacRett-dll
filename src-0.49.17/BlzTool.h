#ifndef AFX_BlzTOOL_H__F143A063_398D_11D4_8D96_87647745DB39__INCLUDED_
#define AFX_BlzTOOL_H__F143A063_398D_11D4_8D96_87647745DB39__INCLUDED_

#include "Tool.h"
/*
 * blzpack  -  BriefLZ example
 *
 * Copyright (c) 2002-2004 by Joergen Ibsen / Jibz
 * All Rights Reserved
 *
 * http://www.ibsensoftware.com/
 *
 * This software is provided 'as-is', without any express
 * or implied warranty.  In no event will the authors be
 * held liable for any damages arising from the use of
 * this software.
 *
 * Permission is granted to anyone to use this software
 * for any purpose, including commercial applications,
 * and to alter it and redistribute it freely, subject to
 * the following restrictions:
 *
 * 1. The origin of this software must not be
 *    misrepresented; you must not claim that you
 *    wrote the original software. If you use this
 *    software in a product, an acknowledgment in
 *    the product documentation would be appreciated
 *    but is not required.
 *
 * 2. Altered source versions must be plainly marked
 *    as such, and must not be misrepresented as
 *    being the original software.
 *
 * 3. This notice may not be removed or altered from
 *    any source distribution.
 */
class CBlzTool : public CTool
{
public:
	CBlzTool() : CTool( "BriefLZ “WŠJ" ){}
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"blz");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
private:
	/* internal data structure */
	typedef struct {
	   const unsigned char *source;
	   unsigned char *destination;
	   unsigned int tag;
	   unsigned int bitcount;
	} BLZDEPACKDATA;
	
	static const unsigned int blz_crctab_n[16];

	static unsigned long get_uint32(const byte *p);
	static unsigned int blz_max_packed_size(unsigned int length);
	static int blz_getbit(BLZDEPACKDATA *ud);
	static unsigned int blz_getgamma(BLZDEPACKDATA *ud);
	unsigned int blz_depack(const void *source,
                              void *destination,
                              unsigned int depacked_length);
	unsigned int blz_crc32(const void *source,
                             unsigned int length,
                             unsigned int initial_crc32);
};

#endif
