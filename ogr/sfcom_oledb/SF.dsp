# Microsoft Developer Studio Project File - Name="SF" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=SF - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SF.mak".
!MESSAGE 
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

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SF - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /ZI /Od /I "..\..\frmts\shapelib" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /FR /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x1009 /d "_DEBUG"
# ADD RSC /l 0x1009 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# Begin Custom Build - Performing registration
OutDir=.\Debug
TargetPath=.\Debug\SF.dll
InputPath=.\Debug\SF.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DebugU"
# PROP BASE Intermediate_Dir "DebugU"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugU"
# PROP Intermediate_Dir "DebugU"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE RSC /l 0x1009 /d "_DEBUG"
# ADD RSC /l 0x1009 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# Begin Custom Build - Performing registration
OutDir=.\DebugU
TargetPath=.\DebugU\SF.dll
InputPath=.\DebugU\SF.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseMinSize"
# PROP BASE Intermediate_Dir "ReleaseMinSize"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseMinSize"
# PROP Intermediate_Dir "ReleaseMinSize"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x1009 /d "NDEBUG"
# ADD RSC /l 0x1009 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# Begin Custom Build - Performing registration
OutDir=.\ReleaseMinSize
TargetPath=.\ReleaseMinSize\SF.dll
InputPath=.\ReleaseMinSize\SF.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseMinDependency"
# PROP BASE Intermediate_Dir "ReleaseMinDependency"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseMinDependency"
# PROP Intermediate_Dir "ReleaseMinDependency"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x1009 /d "NDEBUG"
# ADD RSC /l 0x1009 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# Begin Custom Build - Performing registration
OutDir=.\ReleaseMinDependency
TargetPath=.\ReleaseMinDependency\SF.dll
InputPath=.\ReleaseMinDependency\SF.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseUMinSize"
# PROP BASE Intermediate_Dir "ReleaseUMinSize"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseUMinSize"
# PROP Intermediate_Dir "ReleaseUMinSize"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_DLL" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x1009 /d "NDEBUG"
# ADD RSC /l 0x1009 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# Begin Custom Build - Performing registration
OutDir=.\ReleaseUMinSize
TargetPath=.\ReleaseUMinSize\SF.dll
InputPath=.\ReleaseUMinSize\SF.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "ReleaseUMinDependency"
# PROP BASE Intermediate_Dir "ReleaseUMinDependency"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseUMinDependency"
# PROP Intermediate_Dir "ReleaseUMinDependency"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" /D "_UNICODE" /D "_ATL_STATIC_REGISTRY" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x1009 /d "NDEBUG"
# ADD RSC /l 0x1009 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# Begin Custom Build - Performing registration
OutDir=.\ReleaseUMinDependency
TargetPath=.\ReleaseUMinDependency\SF.dll
InputPath=.\ReleaseUMinDependency\SF.dll
SOURCE="$(InputPath)"

"$(OutDir)\regsvr32.trg" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	if "%OS%"=="" goto NOTNT 
	if not "%OS%"=="Windows_NT" goto NOTNT 
	regsvr32 /s /c "$(TargetPath)" 
	echo regsvr32 exec. time > "$(OutDir)\regsvr32.trg" 
	goto end 
	:NOTNT 
	echo Warning : Cannot register Unicode DLL on Windows 95 
	:end 
	
# End Custom Build

!ENDIF 

# Begin Target

# Name "SF - Win32 Debug"
# Name "SF - Win32 Unicode Debug"
# Name "SF - Win32 Release MinSize"
# Name "SF - Win32 Release MinDependency"
# Name "SF - Win32 Unicode Release MinSize"
# Name "SF - Win32 Unicode Release MinDependency"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\frmts\shapelib\dbfopen.c
# End Source File
# Begin Source File

SOURCE=.\SF.cpp
# End Source File
# Begin Source File

SOURCE=.\SF.def
# End Source File
# Begin Source File

SOURCE=.\SF.idl
# ADD MTL /tlb ".\SF.tlb" /h "SF.h" /iid "SF_i.c" /Oicf
# End Source File
# Begin Source File

SOURCE=.\SF.rc
# End Source File
# Begin Source File

SOURCE=.\SFRS.cpp
# End Source File
# Begin Source File

SOURCE=..\..\frmts\shapelib\shpopen.c

!IF  "$(CFG)" == "SF - Win32 Debug"

# ADD CPP /I "..\..\frmts\shapelib"

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Debug"

!ELSEIF  "$(CFG)" == "SF - Win32 Release MinSize"

!ELSEIF  "$(CFG)" == "SF - Win32 Release MinDependency"

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinSize"

!ELSEIF  "$(CFG)" == "SF - Win32 Unicode Release MinDependency"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\SFDS.h
# End Source File
# Begin Source File

SOURCE=.\SFRS.h
# End Source File
# Begin Source File

SOURCE=.\SFSess.h
# End Source File
# Begin Source File

SOURCE=..\..\frmts\shapelib\shapefil.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\SF.rgs
# End Source File
# End Group
# End Target
# End Project
# Section SF : {6F72746E-006C-006C-80B0-00B0066600B8}
# 	1:26:IDS_DBPROP_LITERALIDENTITY:219
# 	1:30:IDS_DBPROP_INIT_PROVIDERSTRING:189
# 	1:28:IDS_DBPROP_ISupportErrorInfo:168
# 	1:24:IDS_DBPROP_IRowsetUpdate:167
# 	1:28:IDS_DBPROP_MAXTABLESINSELECT:150
# 	1:20:IDS_DBPROP_TABLETERM:126
# 	1:25:IDS_DBPROP_ROWTHREADMODEL:232
# 	1:18:IDS_DBPROP_MAXROWS:222
# 	1:23:IDS_DBPROP_IColumnsInfo:196
# 	1:30:IDS_DBPROP_AUTH_CACHE_AUTHINFO:172
# 	1:33:IDS_DBPROP_BLOCKINGSTORAGEOBJECTS:165
# 	1:30:IDS_DBPROP_NOTIFYROWUNDOINSERT:159
# 	1:29:IDS_DBPROP_CONCATNULLBEHAVIOR:109
# 	1:26:IDS_DBPROP_IRowsetIdentity:202
# 	1:25:IDS_DBPROP_IColumnsRowset:197
# 	1:27:IDS_DBPROP_TRANSACTEDOBJECT:192
# 	1:23:IDS_DBPROP_INIT_TIMEOUT:187
# 	1:32:IDS_DBPROP_AUTH_ENCRYPT_PASSWORD:173
# 	1:26:IDS_DBPROP_MULTIPLERESULTS:162
# 	1:33:IDS_DBPROP_ORDERBYCOLUMNSINSELECT:156
# 	1:26:IDS_DBPROP_NOTIFYCOLUMNSET:134
# 	1:27:IDS_DBPROP_ORDEREDBOOKMARKS:233
# 	1:22:IDS_DBPROP_ROWRESTRICT:231
# 	1:22:IDS_DBPROP_INIT_PROMPT:185
# 	1:24:IDS_DBPROP_INIT_LOCATION:183
# 	1:26:IDS_DBPROP_AUTH_INTEGRATED:174
# 	1:29:IDS_DBPROP_CHANGEINSERTEDROWS:169
# 	1:31:IDS_DBPROP_NOTIFYROWFIRSTCHANGE:136
# 	1:23:IDS_DBPROP_IMMOBILEROWS:217
# 	1:22:IDS_DBPROP_CANHOLDROWS:210
# 	1:23:IDS_DBPROP_SERVERCURSOR:191
# 	1:24:IDS_DBPROP_IRowsetScroll:166
# 	1:31:IDS_DBPROP_PREPAREABORTBEHAVIOR:158
# 	1:21:IDS_DBPROP_MAXROWSIZE:148
# 	1:30:IDS_DBPROP_NOTIFYROWUNDODELETE:142
# 	1:19:IDS_DBPROP_DBMSNAME:112
# 	1:25:IDS_DBPROP_COLUMNRESTRICT:213
# 	1:29:IDS_DBPROP_CANSCROLLBACKWARDS:212
# 	1:22:IDS_DBPROP_IRowsetInfo:203
# 	1:24:IDS_DBPROP_IRowsetChange:201
# 	1:18:IDS_DBPROP_IRowset:200
# 	1:36:IDS_DBPROP_IConnectionPointContainer:198
# 	1:23:IDS_DBPROP_UPDATABILITY:193
# 	1:23:IDS_DBPROP_IConvertType:171
# 	1:21:IDS_DBPROP_OLEOBJECTS:155
# 	1:28:IDS_DBPROP_STRUCTUREDSTORAGE:122
# 	1:24:IDS_DBPROP_PROCEDURETERM:114
# 	1:18:IDS_DBPROP_DBMSVER:113
# 	1:25:IDS_DBPROP_ACTIVESESSIONS:102
# 	1:23:IDS_DBPROP_QUICKRESTART:227
# 	1:35:IDS_DBPROP_INIT_IMPERSONATION_LEVEL:182
# 	1:27:IDS_DBPROP_LITERALBOOKMARKS:218
# 	1:30:IDS_DBPROP_DELAYSTORAGEOBJECTS:216
# 	1:20:IDS_DBPROP_BOOKMARKS:206
# 	1:25:IDS_DBPROP_STRONGIDENTITY:194
# 	1:22:IDS_DBPROP_AUTH_USERID:179
# 	1:37:IDS_DBPROP_ROWSETCONVERSIONSONCOMMAND:161
# 	1:33:IDS_DBPROP_MAXROWSIZEINCLUDESBLOB:149
# 	1:25:IDS_DBPROP_IDENTIFIERCASE:145
# 	1:30:IDS_DBPROP_NOTIFYROWUNDOCHANGE:141
# 	1:26:IDS_DBPROP_NOTIFYROWINSERT:137
# 	1:28:IDS_DBPROP_MULTIPLEPARAMSETS:131
# 	1:19:IDS_DBPROP_USERNAME:127
# 	1:32:IDS_DBPROP_SUPPORTEDTXNISOLEVELS:124
# 	1:23:IDS_DBPROP_PROVIDERNAME:116
# 	1:22:IDS_DBPROP_MAXOPENROWS:220
# 	1:20:IDS_DBPROP_IAccessor:195
# 	1:31:IDS_DBPROP_RETURNPENDINGINSERTS:170
# 	1:26:IDS_DBPROP_NOTIFYROWUPDATE:160
# 	1:30:IDS_DBPROP_HETEROGENEOUSTABLES:144
# 	1:42:IDS_DBPROP_NOTIFYROWSETFETCHPOSITIONCHANGE:140
# 	1:22:IDS_DBPROP_PROVIDERVER:117
# 	1:25:IDS_DBPROP_MAXPENDINGROWS:221
# 	1:25:IDS_DBPROP_COMMANDTIMEOUT:214
# 	1:25:IDS_DBPROP_IRowsetResynch:205
# 	1:24:IDS_DBPROP_IRowsetLocate:204
# 	1:26:IDS_DBPROP_INIT_DATASOURCE:180
# 	1:25:IDS_DBPROP_DSOTHREADMODEL:130
# 	1:22:IDS_DBPROP_SCHEMAUSAGE:120
# 	1:26:IDS_DBPROP_REENTRANTEVENTS:228
# 	1:20:IDS_DBPROP_OWNINSERT:225
# 	1:20:IDS_DBPROP_INIT_LCID:188
# 	1:20:IDS_DBPROP_INIT_MODE:184
# 	1:20:IDS_DBPROP_LOCKMODES:146
# 	1:26:IDS_DBPROP_NOTIFYROWDELETE:135
# 	1:26:IDS_DBPROP_SUPPORTEDTXNDDL:128
# 	1:21:IDS_DBPROP_SQLSUPPORT:121
# 	1:27:IDS_DBPROP_PROVIDEROLEDBVER:115
# 	1:27:IDS_DBPROP_COLUMNDEFINITION:108
# 	1:23:IDS_DBPROP_CATALOGUSAGE:107
# 	1:22:IDS_DBPROP_CATALOGTERM:106
# 	1:22:IDS_DBPROP_OTHERINSERT:223
# 	1:25:IDS_DBPROP_COMMITPRESERVE:215
# 	1:23:IDS_DBPROP_BOOKMARKTYPE:208
# 	1:32:IDS_DBPROP_INIT_PROTECTION_LEVEL:186
# 	1:42:IDS_DBPROP_AUTH_PERSIST_SENSITIVE_AUTHINFO:178
# 	1:24:IDS_DBPROP_NULLCOLLATION:154
# 	1:26:IDS_DBPROP_CATALOGLOCATION:105
# 	1:21:IDS_DBPROP_APPENDONLY:211
# 	1:20:IDS_DBPROP_INIT_HWND:181
# 	1:28:IDS_DBPROP_ISequentialStream:163
# 	1:29:IDS_DBPROP_NOTIFICATIONPHASES:153
# 	1:18:IDS_DBPROP_GROUPBY:143
# 	1:21:IDS_DBPROP_SUBQUERIES:123
# 	1:29:IDS_DBPROP_DATASOURCEREADONLY:111
# 	1:25:IDS_DBPROP_ASYNCTXNCOMMIT:103
# 	1:6:IDR_SF:101
# 	1:32:IDS_DBPROP_REPORTMULTIPLECHANGES:230
# 	1:26:IDS_DBPROP_BOOKMARKSKIPPED:207
# 	1:24:IDS_DBPROP_AUTH_PASSWORD:176
# 	1:24:IDS_DBPROP_ABORTPRESERVE:164
# 	1:21:IDS_DBPROP_SCHEMATERM:119
# 	1:25:IDS_DBPROP_BYREFACCESSORS:104
# 	1:24:IDS_DBPROP_REMOVEDELETED:229
# 	1:27:IDS_DBPROP_MULTITABLEUPDATE:152
# 	1:24:IDS_DBPROP_ASYNCTXNABORT:129
# 	1:32:IDS_DBPROP_SUPPORTEDTXNISORETAIN:125
# 	1:35:IDS_DBPROP_SESS_AUTOCOMMITISOLEVELS:190
# 	1:32:IDS_DBPROP_PREPARECOMMITBEHAVIOR:157
# 	1:33:IDS_DBPROP_MULTIPLESTORAGEOBJECTS:151
# 	1:23:IDS_DBPROP_MAXINDEXSIZE:147
# 	1:30:IDS_DBPROP_NOTIFYROWSETRELEASE:139
# 	1:31:IDS_DBPROP_QUOTEDIDENTIFIERCASE:118
# 	1:26:IDS_DBPROP_OWNUPDATEDELETE:226
# 	1:28:IDS_DBPROP_OTHERUPDATEDELETE:224
# 	1:28:IDS_DBPROP_CANFETCHBACKWARDS:209
# 	1:26:IDS_DBPROP_IProvideMoniker:199
# 	1:33:IDS_DBPROP_AUTH_PERSIST_ENCRYPTED:177
# 	1:29:IDS_DBPROP_AUTH_MASK_PASSWORD:175
# 	1:27:IDS_DBPROP_NOTIFYROWRESYNCH:138
# 	1:27:IDS_DBPROP_PERSISTENTIDTYPE:133
# 	1:38:IDS_DBPROP_OUTPUTPARAMETERAVAILABILITY:132
# 	1:25:IDS_DBPROP_DATASOURCENAME:110
# End Section
