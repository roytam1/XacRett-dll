#ifndef aPLibToolH
#define aPLibToolH

#include "Tool.h"
/*
aPLib is free for non-commercial use
(if nobody charges money for products that use it).
If you use aPLib in a product, an acknowledgement would be appreciated,
e.g. by adding something like the following to the documentation:

    This product uses the aPLib compression library,
    Copyright c 1998-2004 by Jorgen Ibsen, All Rights Reserved.
    For more information, please visit: http://www.ibsensoftware.com/

Using aPLib in a commercial product requires registration
--please check www.ibsensoftware.com for more information.

The definition of a product that _uses_ aPLib is a product
that contains any part of aPLib EXCEPT the decompression code.
So you can sell a product that uses data compressed with aPLib without registering,
but NOT a product that contains any part of the compression code.

You may not distribute aPLib without all of the files.

You may not edit or reverse engineer any of the files
(except the header files and the decompression code, which you may edit as long as you do not remove the copyright notice).

You may not sell aPLib, or any part of it, for money
(except for charging for the media).

This software is provided "as is".
In no event shall I, the author,
be liable for any kind of loss or damage arising out of the use,
abuse or the inability to use this software.
USE IT ENTIRELY AT YOUR OWN RISK!

This software comes without any kind of warranty, either expressed or implied,
including, but not limited to the implied warranties of merchantability or fitness for any particular purpose.

If you do not agree with these terms or if your jurisdiction does not allow the exclusion of warranty and liability as stated above you are NOT allowed to use this software at all.
*/

//要するに圧縮部分を商用利用する場合は有料。XacRettには関係なし。

class CaPLibTool : public CTool
{
public:
	CaPLibTool() : CTool( "aPLib 展開" ){}
	bool IsType( const char* ext )
		{return 0==stricmp(ext,"apk");}

	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

private:
	typedef struct {
		unsigned char	sig[4];
		DWORD			ver;//?
		DWORD			d;//?
		DWORD			d2;//?
		DWORD			o_size;//?
		DWORD			crc;//?
	} APKHEAD;
	typedef struct {
		const unsigned char *source;
		unsigned int srclen;
		unsigned char *destination;
		unsigned int dstlen;
		unsigned int tag;
		unsigned int bitcount;
	} APDEPACKSAFEDATA;

	unsigned int aP_depack_safe(const void *source,
                            unsigned int srclen,
                            void *destination,
                            unsigned int dstlen);

	static int aP_getbit_safe(APDEPACKSAFEDATA *ud, unsigned int *result);
	static int aP_getgamma_safe(APDEPACKSAFEDATA *ud, unsigned int *result);
};

#define APLIB_ERROR (-1)

#endif
