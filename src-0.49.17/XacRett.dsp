# Microsoft Developer Studio Project File - Name="XacRett" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=XacRett - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "XacRett.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "XacRett.mak" CFG="XacRett - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "XacRett - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "XacRett - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "XacRett - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XACRETT_EXPORTS" /D "NO_GZCOMPRESS" /I "lhasa" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XACRETT_EXPORTS" /D "NO_GZCOMPRESS" /I "lhasa" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib lz32.lib /nologo /dll /machine:I386 /nodefaultlib:"libc.lib"

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XACRETT_EXPORTS" /D "NO_GZCOMPRESS" /I "lhasa" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /FR /I "lhasa" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib lz32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"DebugApp/Debug/XacRett.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "XacRett - Win32 Release"
# Name "XacRett - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\AceTool.cpp
# End Source File
# Begin Source File

SOURCE=.\AceBlock.cpp
# End Source File
# Begin Source File

SOURCE=.\AlzTool.cpp
# End Source File
# Begin Source File

SOURCE=.\aPLibTool.cpp
# End Source File
# Begin Source File

SOURCE=.\ArcTool.cpp
# End Source File
# Begin Source File

SOURCE=.\ArgTool.cpp
# End Source File
# Begin Source File

SOURCE=.\ArjTool.cpp
# End Source File
# Begin Source File

SOURCE=.\ArTool.cpp
# End Source File
# Begin Source File

SOURCE=.\AinCodec.cpp
# End Source File
# Begin Source File

SOURCE=.\AinTool.cpp
# End Source File
# Begin Source File

SOURCE=.\AsdTool.cpp
# End Source File
# Begin Source File

SOURCE=.\AskTool.cpp
# End Source File
# Begin Source File

SOURCE=.\AtoBTool.cpp
# End Source File
# Begin Source File

SOURCE=.\BelTool.cpp
# End Source File
# Begin Source File

SOURCE=.\BgaTool.cpp
# End Source File
# Begin Source File

SOURCE=.\BlzTool.cpp
# End Source File
# Begin Source File

SOURCE=.\BooTool.cpp
# End Source File
# Begin Source File

SOURCE=.\Bz2Tool.cpp
# End Source File
# Begin Source File

SOURCE=.\CabTool.cpp
# End Source File
# Begin Source File

SOURCE=.\CompactCTool.cpp
# End Source File
# Begin Source File

SOURCE=.\complib.cpp
# End Source File
# Begin Source File

SOURCE=.\CompressZTool.cpp
# End Source File
# Begin Source File

SOURCE=.\CpioTool.cpp
# End Source File
# Begin Source File

SOURCE=.\CptTool.cpp
# End Source File
# Begin Source File

SOURCE=.\DmgTool.cpp
# End Source File
# Begin Source File

SOURCE=.\DzTool.cpp
# End Source File
# Begin Source File

SOURCE=.\FishTool.cpp
# End Source File
# Begin Source File

SOURCE=.\FrzTool.cpp
# End Source File
# Begin Source File

SOURCE=.\GcaSDK\gca_api.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\GcaSDK\gca_arc.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\GcaSDK\gca_bwt.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\GcaSDK\gca_cod.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\GcaSDK\gca_numb.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\GcaSDK\gca_numv.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\GcaSDK\gca_stage.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\GcaTool.cpp
# End Source File
# Begin Source File

SOURCE=.\GzTool.cpp
# End Source File
# Begin Source File

SOURCE=.\HqxTool.cpp
# End Source File
# Begin Source File

SOURCE=.\ImpTool.cpp
# End Source File
# Begin Source File

SOURCE=.\IshTool.cpp
# End Source File
# Begin Source File

SOURCE=.\IsoTool.cpp
# End Source File
# Begin Source File

SOURCE=.\IsTool.cpp
# End Source File
# Begin Source File

SOURCE=.\JakTool.cpp
# End Source File
# Begin Source File

SOURCE=.\JamTool.cpp
# End Source File
# Begin Source File

SOURCE=.\Jc1Tool.cpp
# End Source File
# Begin Source File

SOURCE=.\kiutil.cpp
# End Source File
# Begin Source File

SOURCE=.\LzhDecoder2.cpp
# End Source File
# Begin Source File

SOURCE=.\LzhTool.cpp
# End Source File
# Begin Source File

SOURCE=.\LbrTool.cpp
# End Source File
# Begin Source File

SOURCE=.\MacBinTool.cpp
# End Source File
# Begin Source File

SOURCE=.\MimeTool.cpp
# End Source File
# Begin Source File

SOURCE=.\MSCompressTool.cpp
# End Source File
# Begin Source File

SOURCE=.\PackzTool.cpp
# End Source File
# Begin Source File

SOURCE=.\PitTool.cpp
# End Source File
# Begin Source File

SOURCE=.\QPakTool.cpp
# End Source File
# Begin Source File

SOURCE=.\Rar3Tool.cpp
# End Source File
# Begin Source File

SOURCE=.\RarTool.cpp
# End Source File
# Begin Source File

SOURCE=.\RezTool.cpp
# End Source File
# Begin Source File

SOURCE=.\SharTool.cpp
# End Source File
# Begin Source File

SOURCE=.\SplitCmdLine.cpp
# End Source File
# Begin Source File

SOURCE=.\SplTool.cpp
# End Source File
# Begin Source File

SOURCE=.\SqxTool.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\TarTool.cpp
# End Source File
# Begin Source File

SOURCE=.\Tool.cpp
# End Source File
# Begin Source File

SOURCE=.\UnixDecoderTool.cpp
# End Source File
# Begin Source File

SOURCE=.\XacRett.cpp
# End Source File
# Begin Source File

SOURCE=.\YncTool.cpp
# End Source File
# Begin Source File

SOURCE=.\Yz1Tool.cpp
# End Source File
# Begin Source File

SOURCE=.\ZacTool.cpp
# End Source File
# Begin Source File

SOURCE=.\ZipTool.cpp
# End Source File
# Begin Source File

SOURCE=.\ZipPpmd.cpp
# End Source File
# Begin Source File

SOURCE=.\ZooTool.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\AceTool.h
# End Source File
# Begin Source File

SOURCE=.\AceBlock.h
# End Source File
# Begin Source File

SOURCE=.\AlzTool.h
# End Source File
# Begin Source File

SOURCE=.\aPLibTool.h
# End Source File
# Begin Source File

SOURCE=.\ArcTool.h
# End Source File
# Begin Source File

SOURCE=.\ArgTool.h
# End Source File
# Begin Source File

SOURCE=.\ArjTool.h
# End Source File
# Begin Source File

SOURCE=.\ArTool.h
# End Source File
# Begin Source File

SOURCE=.\AinCodec.h
# End Source File
# Begin Source File

SOURCE=.\AinTool.h
# End Source File
# Begin Source File

SOURCE=.\AsdTool.h
# End Source File
# Begin Source File

SOURCE=.\AskTool.h
# End Source File
# Begin Source File

SOURCE=.\AtoBTool.h
# End Source File
# Begin Source File

SOURCE=.\BelTool.h
# End Source File
# Begin Source File

SOURCE=.\BgaTool.h
# End Source File
# Begin Source File

SOURCE=.\BlzTool.h
# End Source File
# Begin Source File

SOURCE=.\BooTool.h
# End Source File
# Begin Source File

SOURCE=.\Bz2Tool.h
# End Source File
# Begin Source File

SOURCE=.\CabTool.h
# End Source File
# Begin Source File

SOURCE=.\CompactCTool.h
# End Source File
# Begin Source File

SOURCE=.\complib.h
# End Source File
# Begin Source File

SOURCE=.\CompressZTool.h
# End Source File
# Begin Source File

SOURCE=.\CpioTool.h
# End Source File
# Begin Source File

SOURCE=.\CptTool.h
# End Source File
# Begin Source File

SOURCE=.\DmgTool.h
# End Source File
# Begin Source File

SOURCE=.\DzTool.h
# End Source File
# Begin Source File

SOURCE=.\FishTool.h
# End Source File
# Begin Source File

SOURCE=.\FrzTool.h
# End Source File
# Begin Source File

SOURCE=.\GcaTool.h
# End Source File
# Begin Source File

SOURCE=.\GzTool.h
# End Source File
# Begin Source File

SOURCE=.\HqxTool.h
# End Source File
# Begin Source File

SOURCE=.\ImpTool.h
# End Source File
# Begin Source File

SOURCE=.\IshTool.h
# End Source File
# Begin Source File

SOURCE=.\IsoTool.h
# End Source File
# Begin Source File

SOURCE=.\IsTool.h
# End Source File
# Begin Source File

SOURCE=.\JakTool.h
# End Source File
# Begin Source File

SOURCE=.\JamTool.h
# End Source File
# Begin Source File

SOURCE=.\Jc1Tool.h
# End Source File
# Begin Source File

SOURCE=.\kiutil.h
# End Source File
# Begin Source File

SOURCE=.\LzhDecoder2.h
# End Source File
# Begin Source File

SOURCE=.\LzhTool.h
# End Source File
# Begin Source File

SOURCE=.\LbrTool.h
# End Source File
# Begin Source File

SOURCE=.\MacBinTool.h
# End Source File
# Begin Source File

SOURCE=.\MemmapFile.h
# End Source File
# Begin Source File

SOURCE=.\MimeTool.h
# End Source File
# Begin Source File

SOURCE=.\MSCompressTool.h
# End Source File
# Begin Source File

SOURCE=.\PackzTool.h
# End Source File
# Begin Source File

SOURCE=.\PitTool.h
# End Source File
# Begin Source File

SOURCE=.\QPakTool.h
# End Source File
# Begin Source File

SOURCE=.\Rar3Tool.h
# End Source File
# Begin Source File

SOURCE=.\RarTool.h
# End Source File
# Begin Source File

SOURCE=.\RezTool.h
# End Source File
# Begin Source File

SOURCE=.\SharTool.h
# End Source File
# Begin Source File

SOURCE=.\SplitCmdLine.h
# End Source File
# Begin Source File

SOURCE=.\SplTool.h
# End Source File
# Begin Source File

SOURCE=.\SqxTool.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\TarTool.h
# End Source File
# Begin Source File

SOURCE=.\Tool.h
# End Source File
# Begin Source File

SOURCE=.\UnixDecoderTool.h
# End Source File
# Begin Source File

SOURCE=.\XacRett.h
# End Source File
# Begin Source File

SOURCE=.\YncTool.h
# End Source File
# Begin Source File

SOURCE=.\Yz1Tool.h
# End Source File
# Begin Source File

SOURCE=.\ZacTool.h
# End Source File
# Begin Source File

SOURCE=.\ZipTool.h
# End Source File
# Begin Source File

SOURCE=.\ZooTool.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\XacRett.rc
# End Source File
# End Group
# Begin Group "zlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\zlib\adler32.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\crc32.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\crc32.h
# End Source File
# Begin Source File

SOURCE=.\zlib\gzclose.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\gzlib.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\gzread.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\gzguts.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inffast.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\inffast.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inffixed.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inflate.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\inflate.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inftrees.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\zlib\uncompr.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\zconf.h
# End Source File
# Begin Source File

SOURCE=.\zlib\zlib.h
# End Source File
# Begin Source File

SOURCE=.\zlib\zutil.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\zlib\zutil.h
# End Source File
# End Group
# Begin Group "OpenArchive"

# PROP Default_Filter "cpp;h"
# Begin Group "SorceFile"

# PROP Default_Filter "cpp"
# Begin Source File

SOURCE=.\LzxTool.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAce.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAlz.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenaPLib.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAin.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAr.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenArc.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenArchive.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenArg.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenArj.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAsd.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAsk.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAtoB.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBel.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBga.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBlz.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBoo.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBz2.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCab.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCompactC.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCompressZ.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCpio.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCpt.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenDmg.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenDummy.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenDz.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenFish.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenFrz.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenGca.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenGz.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenHqx.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenImp.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenIs.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenIsh.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenJak.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenJam.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenJc1.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenLbr.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenLzh.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenLzx.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenMacBin.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenMime.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenMSCompress.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenPackz.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenQPak.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenRar.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenRez.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenShar.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenSpl.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenSqx.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenTar.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenUnixDecoder.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenYnc.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenYz1.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenZac.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenZip.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenZoo.cpp
# End Source File
# End Group
# Begin Group "HeaderFile"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\LzxTool.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAce.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAlz.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenaPLib.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAin.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAr.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenArc.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenArchive.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenArg.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenArj.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAsd.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAsk.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenAtoB.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBel.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBga.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBlz.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBoo.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenBz2.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCab.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCompactC.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCompressZ.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCpio.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenCpt.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenDmg.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenDummy.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenDz.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenFish.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenFrz.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenGca.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenGz.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenHqx.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenImp.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenIs.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenIsh.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenJak.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenJam.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenJc1.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenLbr.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenLzh.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenLzx.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenMacBin.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenMime.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenMSCompress.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenPackz.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenQPak.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenRar.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenRez.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenShar.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenSpl.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenSqx.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenTar.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenUnixDecoder.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenYnc.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenYz1.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenZac.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenZip.h
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenZoo.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\OpenArchive\OpenIso.cpp
# End Source File
# Begin Source File

SOURCE=.\OpenArchive\OpenIso.h
# End Source File
# End Group
# Begin Group "unsqx"

# PROP Default_Filter ""
# Begin Group "Source"

# PROP Default_Filter "c"
# Begin Source File

SOURCE=.\unsqx\arcio.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\ctab.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\cx_audio.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\fdi\fdi.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\jcalg1lib\jcalg1.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\mm_coder.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\rcoder.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\sqx_dec.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\sqx_os.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\sqx_vars.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\SQXcrc32.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\t_coder.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\uarde.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\WIN32_INC.C
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\yz32lib\yz32lib_v1.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\ZData\zdata.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\ZData\zdatas.h
# End Source File
# End Group
# Begin Group "Header"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\unsqx\arcio.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\config.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\crc32.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\ctab.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\cx_audio.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\cx_dino.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\cx_err.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\cx_type.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\mm_coder.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\rcoder.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\sqx_h.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\sqx_vars.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\t_coder.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\uarde.h
# End Source File
# End Group
# Begin Group "os"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\unsqx\os\sqx_os.h
# End Source File
# End Group
# Begin Group "crypt"

# PROP Default_Filter ""
# Begin Group "aes"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\unsqx\crypt\aes\aes.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\aes\aes_c.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\aes\aes_c.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\aes\aes_comp.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\aes\aescrypt.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\aes\aeskey.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\aes\aesopt.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\aes\aestab.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\aes\uitypes.h
# End Source File
# End Group
# Begin Group "bf"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\unsqx\crypt\bf\bf_crypt.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\bf\bf_crypt.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\unsqx\crypt\md5.h
# End Source File
# Begin Source File

SOURCE=.\unsqx\crypt\md5c.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# End Group
# End Group
# Begin Group "libbzip2"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\libbzip2\blocksort.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libbzip2\bzlib.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libbzip2\bzlib.h
# End Source File
# Begin Source File

SOURCE=.\libbzip2\bzlib_private.h
# End Source File
# Begin Source File

SOURCE=.\libbzip2\crctable.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libbzip2\decompress.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libbzip2\huffman.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\libbzip2\randtable.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "pkdcl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\pkdcl\pkdcl.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\pkdcl\pkdcl.h
# End Source File
# End Group
# Begin Group "lzma"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\lzma\LzmaDec.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\lzma\LzmaDec.h
# End Source File
# Begin Source File

SOURCE=.\lzma\7zTypes.h
# End Source File
# Begin Source File

SOURCE=.\lzma\Precomp.h
# End Source File
# Begin Source File

SOURCE=.\lzma\Lzma2Dec.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\lzma\Lzma2Dec.h
# End Source File
# End Group
# Begin Group "wzaes"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\wzaes\wzaes.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\wzaes\wzaes.h
# End Source File
# Begin Source File

SOURCE=.\wzaes\izsha1.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\wzaes\izsha1.h
# End Source File
# Begin Source File

SOURCE=.\wzaes\iz_arm_caps.h
# End Source File
# Begin Source File

SOURCE=.\wzaes\izsha1_ni.h
# End Source File
# Begin Source File

SOURCE=.\wzaes\iz_aes_x86.h
# End Source File
# Begin Source File

SOURCE=.\wzaes\iz_arm_crypto.h
# End Source File
# Begin Source File

SOURCE=.\wzaes\ae3_impl.h
# End Source File
# End Group
# Begin Group "ppmd8"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\ppmd8\ppmd8.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\ppmd8\ppmd8dec.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\ppmd8\ppmd8.h
# End Source File
# Begin Source File

SOURCE=.\ppmd8\ppmd.h
# End Source File
# End Group
# Begin Group "xceedbwt"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\xceedbwt\xceedbwt.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=.\xceedbwt\xceedbwt.h
# End Source File
# End Group
# Begin Group "YZ1"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\yz32lib\yz32lib.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\yz32lib\yzDecode.cxx

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\yz32lib\yzFileDe.cxx

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# End Group
# Begin Group "lhasa"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\lhasa\lha_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\null_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lh1_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lh5_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lh6_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lh7_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lhx_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lk7_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lz5_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lzs_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\pm1_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\pm2_decoder.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\crc16.c

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\LhasaBridge.cpp

!IF  "$(CFG)" == "XacRett - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "XacRett - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lhasa\lha_decoder.h
# End Source File
# Begin Source File

SOURCE=.\lhasa\crc16.h
# End Source File
# Begin Source File

SOURCE=.\lhasa\LhasaBridge.h
# End Source File
# Begin Source File

SOURCE=.\lhasa\inttypes.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\XacRett.def
# End Source File
# End Target
# End Project
