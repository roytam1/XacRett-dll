#ifndef SqxToolH
#define SqxToolH

#include "Tool.h"

#if !defined(__CX_TYPE_H)   
#include "unsqx/cx_type.h"
#endif

#if !defined(__CX_ERR_H)   
#include "unsqx/cx_err.h"
#endif

#if !defined(__SQX_VARS_H)
#include "unsqx/sqx_vars.h"
#endif

#if !defined(__CTAB_H)   
#include "unsqx/ctab.h"
#endif

#if !defined(__CX_AUDIO_H)   
#include "unsqx/cx_audio.h"
#endif

#if !defined(__SQX_OS_H)   
#include "unsqx/os/sqx_os.h"
#endif

#if !defined(__ARCIO_H) 
#include "unsqx/arcio.h"
#endif

#if !defined(__ARCIO_H) 
#include "unsqx/arcio.h"
#endif

#if !defined(__UARDE_H)   
#include "unsqx/uarde.h"
#endif

#if !defined(__AES_C_H)   
#include "unsqx/crypt/aes/aes_c.h"
#endif

#if !defined(__BF_CRYPT_H)
#include "unsqx/crypt/bf/bf_crypt.h"
#endif

#if !defined(__CRC32_H)
#include "unsqx/crc32.h"
#endif

class CSqxTool : public CTool  
{
/*********************************************************
Unsqx

C/C++ version Copyright (c) 1999 - 2002
Rainer Nausedat
Bahnhofstrasse 32
26316 Varel

All rights reserved.

Grant of License
================

This End-User License Agreement grants you the following limited, 
non-exclusive rights:

You may use the Software Product (Redistributable Code, Sample Code, 
Source Code and Design Specifications) free of charge in any of your 
Applications provided you comply with this End-UserLicense Agreement.

You may install and use the Software Product on as many computer you use 
solely for the purpose of developing applications which create, extract or 
manipulate files that are compatible with SQX file format.

Source Code
-----------
You may modify the source code located in the Software Product 壮 
"Source" folder to design, develop, and test your Application. You 
may also reproduce and distribute the Source Code in object code 
form along with any modifications you make to the Source Code, 
provided that the modifications do not create files which are not 
compatible with SQX file format. (see also Additional Source Code)

Disclaimer
----------
This software is provided 'as is' with no explcit or implied warranties
in respect of any properties, including, but not limited to, correctness 
and fitness for purpose.

*********************************************************/

//unsqx\内のコードはSQX書庫処理以外の目的で使用不可。
//Unsqxのソースコード「そのもの」はlibでしか配布できません。

public:
	CSqxTool() : CTool( "Sqx 展開" ){}
	bool IsType( const char* ext ) {return 0==stricmp(ext,"sqx");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

	char* GetPass(); //protectedからpublicに変換(^^;;;
};

#endif
