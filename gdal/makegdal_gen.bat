@echo off

:: ****************************************************************************
::  $Id: $
:: 
::  Name:     makegdal_gen.bat
::  Project:  GDAL 
::  Purpose:  Generate MS Visual C++ => 10.0 project files    
::  Author:   Ivan Lucena, [ivan lucena at outlook dot com]
:: 
:: ****************************************************************************
::  Copyright (c) 2007, Ivan Lucena    
:: 
::  Permission is hereby granted, free of charge, to any person obtaining a
::  copy of this software and associated documentation files (the "Software"),
::  to deal in the Software without restriction, including without limitation
::  the rights to use, copy, modify, merge, publish, distribute, sublicense,
::  and/or sell copies of the Software, and to permit persons to whom the
::  Software is furnished to do so, subject to the following conditions:
:: 
::  The above copyright notice and this permission notice shall be included
::  in all copies or substantial portions of the Software.
:: 
::  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
::  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
::  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
::  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
::  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
::  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
::  DEALINGS IN THE SOFTWARE.
:: ****************************************************************************

if "%1"=="" (
   goto :usage
)

if "%2"=="" (
   goto :usage
)

if "%3"=="" (
   goto :usage
)

::  *********************
::  Get Visual C++ version
::  *********************

set _vcver_=%1

set _clver_=1600
set _vstoolset_=v100

if "%_vcver_%"=="14.0" (
	set _clver_=1900
	set _vstoolset_=v140
) else ( if "%_vcver_%"=="12.0" (
	set _clver_=1800
	set _vstoolset_=v120
) else ( if "%_vcver_%"=="11.0" (
	set _clver_=1700
	set _vstoolset_=v110
) else ( if "%_vcver_%"=="10.0" (
	set _clver_=1600
	set _vstoolset_=v100
) else (
    echo Wrong value for parameter 1. See usage:
	goto :usage
))))

::  *********************
::  Get Platform
::  *********************

set _platf_=%2
set _buildplatf_=x86
set _winver_=Win32
set _nmake_opt_win64_=

if not "%_platf_%"=="32" (
    if not "%_platf_%"=="64" (
	    echo Wrong value for parameter 2. See usage:
	    goto :usage
    )
)

if "%_platf_%"=="64" (
    set _winver_=x64
    set _buildplatf_=x64
    set _nmake_opt_win64_=WIN64=1
)

goto :continue

::  *********************
:usage
::  *********************

echo Usage: makegdal_gen ^<Visual C++ version^> [32^|64] ^<^(*^) project file name^>
echo Parameters:
echo    1 : Visual C++ version is not the same as Visual Studio version ^( =^> 10.0 ^)
echo    2 : Windows platform 32 for Win32 and 64 for Win64
echo    3 : Base file name, with no path and no extension ^(*^)
echo Examples:
echo    makegdal_gen 10.1 32 makefileproj_vs10
echo    makegdal_gen 11.0 64 makefileproj_vs11
echo    makegdal_gen 12.0 64 makefileproj_vs12
echo    makegdal_gen 14.0 64 makefileproj_vs14

goto :end

::  *********************
::  Set Project file names
::  *********************

:continue

set _mainfile_=%CD%\%3.vcxproj
set _userfile_=%_mainfile_%.user
set _ftrlfile_=%_mainfile_%.filters

:: Add quotes

set _mainfile_="%_mainfile_%"
set _userfile_="%_userfile_%"
set _ftrlfile_="%_ftrlfile_%"

:: Progress message

echo Generating:
echo   %_mainfile_%
echo   %_userfile_%
echo   %_ftrlfile_%
echo This might take a little while...

:: Delete existing files

if exist %_mainfile_% (
  del %_mainfile_%
)

if exist %_userfile_% ( 
  del %_userfile_%
)

if exist %_ftrlfile_% ( 
  del %_ftrlfile_%
)

:: **********************************************
:: Generate user file (.vcxproj.user)
:: **********************************************

echo ^<?xml version="1.0" encoding="utf-8"?^>                   >> %_userfile_%
echo ^<Project ToolsVersion="%_vcver_%" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>   >> %_userfile_%
echo     ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'"^>  >> %_userfile_%
echo       ^<LocalDebuggerDebuggerType^>Auto^</LocalDebuggerDebuggerType^>                 >> %_userfile_%
echo       ^<LocalDebuggerCommand^>%CD%\apps\gdal_translate.exe^</LocalDebuggerCommand^>   >> %_userfile_%
echo       ^<LocalDebuggerCommandArguments^>--formats^</LocalDebuggerCommandArguments^>    >> %_userfile_%
echo       ^<DebuggerFlavor^>WindowsLocalDebugger^</DebuggerFlavor^>                       >> %_userfile_%
echo     ^</PropertyGroup^>                                     >> %_userfile_%
echo ^</Project^>                                               >> %_userfile_%

:: **********************************************
:: Initialize filters file (.vcxproj.filters)
:: **********************************************

echo ^<?xml version="1.0" encoding="utf-8"?^>                   >> %_ftrlfile_%
echo ^<Project ToolsVersion="5.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>         >> %_ftrlfile_%

:: **********************************************
:: Main file generator (.vcxproj) and filters file (.vcxproj.filters)
:: **********************************************

echo ^<?xml version="1.0" encoding="utf-8"?^>                   >> %_mainfile_%
echo ^<Project DefaultTargets="Build" ToolsVersion="%_vcver_%" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>          >> %_mainfile_%
echo   ^<ItemGroup Label="ProjectConfigurations"^>              >> %_mainfile_%
echo     ^<ProjectConfiguration Include="Debug|%_winver_%"^>    >> %_mainfile_%
echo       ^<Configuration^>Debug^</Configuration^>             >> %_mainfile_%
echo       ^<Platform^>%_winver_%^</Platform^>                  >> %_mainfile_%
echo     ^</ProjectConfiguration^>                              >> %_mainfile_%
echo     ^<ProjectConfiguration Include="Release|%_winver_%"^>  >> %_mainfile_%
echo       ^<Configuration^>Release^</Configuration^>           >> %_mainfile_%
echo       ^<Platform^>%_winver_%^</Platform^>                  >> %_mainfile_%
echo     ^</ProjectConfiguration^>                              >> %_mainfile_%
echo   ^</ItemGroup^>                                           >> %_mainfile_% 
echo   ^<PropertyGroup Label="Globals"^>                        >> %_mainfile_%
echo     ^<Keyword^>MakeFileProj^</Keyword^>                    >> %_mainfile_%
echo   ^</PropertyGroup^>                                       >> %_mainfile_%
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" /^>  >> %_mainfile_%   
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'" Label="Configuration"^>    >> %_mainfile_%
echo     ^<ConfigurationType^>Makefile^</ConfigurationType^>    >> %_mainfile_%
echo     ^<UseDebugLibraries^>true^</UseDebugLibraries^>        >> %_mainfile_%
echo     ^<PlatformToolset^>%_vstoolset_%^</PlatformToolset^>            >> %_mainfile_%
echo   ^</PropertyGroup^>                                       >> %_mainfile_%
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|%_winver_%'" Label="Configuration"^>  >> %_mainfile_%
echo     ^<ConfigurationType^>Makefile^</ConfigurationType^>    >> %_mainfile_%
echo     ^<UseDebugLibraries^>true^</UseDebugLibraries^>        >> %_mainfile_%
echo     ^<PlatformToolset^>%_vstoolset_%^</PlatformToolset^>            >> %_mainfile_%
echo   ^</PropertyGroup^>                                       >> %_mainfile_%
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /^>      >> %_mainfile_%
echo   ^<ImportGroup Label="ExtensionSettings"^>                >> %_mainfile_%  
echo   ^</ImportGroup^>                                         >> %_mainfile_%
echo   ^<ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'"^>     >> %_mainfile_% 
echo     ^<Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" /^>    >> %_mainfile_%
echo   ^</ImportGroup^>                                         >> %_mainfile_%
echo   ^<ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Release|%_winver_%'"^>  >> %_mainfile_%
echo     ^<Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" /^>     >> %_mainfile_%
echo   ^</ImportGroup^>                                         >> %_mainfile_%
echo   ^<PropertyGroup Label="UserMacros" /^>                   >> %_mainfile_%
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'"^>     >> %_mainfile_%
echo     ^<NMakeBuildCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% DEBUG=1 WITH_PDB=1^</NMakeBuildCommandLine^>            >> %_mainfile_%
echo     ^<NMakeOutput^>^</NMakeOutput^>                                                    >> %_mainfile_%
echo     ^<NMakeCleanCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% DEBUG=1 WITH_PDB=1 clean^</NMakeCleanCommandLine^>      >> %_mainfile_%
echo     ^<NMakeReBuildCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% DEBUG=1 WITH_PDB=1 clean ^&amp;^&amp; nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% DEBUG=1 WITH_PDB=1^</NMakeReBuildCommandLine^>  >> %_mainfile_%
echo     ^<NMakePreprocessorDefinitions^>%_winver_%;_DEBUG;$(NMakePreprocessorDefinitions)^</NMakePreprocessorDefinitions^>   >> %_mainfile_%
echo     ^<LibraryPath^>$(VC_LibraryPath_%_buildplatf_%);$(WindowsSDK_LibraryPath_%_buildplatf_%);$(VC_SourcePath);^</LibraryPath^>   >> %_mainfile_%
echo   ^</PropertyGroup^>                                                                   >> %_mainfile_%
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|%_winver_%'"^>   >> %_mainfile_%
echo     ^<NMakeBuildCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% ^</NMakeBuildCommandLine^>            >> %_mainfile_%
echo     ^<NMakeOutput^>^</NMakeOutput^>                                                    >> %_mainfile_%
echo     ^<NMakeCleanCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% clean^</NMakeCleanCommandLine^>      >> %_mainfile_%
echo     ^<NMakeReBuildCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% clean ^&amp;^&amp; nmake -f makefile.vc MSVC_VER=%_clver_%^</NMakeReBuildCommandLine^>  >> %_mainfile_%
echo     ^<NMakePreprocessorDefinitions^>%_winver_%;NDEBUG;$(NMakePreprocessorDefinitions)^</NMakePreprocessorDefinitions^>   >> %_mainfile_%
echo     ^<LibraryPath^>$(VC_LibraryPath_%_buildplatf_%);$(WindowsSDK_LibraryPath_%_buildplatf_%);$(VC_SourcePath);^</LibraryPath^>   >> %_mainfile_%
echo   ^</PropertyGroup^>                                                                   >> %_mainfile_%
echo   ^<ItemDefinitionGroup^>                                  >> %_mainfile_%
echo   ^</ItemDefinitionGroup^>                                 >> %_mainfile_%

:: create a root path with nmake files

echo   ^<ItemGroup^>                                            >> %_mainfile_%
echo       ^<Test Include="nmake.local" /^>                     >> %_mainfile_%
echo       ^<Test Include="nmake.opt" /^>                       >> %_mainfile_%
echo   ^</ItemGroup^>                                           >> %_mainfile_%

echo   ^<ItemGroup^>                                            >> %_ftrlfile_%
echo       ^<Test Include="nmake.local" /^>                     >> %_ftrlfile_%
echo       ^<Test Include="nmake.opt" /^>                       >> %_ftrlfile_%
echo   ^</ItemGroup^>                                           >> %_ftrlfile_%

:: create filters only

echo   ^<ItemGroup^>                                            >> %_ftrlfile_%
call :create_filter . "*.vc;" 1 "Make Files" "None"
call :create_filter . "*.h;*.hpp" 1 "Include Files" "ClInclude"
call :create_filter . "*.c;*.cpp" 1 "Source Files" "ClCompile"
echo   ^</ItemGroup^>                                           >> %_ftrlfile_%

:: create main file and links to filters

echo   ^<ItemGroup^>                                            >> %_mainfile_%
echo   ^<ItemGroup^>                                            >> %_ftrlfile_%
call :create_filter . "*.vc" 0 "Make Files" "None"
echo   ^</ItemGroup^>                                           >> %_mainfile_%
echo   ^</ItemGroup^>                                           >> %_ftrlfile_%

echo   ^<ItemGroup^>                                            >> %_mainfile_%
echo   ^<ItemGroup^>                                            >> %_ftrlfile_%
call :create_filter . "*.h;*.hpp" 0 "Include Files" "ClInclude"
echo   ^</ItemGroup^>                                           >> %_mainfile_%
echo   ^</ItemGroup^>                                           >> %_ftrlfile_%

echo   ^<ItemGroup^>                                            >> %_mainfile_%
echo   ^<ItemGroup^>                                            >> %_ftrlfile_%
call :create_filter . "*.c;*.cpp" 0 "Source Files" "ClCompile"
echo   ^</ItemGroup^>                                           >> %_mainfile_%
echo   ^</ItemGroup^>                                           >> %_ftrlfile_%

:: **********************************************
:: Finalize projects 
:: **********************************************

echo ^</Project^>                                               >> %_ftrlfile_%

echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets"/^>  >> %_mainfile_%
echo ^</Project^>                                               >> %_mainfile_%

echo Done!

:: **********************************************
:: The end
:: **********************************************

goto :end

:: **********************************************
:create_filter
:: **********************************************

    set _path_=%1
    set _mask_=%2
    set _fonly_=%3
    set _name_=%4
    set _item_=%5
    
    ::  *********************
    ::  Remove quotes 
    ::  *********************
    
    set _name_=%_name_:"=%
    set _mask_=%_mask_:"=% 
    set _fonly_=%_fonly_:"=%
    set _item_=%_item_:"=%
  
    ::  *********************
    ::  Stop folders
    ::  *********************
    
    set _folder_=%~nx1
  
    for %%d in (data debian docs html m4 pymod swig) do (
        if "%_folder_%"=="%%d" (
            goto :end
        )
    )

    ::  *********************
    ::  Check if a folde is empty
    ::  *********************

    set _find_=1
    
    for /R %%f in (%_mask_%) do (
      goto :not_empty
    )

    set _find_=0
  
    :not_empty
  
    if %_find_%==0 (
        goto :end
    )

    ::  *********************
    ::  Add filters 
    ::  *********************
    
    if %_fonly_%==1 (
        echo     ^<Filter Include="%_name_%"^>                   >> %_ftrlfile_%
        echo       ^<Extensions^>%_mask_%^</Extensions^>         >> %_ftrlfile_%
        echo     ^</Filter^>                                     >> %_ftrlfile_%
    )
  
    ::  *********************
    ::  Add files
    ::  *********************
    
    if %_fonly_%==0 (
        for %%f in (%_mask_%) do (
            echo     ^<%_item_% Include="%_path_%\%%f"/^>        >> %_mainfile_%
            echo     ^<%_item_% Include="%_path_%\%%f"^>         >> %_ftrlfile_%
            echo       ^<Filter^>%_name_%^</Filter^>             >> %_ftrlfile_%
            echo     ^</%_item_%^>                               >> %_ftrlfile_% 
        )
    )
  
    ::  *********************
    ::  Clib all the branches recursivelly
    ::  *********************
    
    for /D %%d in (*) do (
        cd %%d
        call :create_filter %_path_%\%%d "%_mask_%" %_fonly_% "%_name_%\%%d" %_item_%
        cd ..
    )
    
:: **********************************************
:end
:: **********************************************
