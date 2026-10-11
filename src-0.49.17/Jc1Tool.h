#ifndef Jc1ToolH
#define Jc1ToolH

#include "Tool.h"
#ifndef _DEBUG
/*
JCALG1 Compression Library
Copyright (C) 1999-2002 by Jeremy Collake

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 

This program may be used freely in any applications as long as
the following two criteria are met:
        (a). Notify the author of JCALG1's use and in what application.
        (b). Show notification of JCALG1's usage in your program or
             program documentation along with a link to the authors
             webpage.
////////////////////////////////////////////
Usage of JCALG1 is here:
http://www.collakesoftware.com/jcalg1.htm
*/

#pragma comment(lib,"jcalg1lib/Jcalg1_static.lib")

extern "C" DWORD _stdcall JCALG1_GetUncompressedSizeOfCompressedBlock(
	const void *pBlock);

extern "C" DWORD _stdcall JCALG1_Decompress_Fast(
	const void *Source,
	void *Destination);

class CJc1Tool : public CTool
{
public:
	CJc1Tool() : CTool( "Jcalg1 “WŠJ" ){}
	bool IsType( const char* ext ){
		if(!strcmp(ext,"jcalg1")||!strcmp(ext,"jc")||!strcmp(ext,"jc1"))
			return true;
		else
			return false;
	}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );	
};
#endif
#endif
