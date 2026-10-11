/****************************************************************************
AES_COMP.H

C/C++ version Copyright (c) 2002
Rainer Nausedat
Bahnhofstrasse 32
26316 Varel / Germany

All rights reserved.

****************************************************************************/

#if !defined(__AES_COMP_H)   //- include this file only once per compilation
#define __AES_COMP_H


//- __AES_INT64__
//- If set, the compiler has native int64 support.

//- __KNOWN_COMPILER__
//- If set, the compiler should compile the sources without problems.


//- ##################################################################################

//- The below defines are meant only to make the source more readable
//-
//- _MSC_VER		MS VC compiler, native int64 support
//- _BCB50_VER		Borland C++ Builder 5.x, native int64 support
//- _BC50_VER		Borland C 5.x, *no* native int64 support

#if defined(_MSC_VER)
	#define __KNOWN_COMPILER__
	#define __AES_INT64__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x0550)
	#define __KNOWN_COMPILER__
	#define __AES_INT64__
	#define _BCB50_VER
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x460)
	#define __KNOWN_COMPILER__
	#define _BC50_VER
#endif

#endif //- #if !defined(__AES_COMP_H)
