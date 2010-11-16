# Microsoft Developer Studio Project File - Name="pcsx" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=PCSX - WIN32 RELEASE
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pcsx.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pcsx.mak" CFG="PCSX - WIN32 RELEASE"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pcsx - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "pcsx - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pcsx - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /Zp16 /MT /W3 /GX /O2 /Op /Ob2 /I "../" /I "./zlib" /I "../libpcsxcore" /I "./glue" /I "./" /I "./gui" /I "./intl" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "__WIN32__" /D "_MSC_VER_" /D PCSX_VERSION=\"1.5\" /D "__i386__" /D "ENABLE_NLS" /D PACKAGE=\"pcsx\" /D inline=__forceinline /FR /FD /Zm200 /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x408 /d "NDEBUG"
# ADD RSC /l 0x408 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib wsock32.lib /nologo /subsystem:windows /pdb:none /machine:I386

!ELSEIF  "$(CFG)" == "pcsx - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D PCSX_VERSION=\"1.3\" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /GX /ZI /Od /I "../" /I "./zlib" /I "../libpcsxcore" /I "./glue" /I "./" /I "./gui" /I "./intl" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "__WIN32__" /D "__i386__" /D PCSX_VERSION=\"1.5\" /D "ENABLE_NLS" /D PACKAGE=\"pcsx\" /D inline= /FR /FD /GZ /Zm200 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x408 /d "_DEBUG"
# ADD RSC /l 0x408 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib comctl32.lib wsock32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "pcsx - Win32 Release"
# Name "pcsx - Win32 Debug"
# Begin Group "libpcsxcore"

# PROP Default_Filter ""
# Begin Group "ix86"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\libpcsxcore\ix86\iGte.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\ix86\iR3000A.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\ix86\ix86.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\ix86\ix86.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\libpcsxcore\cdriso.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\cdriso.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\cdrom.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\cdrom.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\cheat.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\cheat.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\coff.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\debug.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\debug.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\decode_xa.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\decode_xa.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\disr3000a.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\gte.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\gte.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\gte_divider.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\mdec.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\mdec.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\misc.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\misc.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\plugins.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\plugins.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\ppf.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\ppf.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psemu_plugin_defs.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxbios.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxbios.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxcommon.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxcommon.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxcounters.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxcounters.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxdma.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxdma.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxhle.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxhle.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxhw.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxhw.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxinterpreter.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxmem.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\psxmem.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\r3000a.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\r3000a.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\sio.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\sio.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\sjisfont.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\socket.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\socket.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\spu.c
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\spu.h
# End Source File
# Begin Source File

SOURCE=..\libpcsxcore\system.h
# End Source File
# End Group
# Begin Group "gui"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\gui\about.bmp
# End Source File
# Begin Source File

SOURCE=.\gui\AboutDlg.c
# End Source File
# Begin Source File

SOURCE=.\gui\AboutDlg.h
# End Source File
# Begin Source File

SOURCE=.\gui\cdrom02.ico
# End Source File
# Begin Source File

SOURCE=.\gui\CheatDlg.c
# End Source File
# Begin Source File

SOURCE=.\gui\ConfigurePlugins.c
# End Source File
# Begin Source File

SOURCE=.\gui\NoPic.h
# End Source File
# Begin Source File

SOURCE=.\gui\pcsx.bmp
# End Source File
# Begin Source File

SOURCE=.\gui\pcsx.exe.manifest
# End Source File
# Begin Source File

SOURCE=.\gui\plugin.c
# End Source File
# Begin Source File

SOURCE=.\gui\plugin.h
# End Source File
# Begin Source File

SOURCE=.\gui\Win32.h
# End Source File
# Begin Source File

SOURCE=.\gui\WndMain.c
# End Source File
# End Group
# Begin Group "zlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\zlib\adler32.c
# End Source File
# Begin Source File

SOURCE=.\zlib\compress.c
# End Source File
# Begin Source File

SOURCE=.\zlib\crc32.c
# End Source File
# Begin Source File

SOURCE=.\zlib\deflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib\deflate.h
# End Source File
# Begin Source File

SOURCE=.\zlib\gzio.c
# End Source File
# Begin Source File

SOURCE=.\zlib\infblock.c
# End Source File
# Begin Source File

SOURCE=.\zlib\infblock.h
# End Source File
# Begin Source File

SOURCE=.\zlib\infcodes.c
# End Source File
# Begin Source File

SOURCE=.\zlib\infcodes.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inffast.c
# End Source File
# Begin Source File

SOURCE=.\zlib\inffast.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inffixed.h
# End Source File
# Begin Source File

SOURCE=.\zlib\inflate.c
# End Source File
# Begin Source File

SOURCE=.\zlib\inftrees.c
# End Source File
# Begin Source File

SOURCE=.\zlib\inftrees.h
# End Source File
# Begin Source File

SOURCE=.\zlib\infutil.c
# End Source File
# Begin Source File

SOURCE=.\zlib\infutil.h
# End Source File
# Begin Source File

SOURCE=.\zlib\trees.c
# End Source File
# Begin Source File

SOURCE=.\zlib\trees.h
# End Source File
# Begin Source File

SOURCE=.\zlib\uncompr.c
# End Source File
# Begin Source File

SOURCE=.\zlib\zconf.h
# End Source File
# Begin Source File

SOURCE=.\zlib\zlib.h
# End Source File
# Begin Source File

SOURCE=.\zlib\zutil.c
# End Source File
# Begin Source File

SOURCE=.\zlib\zutil.h
# End Source File
# End Group
# Begin Group "glue"

# PROP Default_Filter ""
# Begin Group "sys"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\glue\sys\mman.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\glue\stdint.h
# End Source File
# End Group
# Begin Group "intl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\intl\bindtextdom.c
# End Source File
# Begin Source File

SOURCE=.\intl\dcgettext.c
# End Source File
# Begin Source File

SOURCE=.\intl\dgettext.c
# End Source File
# Begin Source File

SOURCE=.\intl\explodename.c
# End Source File
# Begin Source File

SOURCE=.\intl\finddomain.c
# End Source File
# Begin Source File

SOURCE=.\intl\gettext.c
# End Source File
# Begin Source File

SOURCE=.\intl\gettext.h
# End Source File
# Begin Source File

SOURCE=.\intl\gettextP.h
# End Source File
# Begin Source File

SOURCE=".\intl\hash-string.h"
# End Source File
# Begin Source File

SOURCE=".\intl\intl-compat.c"
# End Source File
# Begin Source File

SOURCE=.\intl\intlconfig.h
# End Source File
# Begin Source File

SOURCE=.\intl\l10nflist.c
# End Source File
# Begin Source File

SOURCE=.\intl\libgettext.h
# End Source File
# Begin Source File

SOURCE=.\intl\libintl.h
# End Source File
# Begin Source File

SOURCE=.\intl\loadinfo.h
# End Source File
# Begin Source File

SOURCE=.\intl\loadmsgcat.c
# End Source File
# Begin Source File

SOURCE=.\intl\localealias.c
# End Source File
# Begin Source File

SOURCE=.\intl\textdomain.c
# End Source File
# End Group
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=.\pcsx.rc
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# End Target
# End Project
