#ifndef AFX_OPENCAB_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_
#define AFX_OPENCAB_H__8AABAD86_FD1C_4CD7_884F_D803CA349B5F__INCLUDED_


#include "OpenArchive.h"
#include <vector>

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
	struct CAB_HEADER
	{
		DWORD sig;              // file signature 'MSCF' (CAB_SIGNATURE)
		DWORD csumHeader;       // header checksum (0 if not used)
		DWORD cbCabinet;        // cabinet file size
		DWORD csumFolders;      // folders checksum (0 if not used)
		DWORD coffFiles;        // offset of first CAB_ENTRY
		DWORD csumFiles;        // files checksum (0 if not used)
		WORD  version;          // cabinet version (CAB_VERSION)
		WORD  cFolders;         // number of folders
		WORD  cFiles;           // number of files
		WORD  flags;            // cabinet flags (CAB_FLAG_*)
		WORD  setID;            // cabinet set id
		WORD  iCabinet;         // zero-based cabinet number
	};
	struct CAB_FOLDER
	{
		DWORD coffCabStart;     // offset of folder data
		WORD  cCFData;          // ???
		WORD  typeCompress;     // compression type (tcomp* in FDI.h)
	};
	struct CAB_ENTRY
	{
		DWORD cbFile;           // uncompressed file size
		DWORD uoffFolderStart;  // file offset after decompression
		WORD  iFolder;          // file control id (CAB_FILE_*)
		WORD  date;             // file date stamp, as used by DOS
		WORD  time;             // file time stamp, as used by DOS
		WORD  attribs;          // file attributes (CAB_ATTRIB_*)

		char fnm[MAX_PATH];
	};

	bool doHeader(unsigned char* buf );
	bool read_header( CAB_ENTRY &hdr,unsigned char* buf );

	int iFiles;
	std::vector<WORD> folderComp;// per-folder compression types (tcompTYPE_*)
};

#endif