// -*- Mode: C++ -*-

// Rar3Tool.cpp

#include "stdafx.h"
#include "Rar3Tool.h"
#include "unrar/dll.hpp"

#ifdef _DEBUG
	#pragma comment( lib, "unrar/debugUnRAR.lib" )
#else
	#pragma comment( lib, "unrar/UnRAR.lib" )
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// --- コールバック関数 ---
int CALLBACK unrar_callback(UINT msg, LONG usr_dat, LONG p1, LONG p2)
{
  CRar3Tool  *rartool = (CRar3Tool *)usr_dat;

  switch (msg)
  {
  case UCM_PROCESSDATA:
    return 0;

  case UCM_CHANGEVOLUME:
    return -1;  // 未対応

  case UCM_NEEDPASSWORD:
    {
      const char  *passwd = rartool->GetPassword();
      if (NULL == passwd)
        return -1;
      strncpy((char *)p1, passwd, (size_t)p2);
      return 0;
    }

  default:
    return -1;
  }
}


// --- コンストラクタ ---
CRar3Tool::CRar3Tool(void)
     : CTool("Rar 展開")
{
}


// --- デストラクタ ---
CRar3Tool::~CRar3Tool()
{
}


// --- 拡張子により対応形式かの判定を行う ---
bool CRar3Tool::IsType(const char *ext)
{
  return (0 == stricmp("rar", ext));
}


// --- 対応形式かの判定を行う ---
bool CRar3Tool::Check(const char *fname, unsigned long fsize)
{
  if (0 == check_archive(common_buf, MIN(XACR_BUFSIZE, fsize)))
    return true;
  else if (0 == check_sfx(common_buf, MIN(XACR_BUFSIZE, fsize), fname))
    return true;
  else
    return false;
}


// --- アーカイブを展開する ---
bool CRar3Tool::Extract(const char *fname, const char * /*ddir*/)
{
	RAROpenArchiveData  arc_data;
	memset(&arc_data, 0, sizeof(arc_data));

	char  arc_name[MAX_PATH * 2 + 1];
	strncpy(arc_name, fname, MAX_PATH * 2);
	arc_name[MAX_PATH * 2] = '\0';

	arc_data.ArcName = arc_name;
	arc_data.OpenMode = RAR_OM_EXTRACT;

	HANDLE  arc_h = RAROpenArchive(&arc_data);
	if (NULL == arc_h)
		return false;

	RARSetCallback(arc_h, unrar_callback, (LONG)this);

	RARHeaderData  hdr;
	FILE* out=NULL;
	while (0 == RARReadHeader(arc_h, &hdr))
	{
		if (m_Endflg) return true;
		//ワイルドカード処理
		if (!CheckStr(hdr.FileName))
		{
			RARProcessFile(arc_h, RAR_SKIP, NULL, hdr.FileName);
			continue;
		}
		char *name = kiutil::pathMake( hdr.FileName );
		out=fopen(name,"wb");
		fclose(out);
		if(out)
			RARProcessFile(arc_h, RAR_EXTRACT, NULL, hdr.FileName);
		else
			RARProcessFile(arc_h, RAR_SKIP, NULL, hdr.FileName);
		
	}

	RARCloseArchive(arc_h);

	return true;
}


// --- rar アーカイブかの判定を行う ---
int CRar3Tool::check_archive(const unsigned char *buf, size_t buf_size)
{
  static const unsigned char  RAR_SIGNATURE[]
    = { 0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00 };

  if (buf_size < sizeof(RAR_SIGNATURE))
    return 1;

  return memcmp(RAR_SIGNATURE, buf, sizeof(RAR_SIGNATURE));
}


// --- SFX かの判定を行う ---
int CRar3Tool::check_sfx(const unsigned char *buf,
                         size_t buf_size,
                         const char *fname)
{

  size_t  exe_size;

  {
    static const unsigned int  SIGNATURE_MZ = 0x5a4d;  // "MZ"

    const IMAGE_DOS_HEADER  *dos_hdr = (const IMAGE_DOS_HEADER *)common_buf;
    if (buf_size < sizeof(IMAGE_DOS_HEADER) ||
        SIGNATURE_MZ != dos_hdr->e_magic)
      return 1;

    const IMAGE_NT_HEADERS  *nt_hdrs
      = (const IMAGE_NT_HEADERS *)(common_buf + dos_hdr->e_lfanew);

    if (buf_size < dos_hdr->e_lfanew + sizeof(IMAGE_NT_HEADERS) ||
        IMAGE_NT_SIGNATURE != nt_hdrs->Signature)
      return 1;

    unsigned int  num_sections = nt_hdrs->FileHeader.NumberOfSections;
    if (0 == num_sections)
      return 1;

    unsigned int  last_section_pos = dos_hdr->e_lfanew + sizeof(IMAGE_NT_HEADERS)
      + sizeof(IMAGE_SECTION_HEADER) * (num_sections - 1);

    const IMAGE_SECTION_HEADER  *section_hdr
      = (const IMAGE_SECTION_HEADER *)(common_buf + last_section_pos);

    if (buf_size < last_section_pos + sizeof(IMAGE_SECTION_HEADER))
      return 1;

    exe_size = section_hdr->SizeOfRawData + section_hdr->PointerToRawData;
  }

  return (buf_size > exe_size) ?
    check_archive(buf + exe_size, buf_size - exe_size) : 1;
}
