bool ExtractHardlink(CommandData *Cmd,const std::wstring &NameNew,const std::wstring &NameExisting)
{
  if (!FileExist(NameExisting))
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    uiMsg(UIERROR_NOLINKTARGET);
    ErrHandler.SetErrorCode(RARX_CREATE);
    return false;
  }
  CreatePath(NameNew,true,Cmd->DisableNames);

#ifdef _WIN_ALL
  /* VC6/retro: CreateHardLinkW is w2k+, load dynamically (VC6_CHL). */
  typedef BOOL (WINAPI *CHLFUNC)(LPCWSTR,LPCWSTR,LPSECURITY_ATTRIBUTES);
  HMODULE hKrnl=GetModuleHandle(L"kernel32.dll");
  CHLFUNC pChl=hKrnl==NULL ? NULL : (CHLFUNC)GetProcAddress(hKrnl,"CreateHardLinkW");
  bool Success=pChl!=NULL && pChl(NameNew.c_str(),NameExisting.c_str(),NULL)!=0;
  if (!Success)
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  return Success;
#elif defined(_UNIX)
  std::string NameExistingA,NameNewA;
  WideToChar(NameExisting,NameExistingA);
  WideToChar(NameNew,NameNewA);
  bool Success=link(NameExistingA.c_str(),NameNewA.c_str())==0;
  if (!Success)
  {
    uiMsg(UIERROR_HLINKCREATE,NameNew);
    ErrHandler.SysErrMsg();
    ErrHandler.SetErrorCode(RARX_CREATE);
  }
  return Success;
#else
  return false;
#endif
}

