# Microsoft Developer Studio Project File - Name="vidsend" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=vidsend - Win32 camparty debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "vidsend.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "vidsend.mak" CFG="vidsend - Win32 camparty debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "vidsend - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "vidsend - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "vidsend - Win32 Newmeet Release" (based on "Win32 (x86) Application")
!MESSAGE "vidsend - Win32 Newmeet Debug" (based on "Win32 (x86) Application")
!MESSAGE "vidsend - Win32 Newmeet Release Inglese" (based on "Win32 (x86) Application")
!MESSAGE "vidsend - Win32 Release StandAlone" (based on "Win32 (x86) Application")
!MESSAGE "vidsend - Win32 Camparty Release" (based on "Win32 (x86) Application")
!MESSAGE "vidsend - Win32 camparty debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "vidsend - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_LINGUA_ITALIANA" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL" /d "_LINGUA_ITALIANA"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib ws2_32.lib version.lib strmiids.lib d3dx8.lib d3d8.lib d3dxd.lib Dxerr8.lib uuid.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "vidsend - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_LINGUA_ITALIANA" /D "_STANDALONE_MODE" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x410 /d "_DEBUG" /d "_AFXDLL" /d "_LINGUA_ITALIANA"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib ws2_32.lib version.lib strmiids.lib d3dx8.lib d3d8.lib d3dxd.lib Dxerr8.lib uuid.lib ole32.lib oleaut32.lib iphlpapi.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /map

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "vidsend___Win32_Newmeet_Release"
# PROP BASE Intermediate_Dir "vidsend___Win32_Newmeet_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Newmeet_Release"
# PROP Intermediate_Dir "Newmeet_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /D "_LINGUA_ITALIANA" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE"
# ADD RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE" /d "_LINGUA_ITALIANA"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib ws2_32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib version.lib /nologo /subsystem:windows /machine:I386 /out:"Newmeet_Release/nmhc.exe"

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "vidsend___Win32_Newmeet_Debug"
# PROP BASE Intermediate_Dir "vidsend___Win32_Newmeet_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Newmeet_Debug"
# PROP Intermediate_Dir "Newmeet_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /D "_LINGUA_ITALIANA" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x410 /d "_DEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE" /d "_LINGUA_ITALIANA"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib ws2_32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib version.lib /nologo /subsystem:windows /debug /machine:I386 /out:"Newmeet_Debug/nmhc.exe" /pdbtype:sept

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Release Inglese"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "vidsend___Win32_Newmeet_Release_Inglese"
# PROP BASE Intermediate_Dir "vidsend___Win32_Newmeet_Release_Inglese"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Newmeet_Release_Inglese"
# PROP Intermediate_Dir "Newmeet_Release_Inglese"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /D "_LINGUA_INGLESE" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE"
# ADD RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE" /d "_LINGUA_INGLESE"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib ws2_32.lib /nologo /subsystem:windows /machine:I386 /out:"Newmeet_Release/nmvidsend.exe"
# ADD LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib version.lib /nologo /subsystem:windows /machine:I386 /out:"Newmeet_Release_inglese/nmvidsend.exe"

!ELSEIF  "$(CFG)" == "vidsend - Win32 Release StandAlone"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "vidsend___Win32_Release_StandAlone"
# PROP BASE Intermediate_Dir "vidsend___Win32_Release_StandAlone"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_StandAlone"
# PROP Intermediate_Dir "Release_StandAlone"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_LINGUA_ITALIANA" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_LINGUA_ITALIANA" /D "_STANDALONE_MODE" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL" /d "_LINGUA_ITALIANA" /d "_STANDALONE_MODE"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib ws2_32.lib version.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib ws2_32.lib version.lib strmiids.lib d3dx8.lib d3d8.lib d3dxd.lib Dxerr8.lib iphlpapi.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "vidsend - Win32 Camparty Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "vidsend___Win32_Camparty_Release"
# PROP BASE Intermediate_Dir "vidsend___Win32_Camparty_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Camparty_Release"
# PROP Intermediate_Dir "Camparty_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /D "_LINGUA_ITALIANA" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /D "_LINGUA_ITALIANA" /D "_CAMPARTY_MODE" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE" /d "_LINGUA_ITALIANA"
# ADD RSC /l 0x410 /d "NDEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE" /d "_LINGUA_ITALIANA" /d "_CAMPARTY_MODE"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib version.lib /nologo /subsystem:windows /machine:I386 /out:"Newmeet_Release/nmvidsend.exe"
# ADD LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib version.lib /nologo /subsystem:windows /machine:I386 /out:"camparty_Release/camparty.exe"

!ELSEIF  "$(CFG)" == "vidsend - Win32 camparty debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "vidsend___Win32_camparty_debug0"
# PROP BASE Intermediate_Dir "vidsend___Win32_camparty_debug0"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "camparty_debug"
# PROP Intermediate_Dir "camparty_debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /D "_LINGUA_ITALIANA" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /Gm /GX /ZI /Od /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_NEWMEET_MODE" /D "_LINGUA_ITALIANA" /D "_CAMPARTY_MODE" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x410 /d "_DEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE" /d "_LINGUA_ITALIANA"
# ADD RSC /l 0x410 /d "_NDEBUG" /d "_AFXDLL" /d "_NEWMEET_MODE" /d "_LINGUA_ITALIANA" /d "_CAMPARTY_MODE"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib version.lib /nologo /subsystem:windows /debug /machine:I386 /out:"Newmeet_Debug/nmvidsend.exe" /pdbtype:sept
# ADD LINK32 vfw32.lib winmm.lib msacm32.lib rasapi32.lib version.lib /nologo /subsystem:windows /debug /machine:I386 /out:"camparty_Debug/camparty.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "vidsend - Win32 Release"
# Name "vidsend - Win32 Debug"
# Name "vidsend - Win32 Newmeet Release"
# Name "vidsend - Win32 Newmeet Debug"
# Name "vidsend - Win32 Newmeet Release Inglese"
# Name "vidsend - Win32 Release StandAlone"
# Name "vidsend - Win32 Camparty Release"
# Name "vidsend - Win32 camparty debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CGif.cpp
# End Source File
# Begin Source File

SOURCE=.\ChildFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\CJPEG.cpp
# End Source File
# Begin Source File

SOURCE=.\DigitalText.cpp
# End Source File
# Begin Source File

SOURCE=.\ErrorObject.cpp
# End Source File
# Begin Source File

SOURCE=.\font.cpp
# End Source File
# Begin Source File

SOURCE=.\Image.cpp
# End Source File
# Begin Source File

SOURCE=.\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\oleobject.cpp
# End Source File
# Begin Source File

SOURCE=.\oleobjects.cpp
# End Source File
# Begin Source File

SOURCE=.\picture.cpp
# End Source File
# Begin Source File

SOURCE=.\ping.cpp
# End Source File
# Begin Source File

SOURCE=.\richtext.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\vidsend.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsend.rc
# End Source File
# Begin Source File

SOURCE=.\vidsenddialog.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsendDoc.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsendLog.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsendSerial.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsendSet.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsendSockets.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsendTime.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsendVideo.cpp
# End Source File
# Begin Source File

SOURCE=.\vidsendView.cpp
# End Source File
# Begin Source File

SOURCE=.\webbrowser2.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ChildFrm.h
# End Source File
# Begin Source File

SOURCE=.\Image.h
# End Source File
# Begin Source File

SOURCE=.\MainFrm.h
# End Source File
# Begin Source File

SOURCE=.\oleobject.h
# End Source File
# Begin Source File

SOURCE=.\oleobjects.h
# End Source File
# Begin Source File

SOURCE=.\picture.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h

!IF  "$(CFG)" == "vidsend - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=vidsend
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=vidsend
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Release"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=nmhc
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Debug"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=nmhc
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Release Inglese"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=nmvidsend
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Release StandAlone"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=vidsend
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Camparty Release"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=camparty
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 camparty debug"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Making help include file...
TargetName=camparty
InputPath=.\Resource.h

"hlp\$(TargetName).hm" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	echo. >"hlp\$(TargetName).hm" 
	echo // Commands (ID_* and IDM_*) >>"hlp\$(TargetName).hm" 
	makehm ID_,HID_,0x10000 IDM_,HIDM_,0x10000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Prompts (IDP_*) >>"hlp\$(TargetName).hm" 
	makehm IDP_,HIDP_,0x30000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Resources (IDR_*) >>"hlp\$(TargetName).hm" 
	makehm IDR_,HIDR_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Dialogs (IDD_*) >>"hlp\$(TargetName).hm" 
	makehm IDD_,HIDD_,0x20000 resource.h >>"hlp\$(TargetName).hm" 
	echo. >>"hlp\$(TargetName).hm" 
	echo // Frame Controls (IDW_*) >>"hlp\$(TargetName).hm" 
	makehm IDW_,HIDW_,0x50000 resource.h >>"hlp\$(TargetName).hm" 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\richtext.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\vidsend.h
# End Source File
# Begin Source File

SOURCE=.\vidsenddialog.h
# End Source File
# Begin Source File

SOURCE=.\vidsendDoc.h
# End Source File
# Begin Source File

SOURCE=.\vidsenddshow.h
# End Source File
# Begin Source File

SOURCE=.\vidsendLog.h
# End Source File
# Begin Source File

SOURCE=.\vidsendSerial.h
# End Source File
# Begin Source File

SOURCE=.\vidsendSet.h
# End Source File
# Begin Source File

SOURCE=.\vidsendSockets.h
# End Source File
# Begin Source File

SOURCE=.\vidsendTime.h
# End Source File
# Begin Source File

SOURCE=.\vidsendVideo.h
# End Source File
# Begin Source File

SOURCE=.\vidsendView.h
# End Source File
# Begin Source File

SOURCE=.\webbrowser2.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\2000.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bin00001.bin
# End Source File
# Begin Source File

SOURCE=.\res\bin00002.bin
# End Source File
# Begin Source File

SOURCE=.\res\bin00003.bin
# End Source File
# Begin Source File

SOURCE=.\res\bin00004.bin
# End Source File
# Begin Source File

SOURCE=.\res\bin00005.bin
# End Source File
# Begin Source File

SOURCE=.\res\bin00006.bin
# End Source File
# Begin Source File

SOURCE=.\res\bin00007.bin
# End Source File
# Begin Source File

SOURCE=.\res\bin00010.bin
# End Source File
# Begin Source File

SOURCE=.\res\bin00011.bin
# End Source File
# Begin Source File

SOURCE=.\res\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bitmap2.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00001.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00002.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00003.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00004.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00005.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00006.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00007.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00008.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00009.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00010.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00011.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00012.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00013.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00014.bmp
# End Source File
# Begin Source File

SOURCE=.\res\browserb.bmp
# End Source File
# Begin Source File

SOURCE=.\res\clear.ico
# End Source File
# Begin Source File

SOURCE=.\res\cool.bmp
# End Source File
# Begin Source File

SOURCE=.\res\directx.ico
# End Source File
# Begin Source File

SOURCE=.\res\display1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\display_.bmp
# End Source File
# Begin Source File

SOURCE=.\res\embarass.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_02.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_03.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_04.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_05.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_06.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_07.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_08.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_100.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_101.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_102.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_103.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_104.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_105.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_106.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_107.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_108.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_109.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_11.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_110.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_111.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_112.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_113.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_114.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_115.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_116.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_117.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_118.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_119.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_12.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_120.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_121.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_122.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_123.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_124.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_125.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_12_.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_13.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_14.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_15.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_16.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_17.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_18.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_19.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_20.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_21.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_22.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_23.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_24.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_26.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_27.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_28.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_29.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_30.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_31.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_32.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_33.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_34.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_35.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_36.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_37.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_38.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_39.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_40.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_41.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_42.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_43.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_44.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_45.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_46.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_47.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_48.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_49.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_50.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_51.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_52.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_53.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_54.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_55.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_56.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_57.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_58.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_59.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_60.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_61.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_62.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_63.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_64.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_65.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_66.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_67.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_68.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_69.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_70.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_71.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_72.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_73.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_74.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_75.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_76.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_77.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_78.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_79.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_8.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_80.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_81.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_82.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_84.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_85.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_86.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_87.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_88.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_89.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_9.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_90.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_91.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_92.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_93.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_94.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_95.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_96.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_97.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_99.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_coo.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_foo.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_fro.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_inn.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_kis.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_lau.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_ok.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_und.bmp
# End Source File
# Begin Source File

SOURCE=.\res\emot_win.bmp
# End Source File
# Begin Source File

SOURCE=.\res\exibitor.ico
# End Source File
# Begin Source File

SOURCE=.\res\eyes2.ico
# End Source File
# Begin Source File

SOURCE=.\film.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00001.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00002.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00003.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00004.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00005.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00006.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00007.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00008.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00009.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00010.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00011.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00012.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00013.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00014.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00015.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00016.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00017.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00018.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00019.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00020.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00021.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00022.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00023.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00024.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00025.ico
# End Source File
# Begin Source File

SOURCE=.\res\ico00026.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_main.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_vids.ico
# End Source File
# Begin Source File

SOURCE=.\res\led.bmp
# End Source File
# Begin Source File

SOURCE=.\res\ledoff.ico
# End Source File
# Begin Source File

SOURCE=.\res\ledon.ico
# End Source File
# Begin Source File

SOURCE=.\res\man1.ico
# End Source File
# Begin Source File

SOURCE=.\res\manopola.bmp
# End Source File
# Begin Source File

SOURCE=.\res\monoscop.bmp
# End Source File
# Begin Source File

SOURCE=.\res\NewMeet.ico
# End Source File
# Begin Source File

SOURCE=.\res\novideo.bmp
# End Source File
# Begin Source File

SOURCE=.\res\novideo1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\pannelli.bmp
# End Source File
# Begin Source File

SOURCE=.\res\pause1.ico
# End Source File
# Begin Source File

SOURCE=.\res\play1.ico
# End Source File
# Begin Source File

SOURCE=.\res\record1.ico
# End Source File
# Begin Source File

SOURCE=.\res\runnerma.ico
# End Source File
# Begin Source File

SOURCE=.\res\supervis.ico
# End Source File
# Begin Source File

SOURCE=.\res\Toolbar.bmp
# End Source File
# Begin Source File

SOURCE=.\res\trueimg.bmp
# End Source File
# Begin Source File

SOURCE=.\res\vidsend.ico
# End Source File
# Begin Source File

SOURCE=.\res\vidsend.rc2
# End Source File
# Begin Source File

SOURCE=.\res\vidsend2.ico
# End Source File
# Begin Source File

SOURCE=.\res\vidsendDoc.ico
# End Source File
# Begin Source File

SOURCE=.\res\vidsenty.ico
# End Source File
# Begin Source File

SOURCE=.\res\wave1.bin
# End Source File
# Begin Source File

SOURCE=.\res\wave2.bin
# End Source File
# End Group
# Begin Group "Help Files"

# PROP Default_Filter "cnt;rtf"
# Begin Source File

SOURCE=.\hlp\AfxCore.rtf
# End Source File
# Begin Source File

SOURCE=.\hlp\AfxDb.rtf
# End Source File
# Begin Source File

SOURCE=.\hlp\AfxPrint.rtf
# End Source File
# Begin Source File

SOURCE=.\hlp\AppExit.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\Bullet.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\CurArw2.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\CurArw4.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\CurHelp.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\EditCopy.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\EditCut.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\EditPast.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\EditUndo.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\FileNew.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\FileOpen.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\FilePrnt.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\FileSave.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\HlpSBar.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\HlpTBar.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\RecFirst.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\RecLast.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\RecNext.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\RecPrev.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\Scmax.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\ScMenu.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\Scmin.bmp
# End Source File
# Begin Source File

SOURCE=.\hlp\vidsend.cnt

!IF  "$(CFG)" == "vidsend - Win32 Release"

# PROP Ignore_Default_Tool 1
# Begin Custom Build - Copying contents file...
OutDir=.\Release
InputPath=.\hlp\vidsend.cnt
InputName=vidsend

"$(OutDir)\$(InputName).cnt" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy "hlp\$(InputName).cnt" $(OutDir)

# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Debug"

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Release"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Copying contents file...
OutDir=.\Newmeet_Release
InputPath=.\hlp\vidsend.cnt
InputName=vidsend

"$(OutDir)\$(InputName).cnt" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy "hlp\$(InputName).cnt" $(OutDir)

# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Debug"

!ELSEIF  "$(CFG)" == "vidsend - Win32 Newmeet Release Inglese"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Copying contents file...
OutDir=.\Newmeet_Release_Inglese
InputPath=.\hlp\vidsend.cnt
InputName=vidsend

"$(OutDir)\$(InputName).cnt" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy "hlp\$(InputName).cnt" $(OutDir)

# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Release StandAlone"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Copying contents file...
OutDir=.\Release_StandAlone
InputPath=.\hlp\vidsend.cnt
InputName=vidsend

"$(OutDir)\$(InputName).cnt" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy "hlp\$(InputName).cnt" $(OutDir)

# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 Camparty Release"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build - Copying contents file...
OutDir=.\Camparty_Release
InputPath=.\hlp\vidsend.cnt
InputName=vidsend

"$(OutDir)\$(InputName).cnt" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy "hlp\$(InputName).cnt" $(OutDir)

# End Custom Build

!ELSEIF  "$(CFG)" == "vidsend - Win32 camparty debug"

!ENDIF 

# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
# Section vidsend : {859321D0-3FD1-11CF-8981-00AA00688B10}
# 	2:5:Class:COLEObjects
# 	2:10:HeaderFile:oleobjects.h
# 	2:8:ImplFile:oleobjects.cpp
# End Section
# Section vidsend : {8856F961-340A-11D0-A96B-00C04FD705A2}
# 	2:21:DefaultSinkHeaderFile:webbrowser2.h
# 	2:16:DefaultSinkClass:CWebBrowser2
# End Section
# Section vidsend : {BEF6E003-A874-101A-8BBA-00AA00300CAB}
# 	2:5:Class:COleFont
# 	2:10:HeaderFile:font.h
# 	2:8:ImplFile:font.cpp
# End Section
# Section vidsend : {3B7C8860-D78F-101B-B9B5-04021C009402}
# 	2:21:DefaultSinkHeaderFile:richtext.h
# 	2:16:DefaultSinkClass:CRichText
# End Section
# Section vidsend : {7BF80981-BF32-101A-8BBB-00AA00300CAB}
# 	2:5:Class:CPicture
# 	2:10:HeaderFile:picture.h
# 	2:8:ImplFile:picture.cpp
# End Section
# Section vidsend : {ED117630-4090-11CF-8981-00AA00688B10}
# 	2:5:Class:COLEObject
# 	2:10:HeaderFile:oleobject.h
# 	2:8:ImplFile:oleobject.cpp
# End Section
# Section vidsend : {E9A5593C-CAB0-11D1-8C0B-0000F8754DA1}
# 	2:5:Class:CRichText
# 	2:10:HeaderFile:richtext.h
# 	2:8:ImplFile:richtext.cpp
# End Section
# Section vidsend : {D30C1661-CDAF-11D0-8A3E-00C04FC9E26E}
# 	2:5:Class:CWebBrowser2
# 	2:10:HeaderFile:webbrowser2.h
# 	2:8:ImplFile:webbrowser2.cpp
# End Section
