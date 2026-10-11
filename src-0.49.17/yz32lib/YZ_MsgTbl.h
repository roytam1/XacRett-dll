//////////////////////////////////////////////////////////////////////
// YZ_MsgTbl.h
//
// Compatibility shim.
//
// The original YZ1 SDK shipped a message/number table used for the
// second argument of yzFileEv::MessEX/MessINF/MessYN/MessINPUT. XacRett
// never inspects those values (CYzEv / CKiYzEv ignore them), so an empty
// translation unit is behaviourally equivalent. Kept so the include in
// Yz1Tool.cpp resolves.
//////////////////////////////////////////////////////////////////////
#ifndef YZ32LIB_YZ_MSGTBL_H_
#define YZ32LIB_YZ_MSGTBL_H_

/* Message numbers are passed through opaquely; nothing to map here. */

#endif /* YZ32LIB_YZ_MSGTBL_H_ */