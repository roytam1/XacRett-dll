//////////////////////////////////////////////////////////////////////
// yzFileDe.cxx
//
// Compatibility shim.
//
// The original YZ1 SDK shipped the decoder implementation as a file that
// was #included directly by the consumer:
//
//     #include "../yz32lib/yzFileDe.cxx"
//
// in OpenYz1.h. The reconstruction supplies the same declarations in
// yz32lib.h plus a single translation unit (yz32lib.cpp) holding the
// definitions, so this file only has to pull the declarations in for
// sources that expect the .cxx to be self-contained.
//
// Include yz32lib.h first when the declarations are needed twice in one
// translation unit.
//////////////////////////////////////////////////////////////////////
#ifndef YZ32LIB_YZFILEDE_CXX_
#define YZ32LIB_YZFILEDE_CXX_

#include "yz32lib.h"

// XacRett names these types unqualified (yzFileEv, yzFileDecode,
// yzDecHead, yzDecodeHeader, yzFileStat, InEv, ...). Import them here
// rather than in yz32lib.h, so that yz32lib.cpp -- which also pulls in
// the flat yz32lib_v1.h -- keeps its own ::yz* references unambiguous.
using namespace nsYZ32v2;

#endif /* YZ32LIB_YZFILEDE_CXX_ */