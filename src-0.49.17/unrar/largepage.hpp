#ifndef _RAR_LARGEPAGE_
#define _RAR_LARGEPAGE_

class LargePageAlloc
{
  private:
    #if defined(_MSC_VER) && (_MSC_VER < 1300) /* VC6 */
    static const wchar *LOCKMEM_SWITCH; /* def in .cpp (VC6_LP) */
    #else
    static constexpr const wchar *LOCKMEM_SWITCH=L"isetup_privilege_lockmem";
    #endif /* VC6 */


    void* new_large(size_t Size);
    bool delete_large(void *Addr);
#ifdef _WIN_ALL
    std::vector<void*> LargeAlloc;
    SIZE_T PageSize;
#endif
    bool UseLargePages;
  public:
    LargePageAlloc();
    void AllowLargePages(bool Allow);
    static bool IsPrivilegeAssigned();
    static bool AssignPrivilege();
    static bool AssignPrivilegeBySid(const std::wstring &Sid);
    static bool AssignConfirmation();

    static bool ProcessSwitch(CommandData *Cmd,const wchar *Switch)
    {
      if (Switch[0]==LOCKMEM_SWITCH[0])
      {
        size_t Length=wcslen(LOCKMEM_SWITCH);
        if (wcsncmp(Switch,LOCKMEM_SWITCH,Length)==0)
        {
          LargePageAlloc::AssignPrivilegeBySid(Switch+Length);
          return true;
        }
      }
      return false;
    }

#if !defined(_MSC_VER) || (_MSC_VER >= 1200)
    /* VC4 cannot parse member templates at all; both are
       caller-free, so dropping the declarations is safe. */
    template <class T> T* new_l(size_t Size,bool Clear=false)
    {
      T *Allocated=(T*)new_large(Size*sizeof(T));
      if (Allocated==nullptr)
        Allocated=Clear ? new T[Size]{} : new T[Size];
      return Allocated;
    }

    template <class T> void delete_l(T *Addr)
    {
      if (!delete_large(Addr))
        delete[] Addr;
    }
#endif /* VC4: no member templates */
};


#endif
