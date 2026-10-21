#ifndef AFX_GCATOOL_H__025F5C18_B840_44DF_803C_D717F64E4F61__INCLUDED_
#define AFX_GCATOOL_H__025F5C18_B840_44DF_803C_D717F64E4F61__INCLUDED_

#include "Tool.h"

class CGcaTool : public CTool
{
//
// using
//
// GcaSDK 0.9g
//   http://www1.odn.ne.jp/~synsyr/
// Copyright (C) 2000,2001 SYN All Rights Reserved.
//

public:
	CGcaTool() : CTool( "Gca “WŠJ" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"gca");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );

  	char* pwd(){ return GetPassword(); }

	// set when the archive turned out to be encrypted and the password
	// was missing or wrong, so that the extraction can fail loudly
	// instead of quietly producing nothing
  	bool m_cryptoError;
};

// The password of an encrypted GCA archive.  It is either the raw
// text or, when the argument starts with a single '#', the hex SHA-1
// digest of a key file -- which is what gcac.exe's -k switch does.
// "##text" means the literal text.  The empty string means "no
// password", and the archive is then reported as needing a key.
std::string GcaSpellFromArg( const char* arg );

#endif
