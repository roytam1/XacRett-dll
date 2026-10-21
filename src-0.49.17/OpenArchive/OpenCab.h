#ifndef AFX_OPENCAB_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENCAB_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include "../CabDecode.h"

#define CAB_ATTRIB_READONLY  0x0001
#define CAB_ATTRIB_HIDDEN    0x0002
#define CAB_ATTRIB_SYSTEM    0x0004
#define CAB_ATTRIB_VOLUME    0x0008
#define CAB_ATTRIB_DIRECTORY 0x0010
#define CAB_ATTRIB_ARCHIVE   0x0020

class COpenCab : public COpenArchive
{
public: //-- COpenArchive 共通インターフェイス -----------

	COpenCab() : COpenArchive( )
	{COpenCab::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	// The listing uses the same parser as the extraction, so the two
	// can never disagree about what is in the archive.  The old code
	// had a parser of its own, and it only accepted an archive whose
	// cbCabinet field was exactly the size of the file, so most real
	// cabinets listed as empty.
	CabArchive	m_cab;			// the header as the engine read it
	int			m_index;		// the member being listed
};

#endif