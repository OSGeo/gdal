# Microsoft Developer Studio Generated NMAKE File, Based on SF.dsp
!IF "$(CFG)" == ""
CFG=SF - Win32 Debug
!MESSAGE No configuration specified. Defaulting to SF - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "SF - Win32 Debug" && "$(CFG)" != "SF - Win32 Unicode Debug" && "$(CFG)" != "SF - Win32 Release MinSize" && "$(CFG)" != "SF - Win32 Release MinDependency" && "$(CFG)" != "SF - Win32 Unicode Release MinSize" && "$(CFG)" != "SF - Win32 Unicode Release MinDependency"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SF.mak" CFG="SF - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SF - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SF - Win32 Unicode Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SF - Win32 Release MinSize" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SF - Win32 Release MinDependency" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SF - Win32 Unicode Release MinSize" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "SF - Win32 Unicode Release MinDependency" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "SF - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\SF.dll" ".\SF.tlb" ".\SF.h" ".\SF_i.c" "$(OUTDIR)\SF.pch" "$(OUTDIR)\SF.bsc" ".\Debug\regsvr32.trg"


CLEAN :
	-@erase "$(INTDIR)\dbfopen.obj"
	-@erase "$(INTDIR)\dbfopen.sbr"
	-@erase "$(INTDIR)\SF.obj"
	-@erase "$(INTDIR)\SF.pch"
	-@erase "$(INTDIR)\SF.res"
	-@erase "$(INTDIR)\SF.sbr"
	-@erase "$(INTDIR)\SFRS.obj"
	-@erase "$(INTDIR)\SFRS.sbr"
	-@erase "$(INTDIR)\shpopen.obj"
	-@erase "$(INTDIR)\shpopen.sbr"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\StdAfx.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\SF.bsc"
	-@erase "$(OUTDIR)\SF.dll"
	-@erase "$(OUTDIR)\SF.exp"
	-@erase "$(OUTDIR)\SF.ilk"
	-@erase "$(OUTDIR)\SF.lib"
	-@erase "$(OUTDIR)\SF.pdb"
	-@erase ".\SF.h"
	-@erase ".\SF.tlb"
	-@erase ".\SF_i.c"
	-@erase ".\Debug\regsvr32.trg"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
MSDASDK=f:\MSDASDK
ATL=F:\VisualStudio\VC98\ATL
CPP_PROJ=/nologo /MTd /W3 /Gm /ZI /Od /I "$(ATL)\Include" /I "$(MSDASDK)\include\oledb" /I "..\..\frmts\shapelib" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) $(CPP_PROJ)

.cpp{$(INTDIR)}.obj::
   $(CPP) $(CPP_PROJ)

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=
RSC=rc.exe
RSC_PROJ=/l 0x1009 /fo"$(INTDIR)\SF.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\SF.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\dbfopen.sbr" \
	"$(INTDIR)\SF.sbr" \
	"$(INTDIR)\SFRS.sbr" \
	"$(INTDIR)\shpopen.sbr" \
	"$(INTDIR)\StdAfx.sbr"

"$(OUTDIR)\SF.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /incremental:yes /pdb:"$(OUTDIR)\SF.pdb" /debug /machine:I386 /def:".\SF.def" /out:"$(OUTDIR)\SF.dll" /implib:"$(OUTDIR)\SF.lib" /pdbtype:sept 
DEF_FILE= \
	".\SF.def"
LINK32_OBJS= \
	"$(INTDIR)\dbfopen.obj" \
	"$(INTDIR)\SF.obj" \
	"$(INTDIR)\SFRS.obj" \
	"$(INTDIR)\shpopen.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\SF.res"

"$(OUTDIR)\SF.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

OutDir=.\Debug
TargetPath=.\Debug\SF.dll
InputPath=.\Debug\SF.dll
SOURCE="$(InputPath)"

"$(OUTDIR)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
<< 
	

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"

OUTDIR=.\DebugU
INTDIR=.\DebugU
# Begin Custom Macros
OutDir=.\DebugU
# End Custom Macros

ALL : "$(OUTDIR)\SF.dll" ".\SF.tlb" ".\SF.h" ".\SF_i.c" ".\DebugU\regsvr32.trg"


CLEAN :
	-@erase "$(INTDIR)\dbfopen.obj"
	-@erase "$(INTDIR)\SF.obj"
	-@erase "$(INTDIR)\SF.pch"
	-@erase "$(INTDIR)\SF.res"
	-@erase "$(INTDIR)\SFRS.obj"
	-@erase "$(INTDIR)\shpopen.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\SF.dll"
	-@erase "$(OUTDIR)\SF.exp"
	-@erase "$(OUTDIR)\SF.ilk"
	-@erase "$(OUTDIR)\SF.lib"
	-@erase "$(OUTDIR)\SF.pdb"
	-@erase ".\SF.h"
	-@erase ".\SF.tlb"
	-@erase ".\SF_i.c"
	-@erase ".\DebugU\regsvr32.trg"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=
RSC=rc.exe
RSC_PROJ=/l 0x1009 /fo"$(INTDIR)\SF.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\SF.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /incremental:yes /pdb:"$(OUTDIR)\SF.pdb" /debug /machine:I386 /def:".\SF.def" /out:"$(OUTDIR)\SF.dll" /implib:"$(OUTDIR)\SF.lib" /pdbtype:sept 
DEF_FILE= \
	".\SF.def"
LINK32_OBJS= \
	"$(INTDIR)\dbfopen.obj" \
	"$(INTDIR)\SF.obj" \
	"$(INTDIR)\SFRS.obj" \
	"$(INTDIR)\shpopen.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\SF.res"

"$(OUTDIR)\SF.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

OutDir=.\DebugU
TargetPath=.\DebugU\SF.dll
InputPath=.\DebugU\SF.dll
SOURCE="$(InputPath)"

"$(OUTDIR)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
<< 
	

!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"

OUTDIR=.\ReleaseMinSize
INTDIR=.\ReleaseMinSize
# Begin Custom Macros
OutDir=.\ReleaseMinSize
# End Custom Macros

ALL : "$(OUTDIR)\SF.dll" ".\SF.tlb" ".\SF.h" ".\SF_i.c" ".\ReleaseMinSize\regsvr32.trg"


CLEAN :
	-@erase "$(INTDIR)\dbfopen.obj"
	-@erase "$(INTDIR)\SF.obj"
	-@erase "$(INTDIR)\SF.pch"
	-@erase "$(INTDIR)\SF.res"
	-@erase "$(INTDIR)\SFRS.obj"
	-@erase "$(INTDIR)\shpopen.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\SF.dll"
	-@erase "$(OUTDIR)\SF.exp"
	-@erase "$(OUTDIR)\SF.lib"
	-@erase ".\SF.h"
	-@erase ".\SF.tlb"
	-@erase ".\SF_i.c"
	-@erase ".\ReleaseMinSize\regsvr32.trg"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=
RSC=rc.exe
RSC_PROJ=/l 0x1009 /fo"$(INTDIR)\SF.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\SF.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\SF.pdb" /machine:I386 /def:".\SF.def" /out:"$(OUTDIR)\SF.dll" /implib:"$(OUTDIR)\SF.lib" 
DEF_FILE= \
	".\SF.def"
LINK32_OBJS= \
	"$(INTDIR)\dbfopen.obj" \
	"$(INTDIR)\SF.obj" \
	"$(INTDIR)\SFRS.obj" \
	"$(INTDIR)\shpopen.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\SF.res"

"$(OUTDIR)\SF.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

OutDir=.\ReleaseMinSize
TargetPath=.\ReleaseMinSize\SF.dll
InputPath=.\ReleaseMinSize\SF.dll
SOURCE="$(InputPath)"

"$(OUTDIR)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
<< 
	

!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"

OUTDIR=.\ReleaseMinDependency
INTDIR=.\ReleaseMinDependency
# Begin Custom Macros
OutDir=.\ReleaseMinDependency
# End Custom Macros

ALL : "$(OUTDIR)\SF.dll" ".\SF.tlb" ".\SF.h" ".\SF_i.c" ".\ReleaseMinDependency\regsvr32.trg"


CLEAN :
	-@erase "$(INTDIR)\dbfopen.obj"
	-@erase "$(INTDIR)\SF.obj"
	-@erase "$(INTDIR)\SF.pch"
	-@erase "$(INTDIR)\SF.res"
	-@erase "$(INTDIR)\SFRS.obj"
	-@erase "$(INTDIR)\shpopen.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\SF.dll"
	-@erase "$(OUTDIR)\SF.exp"
	-@erase "$(OUTDIR)\SF.lib"
	-@erase ".\SF.h"
	-@erase ".\SF.tlb"
	-@erase ".\SF_i.c"
	-@erase ".\ReleaseMinDependency\regsvr32.trg"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=
RSC=rc.exe
RSC_PROJ=/l 0x1009 /fo"$(INTDIR)\SF.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\SF.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\SF.pdb" /machine:I386 /def:".\SF.def" /out:"$(OUTDIR)\SF.dll" /implib:"$(OUTDIR)\SF.lib" 
DEF_FILE= \
	".\SF.def"
LINK32_OBJS= \
	"$(INTDIR)\dbfopen.obj" \
	"$(INTDIR)\SF.obj" \
	"$(INTDIR)\SFRS.obj" \
	"$(INTDIR)\shpopen.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\SF.res"

"$(OUTDIR)\SF.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

OutDir=.\ReleaseMinDependency
TargetPath=.\ReleaseMinDependency\SF.dll
InputPath=.\ReleaseMinDependency\SF.dll
SOURCE="$(InputPath)"

"$(OUTDIR)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
<< 
	

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"

OUTDIR=.\ReleaseUMinSize
INTDIR=.\ReleaseUMinSize
# Begin Custom Macros
OutDir=.\ReleaseUMinSize
# End Custom Macros

ALL : "$(OUTDIR)\SF.dll" ".\SF.tlb" ".\SF.h" ".\SF_i.c" ".\ReleaseUMinSize\regsvr32.trg"


CLEAN :
	-@erase "$(INTDIR)\dbfopen.obj"
	-@erase "$(INTDIR)\SF.obj"
	-@erase "$(INTDIR)\SF.pch"
	-@erase "$(INTDIR)\SF.res"
	-@erase "$(INTDIR)\SFRS.obj"
	-@erase "$(INTDIR)\shpopen.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\SF.dll"
	-@erase "$(OUTDIR)\SF.exp"
	-@erase "$(OUTDIR)\SF.lib"
	-@erase ".\SF.h"
	-@erase ".\SF.tlb"
	-@erase ".\SF_i.c"
	-@erase ".\ReleaseUMinSize\regsvr32.trg"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=
RSC=rc.exe
RSC_PROJ=/l 0x1009 /fo"$(INTDIR)\SF.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\SF.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\SF.pdb" /machine:I386 /def:".\SF.def" /out:"$(OUTDIR)\SF.dll" /implib:"$(OUTDIR)\SF.lib" 
DEF_FILE= \
	".\SF.def"
LINK32_OBJS= \
	"$(INTDIR)\dbfopen.obj" \
	"$(INTDIR)\SF.obj" \
	"$(INTDIR)\SFRS.obj" \
	"$(INTDIR)\shpopen.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\SF.res"

"$(OUTDIR)\SF.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

OutDir=.\ReleaseUMinSize
TargetPath=.\ReleaseUMinSize\SF.dll
InputPath=.\ReleaseUMinSize\SF.dll
SOURCE="$(InputPath)"

"$(OUTDIR)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
<< 
	

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"

OUTDIR=.\ReleaseUMinDependency
INTDIR=.\ReleaseUMinDependency
# Begin Custom Macros
OutDir=.\ReleaseUMinDependency
# End Custom Macros

ALL : "$(OUTDIR)\SF.dll" ".\SF.tlb" ".\SF.h" ".\SF_i.c" ".\ReleaseUMinDependency\regsvr32.trg"


CLEAN :
	-@erase "$(INTDIR)\dbfopen.obj"
	-@erase "$(INTDIR)\SF.obj"
	-@erase "$(INTDIR)\SF.pch"
	-@erase "$(INTDIR)\SF.res"
	-@erase "$(INTDIR)\SFRS.obj"
	-@erase "$(INTDIR)\shpopen.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\SF.dll"
	-@erase "$(OUTDIR)\SF.exp"
	-@erase "$(OUTDIR)\SF.lib"
	-@erase ".\SF.h"
	-@erase ".\SF.tlb"
	-@erase ".\SF_i.c"
	-@erase ".\ReleaseUMinDependency\regsvr32.trg"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=
RSC=rc.exe
RSC_PROJ=/l 0x1009 /fo"$(INTDIR)\SF.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\SF.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /incremental:no /pdb:"$(OUTDIR)\SF.pdb" /machine:I386 /def:".\SF.def" /out:"$(OUTDIR)\SF.dll" /implib:"$(OUTDIR)\SF.lib" 
DEF_FILE= \
	".\SF.def"
LINK32_OBJS= \
	"$(INTDIR)\dbfopen.obj" \
	"$(INTDIR)\SF.obj" \
	"$(INTDIR)\SFRS.obj" \
	"$(INTDIR)\shpopen.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\SF.res"

"$(OUTDIR)\SF.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

OutDir=.\ReleaseUMinDependency
TargetPath=.\ReleaseUMinDependency\SF.dll
InputPath=.\ReleaseUMinDependency\SF.dll
SOURCE="$(InputPath)"

"$(OUTDIR)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
<< 
	

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("SF.dep")
!INCLUDE "SF.dep"
!ELSE 
!MESSAGE Warning: cannot find "SF.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "SF - Win32 Debug" || "$(CFG)" == "SF - Win32 Unicode Debug" || "$(CFG)" == "SF - Win32 Release MinSize" || "$(CFG)" == "SF - Win32 Release MinDependency" || "$(CFG)" == "SF - Win32 Unicode Release MinSize" || "$(CFG)" == "SF - Win32 Unicode Release MinDependency"
SOURCE=..\..\frmts\shapelib\dbfopen.c

!IF  "$(CFG)" == "SF - Win32 Debug"


"$(INTDIR)\dbfopen.obj"	"$(INTDIR)\dbfopen.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"


"$(INTDIR)\dbfopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"


"$(INTDIR)\dbfopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"


"$(INTDIR)\dbfopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"


"$(INTDIR)\dbfopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"


"$(INTDIR)\dbfopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\SF.cpp

!IF  "$(CFG)" == "SF - Win32 Debug"


"$(INTDIR)\SF.obj"	"$(INTDIR)\SF.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"


"$(INTDIR)\SF.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"


"$(INTDIR)\SF.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"


"$(INTDIR)\SF.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"


"$(INTDIR)\SF.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"


"$(INTDIR)\SF.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ENDIF 

SOURCE=.\SF.idl

!IF  "$(CFG)" == "SF - Win32 Debug"

MTL_SWITCHES=/tlb ".\SF.tlb" /h "SF.h" /iid "SF_i.c" /Oicf 

".\SF.tlb"	".\SF.h"	".\SF_i.c" : $(SOURCE) "$(INTDIR)"
	$(MTL) @<<
  $(MTL_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"

MTL_SWITCHES=/tlb ".\SF.tlb" /h "SF.h" /iid "SF_i.c" /Oicf 

".\SF.tlb"	".\SF.h"	".\SF_i.c" : $(SOURCE) "$(INTDIR)"
	$(MTL) @<<
  $(MTL_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"

MTL_SWITCHES=/tlb ".\SF.tlb" /h "SF.h" /iid "SF_i.c" /Oicf 

".\SF.tlb"	".\SF.h"	".\SF_i.c" : $(SOURCE) "$(INTDIR)"
	$(MTL) @<<
  $(MTL_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"

MTL_SWITCHES=/tlb ".\SF.tlb" /h "SF.h" /iid "SF_i.c" /Oicf 

".\SF.tlb"	".\SF.h"	".\SF_i.c" : $(SOURCE) "$(INTDIR)"
	$(MTL) @<<
  $(MTL_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"

MTL_SWITCHES=/tlb ".\SF.tlb" /h "SF.h" /iid "SF_i.c" /Oicf 

".\SF.tlb"	".\SF.h"	".\SF_i.c" : $(SOURCE) "$(INTDIR)"
	$(MTL) @<<
  $(MTL_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"

MTL_SWITCHES=/tlb ".\SF.tlb" /h "SF.h" /iid "SF_i.c" /Oicf 

".\SF.tlb"	".\SF.h"	".\SF_i.c" : $(SOURCE) "$(INTDIR)"
	$(MTL) @<<
  $(MTL_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\SF.rc

"$(INTDIR)\SF.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\SFRS.cpp

!IF  "$(CFG)" == "SF - Win32 Debug"


"$(INTDIR)\SFRS.obj"	"$(INTDIR)\SFRS.sbr" : $(SOURCE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"


"$(INTDIR)\SFRS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"


"$(INTDIR)\SFRS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"


"$(INTDIR)\SFRS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"


"$(INTDIR)\SFRS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"


"$(INTDIR)\SFRS.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"


!ENDIF 

SOURCE=..\..\frmts\shapelib\shpopen.c

!IF  "$(CFG)" == "SF - Win32 Debug"

ATL=F:\VisualStudio\VC98\ATL
CPP_SWITCHES=/nologo /MTd /W3 /Gm /ZI /Od /I "$(ATL)\Include" /I "..\..\frmts\shapelib" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR"$(INTDIR)\\" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\shpopen.obj"	"$(INTDIR)\shpopen.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\shpopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"

CPP_SWITCHES=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\shpopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"

CPP_SWITCHES=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\shpopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"

CPP_SWITCHES=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\shpopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"

CPP_SWITCHES=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yu"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\shpopen.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\SF.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\StdAfx.cpp

!IF  "$(CFG)" == "SF - Win32 Debug"

ATL=F:\VisualStudio\VC98\ATL
CPP_SWITCHES=/nologo /MTd /W3 /Gm /ZI /Od /I $(ATL)\Include /I "..\..\frmts\shapelib" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\SF.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\StdAfx.sbr"	"$(INTDIR)\SF.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /Fp"$(INTDIR)\SF.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\SF.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"

CPP_SWITCHES=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\SF.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"

CPP_SWITCHES=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\SF.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"

CPP_SWITCHES=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\SF.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"

CPP_SWITCHES=/nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Fp"$(INTDIR)\SF.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\SF.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 


!ENDIF 

