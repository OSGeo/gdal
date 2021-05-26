@echo off

:: ****************************************************************************
::  $Id: $
:: 
::  Name:     generate_vcxproj.bat
::  Project:  GDAL 
::  Purpose:  Generate MS Visual C++ => 12.0 project files
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
setlocal

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

if "%_vcver_%"=="16.0" (
	set _clver_=1920
	set _vstoolset_=v142
) else if "%_vcver_%"=="15.0" (
	set _clver_=1910
	set _vstoolset_=v141
) else if "%_vcver_%"=="14.0" (
	set _clver_=1900
	set _vstoolset_=v140
) else ( if "%_vcver_%"=="12.0" (
	set _clver_=1800
	set _vstoolset_=v120
) else (
    echo Wrong value for parameter 1. See usage:
	goto :usage
))

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

echo Usage: generate_vcxproj ^<Visual C++ version^> [32^|64] ^<^(*^) project file name^>
echo Parameters:
echo    1 : Visual C++ version is not the same as Visual Studio version ^( =^> 14.0 ^)
echo    2 : Windows platform 32 for Win32 and 64 for Win64
echo    3 : Base file name, with no path and no extension ^(*^)
echo Examples:
echo    generate_vcxproj 12.0 64 gdal_vs2013
echo    generate_vcxproj 14.0 64 gdal_vs2015
echo    generate_vcxproj 15.0 64 gdal_vs2017
echo    generate_vcxproj 16.0 64 gdal_vs2019
echo WARNING: GDAL requires C++11. It is not guaranteed to build with VS2013.

goto :end

::  *********************
::  Set Project file names
::  *********************

:continue

set _gdaldir_=%CD%
set _testdir_=
FOR /F %%i IN ("%_gdaldir_%\..\autotest\cpp") DO (
    if exist "%%~fi" set _testdir_=%%~fi
)

set _gdalsln_=%_gdaldir_%\%3.sln
set _gdalproj_=%_gdaldir_%\%3.vcxproj
set _gdaluser_=%_gdalproj_%.user
set _gdalfltr_=%_gdalproj_%.filters
set _testproj_=%_testdir_%\%3_test.vcxproj
set _testuser_=%_testproj_%.user
set _testfltr_=%_testproj_%.filters

:: Add quotes

set _gdalproj_="%_gdalproj_%"
set _gdaluser_="%_gdaluser_%"
set _gdalfltr_="%_gdalfltr_%"
set _testproj_="%_testproj_%"
set _testuser_="%_testuser_%"
set _testfltr_="%_testfltr_%"

:: Progress message

echo Generating:
echo   %_gdalproj_%
echo   %_gdaluser_%
echo   %_gdalfltr_%

if defined _testdir_ (
    echo   %_testproj_%
    echo   %_testuser_%
    echo   %_testfltr_%
)
:: if defined _testdir_

echo This might take a little while...

:: Delete existing files

if exist %_gdalproj_% (
  del %_gdalproj_%
)

if exist %_gdaluser_% ( 
  del %_gdaluser_%
)

if exist %_gdalfltr_% ( 
  del %_gdalfltr_%
)

if exist %_testproj_% (
  del %_testproj_%
)

if exist %_testuser_% ( 
  del %_testuser_%
)

if exist %_testfltr_% ( 
  del %_testfltr_%
)

:: **********************************************
:: Generate project user files (.vcxproj.user)
:: **********************************************

echo ^<?xml version="1.0" encoding="utf-8"?^>                   >> %_gdaluser_%
echo ^<Project ToolsVersion="%_vcver_%" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>   >> %_gdaluser_%
echo     ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'"^>  >> %_gdaluser_%
echo       ^<DebuggerFlavor^>WindowsLocalDebugger^</DebuggerFlavor^>                       >> %_gdaluser_%
echo       ^<LocalDebuggerDebuggerType^>Auto^</LocalDebuggerDebuggerType^>                 >> %_gdaluser_%
echo       ^<LocalDebuggerCommand^>%CD%\apps\gdal_translate.exe^</LocalDebuggerCommand^>   >> %_gdaluser_%
echo       ^<LocalDebuggerCommandArguments^>--formats^</LocalDebuggerCommandArguments^>    >> %_gdaluser_%
echo       ^<LocalDebuggerEnvironment^>CPL_DEBUG=ON                                        >> %_gdaluser_%
echo       GDAL_DATA=%_gdaldir_%\data                                                      >> %_gdaluser_%
echo       ^</LocalDebuggerEnvironment^>                                                   >> %_gdaluser_%
echo     ^</PropertyGroup^>                                     >> %_gdaluser_%
echo ^</Project^>                                               >> %_gdaluser_%

if defined _testdir_ (
echo ^<?xml version="1.0" encoding="utf-8"?^>                   >> %_testuser_%
echo ^<Project ToolsVersion="%_vcver_%" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>   >> %_testuser_%
echo     ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'"^>  >> %_testuser_%
echo       ^<DebuggerFlavor^>WindowsLocalDebugger^</DebuggerFlavor^>                       >> %_testuser_%
echo       ^<LocalDebuggerDebuggerType^>Auto^</LocalDebuggerDebuggerType^>                 >> %_testuser_%
echo       ^<LocalDebuggerCommand^>%_testdir_%\gdal_unit_test.exe^</LocalDebuggerCommand^> >> %_testuser_%
echo       ^<LocalDebuggerCommandArguments^>^</LocalDebuggerCommandArguments^>             >> %_testuser_%
echo       ^<LocalDebuggerEnvironment^>CPL_DEBUG=ON                                        >> %_testuser_%
echo       GDAL_DATA=%_gdaldir_%\data                                                      >> %_testuser_%
echo       ^</LocalDebuggerEnvironment^>                                                   >> %_testuser_%
echo     ^</PropertyGroup^>                                     >> %_testuser_%
echo ^</Project^>                                               >> %_testuser_%
)
:: if defined _testdir_

:: **********************************************
:: Initialize filters files (.vcxproj.filters)
:: **********************************************

echo ^<?xml version="1.0" encoding="utf-8"?^>                   >> %_gdalfltr_%
echo ^<Project ToolsVersion="5.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>         >> %_gdalfltr_%

:: **********************************************
:: Main file generator (.vcxproj) and filters file (.vcxproj.filters)
:: **********************************************

echo ^<?xml version="1.0" encoding="utf-8"?^>                   >> %_gdalproj_%
echo ^<Project DefaultTargets="Build" ToolsVersion="%_vcver_%" xmlns="http://schemas.microsoft.com/developer/msbuild/2003"^>          >> %_gdalproj_%
echo   ^<ItemGroup Label="ProjectConfigurations"^>              >> %_gdalproj_%
echo     ^<ProjectConfiguration Include="Debug|%_winver_%"^>    >> %_gdalproj_%
echo       ^<Configuration^>Debug^</Configuration^>             >> %_gdalproj_%
echo       ^<Platform^>%_winver_%^</Platform^>                  >> %_gdalproj_%
echo     ^</ProjectConfiguration^>                              >> %_gdalproj_%
echo     ^<ProjectConfiguration Include="Release|%_winver_%"^>  >> %_gdalproj_%
echo       ^<Configuration^>Release^</Configuration^>           >> %_gdalproj_%
echo       ^<Platform^>%_winver_%^</Platform^>                  >> %_gdalproj_%
echo     ^</ProjectConfiguration^>                              >> %_gdalproj_%
echo   ^</ItemGroup^>                                           >> %_gdalproj_% 
echo   ^<PropertyGroup Label="Globals"^>                        >> %_gdalproj_%
echo     ^<Keyword^>MakeFileProj^</Keyword^>                    >> %_gdalproj_%
echo   ^</PropertyGroup^>                                       >> %_gdalproj_%
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" /^>  >> %_gdalproj_%   
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'" Label="Configuration"^>    >> %_gdalproj_%
echo     ^<ConfigurationType^>Makefile^</ConfigurationType^>    >> %_gdalproj_%
echo     ^<UseDebugLibraries^>true^</UseDebugLibraries^>        >> %_gdalproj_%
echo     ^<PlatformToolset^>%_vstoolset_%^</PlatformToolset^>            >> %_gdalproj_%
echo   ^</PropertyGroup^>                                       >> %_gdalproj_%
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|%_winver_%'" Label="Configuration"^>  >> %_gdalproj_%
echo     ^<ConfigurationType^>Makefile^</ConfigurationType^>    >> %_gdalproj_%
echo     ^<UseDebugLibraries^>true^</UseDebugLibraries^>        >> %_gdalproj_%
echo     ^<PlatformToolset^>%_vstoolset_%^</PlatformToolset^>            >> %_gdalproj_%
echo   ^</PropertyGroup^>                                       >> %_gdalproj_%
echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" /^>      >> %_gdalproj_%
echo   ^<ImportGroup Label="ExtensionSettings"^>                >> %_gdalproj_%  
echo   ^</ImportGroup^>                                         >> %_gdalproj_%
echo   ^<ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'"^>     >> %_gdalproj_% 
echo     ^<Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" /^>    >> %_gdalproj_%
echo   ^</ImportGroup^>                                         >> %_gdalproj_%
echo   ^<ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Release|%_winver_%'"^>  >> %_gdalproj_%
echo     ^<Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" /^>     >> %_gdalproj_%
echo   ^</ImportGroup^>                                         >> %_gdalproj_%
echo   ^<PropertyGroup Label="UserMacros" /^>                   >> %_gdalproj_%
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|%_winver_%'"^>     >> %_gdalproj_%
echo     ^<NMakeBuildCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% DEBUG=1 WITH_PDB=1^</NMakeBuildCommandLine^>            >> %_gdalproj_%
echo     ^<NMakeOutput^>^</NMakeOutput^>                                                    >> %_gdalproj_%
echo     ^<NMakeCleanCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% DEBUG=1 WITH_PDB=1 clean^</NMakeCleanCommandLine^>      >> %_gdalproj_%
echo     ^<NMakeReBuildCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% DEBUG=1 WITH_PDB=1 clean ^&amp;^&amp; nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% DEBUG=1 WITH_PDB=1^</NMakeReBuildCommandLine^>  >> %_gdalproj_%
echo     ^<NMakePreprocessorDefinitions^>%_winver_%;_DEBUG;$(NMakePreprocessorDefinitions)^</NMakePreprocessorDefinitions^>   >> %_gdalproj_%
echo     ^<LibraryPath^>$(VC_LibraryPath_%_buildplatf_%);$(WindowsSDK_LibraryPath_%_buildplatf_%);$(VC_SourcePath);^</LibraryPath^>   >> %_gdalproj_%
echo   ^</PropertyGroup^>                                                                   >> %_gdalproj_%
echo   ^<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|%_winver_%'"^>   >> %_gdalproj_%
echo     ^<NMakeBuildCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% ^</NMakeBuildCommandLine^>            >> %_gdalproj_%
echo     ^<NMakeOutput^>^</NMakeOutput^>                                                    >> %_gdalproj_%
echo     ^<NMakeCleanCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% clean^</NMakeCleanCommandLine^>      >> %_gdalproj_%
echo     ^<NMakeReBuildCommandLine^>nmake -f makefile.vc MSVC_VER=%_clver_% %_nmake_opt_win64_% clean ^&amp;^&amp; nmake -f makefile.vc MSVC_VER=%_clver_%^</NMakeReBuildCommandLine^>  >> %_gdalproj_%
echo     ^<NMakePreprocessorDefinitions^>%_winver_%;NDEBUG;$(NMakePreprocessorDefinitions)^</NMakePreprocessorDefinitions^>   >> %_gdalproj_%
echo     ^<LibraryPath^>$(VC_LibraryPath_%_buildplatf_%);$(WindowsSDK_LibraryPath_%_buildplatf_%);$(VC_SourcePath);^</LibraryPath^>   >> %_gdalproj_%
echo   ^</PropertyGroup^>                                                                   >> %_gdalproj_%
echo   ^<ItemDefinitionGroup^>                                  >> %_gdalproj_%
echo   ^</ItemDefinitionGroup^>                                 >> %_gdalproj_%

:: create a root path with nmake files

echo   ^<ItemGroup^>                                            >> %_gdalproj_%
echo       ^<Test Include="%_gdaldir_%\nmake.local" /^>                     >> %_gdalproj_%
echo       ^<Test Include="%_gdaldir_%\nmake.opt" /^>                       >> %_gdalproj_%
echo   ^</ItemGroup^>                                           >> %_gdalproj_%

echo   ^<ItemGroup^>                                            >> %_gdalfltr_%
echo       ^<Test Include="%_gdaldir_%\nmake.local" /^>                     >> %_gdalfltr_%
echo       ^<Test Include="%_gdaldir_%\nmake.opt" /^>                       >> %_gdalfltr_%
echo   ^</ItemGroup^>                                           >> %_gdalfltr_%

:: Use base of main .vcxproj and .filters as template for test project

if defined _testdir_ (
    copy /Y %_gdalproj_% %_testproj_% >NUL
    copy /Y %_gdalfltr_% %_testfltr_% >NUL
)
:: if defined _testdir_

:: create main project filters only

echo   ^<ItemGroup^>                                            >> %_gdalfltr_%
call :create_filter %_gdaldir_% "*.vc;" 1 "Make Files" "None" %_gdalproj_% %_gdalfltr_%
call :create_filter %_gdaldir_% "*.h;*.hpp" 1 "Include Files" "ClInclude" %_gdalproj_% %_gdalfltr_%
call :create_filter %_gdaldir_% "*.c;*.cpp" 1 "Source Files" "ClCompile" %_gdalproj_% %_gdalfltr_%
echo   ^</ItemGroup^>                                           >> %_gdalfltr_%

:: create test project filters only

if defined _testdir_ (
echo   ^<ItemGroup^>                                            >> %_testfltr_%
call :create_filter %_testdir_% "*.vc;" 1 "Make Files" "None" %_testproj_% %_testfltr_%
call :create_filter %_testdir_% "*.h;*.hpp" 1 "Include Files" "ClInclude" %_testproj_% %_testfltr_%
call :create_filter %_testdir_% "*.c;*.cpp" 1 "Source Files" "ClCompile" %_testproj_% %_testfltr_%
echo   ^</ItemGroup^>                                           >> %_testfltr_%
)
:: if defined _testdir_

:: create main project and links to filters

echo   ^<ItemGroup^>                                            >> %_gdalproj_%
echo   ^<ItemGroup^>                                            >> %_gdalfltr_%
call :create_filter %_gdaldir_% "*.vc" 0 "Make Files" "None" %_gdalproj_% %_gdalfltr_%
echo   ^</ItemGroup^>                                           >> %_gdalproj_%
echo   ^</ItemGroup^>                                           >> %_gdalfltr_%

echo   ^<ItemGroup^>                                            >> %_gdalproj_%
echo   ^<ItemGroup^>                                            >> %_gdalfltr_%
call :create_filter %_gdaldir_% "*.h;*.hpp" 0 "Include Files" "ClInclude" %_gdalproj_% %_gdalfltr_%
echo   ^</ItemGroup^>                                           >> %_gdalproj_%
echo   ^</ItemGroup^>                                           >> %_gdalfltr_%

echo   ^<ItemGroup^>                                            >> %_gdalproj_%
echo   ^<ItemGroup^>                                            >> %_gdalfltr_%
call :create_filter %_gdaldir_% "*.c;*.cpp" 0 "Source Files" "ClCompile" %_gdalproj_% %_gdalfltr_%
echo   ^</ItemGroup^>                                           >> %_gdalproj_%
echo   ^</ItemGroup^>                                           >> %_gdalfltr_%

:: create test project and links to filters

if defined _testdir_ (
echo   ^<ItemGroup^>                                            >> %_testproj_%
echo   ^<ItemGroup^>                                            >> %_testfltr_%
call :create_filter %_testdir_% "*.vc" 0 "Make Files" "None" %_testproj_% %_testfltr_%
echo   ^</ItemGroup^>                                           >> %_testproj_%
echo   ^</ItemGroup^>                                           >> %_testfltr_%

echo   ^<ItemGroup^>                                            >> %_testproj_%
echo   ^<ItemGroup^>                                            >> %_testfltr_%
call :create_filter %_testdir_% "*.h;*.hpp" 0 "Include Files" "ClInclude" %_testproj_% %_testfltr_%
echo   ^</ItemGroup^>                                           >> %_testproj_%
echo   ^</ItemGroup^>                                           >> %_testfltr_%

echo   ^<ItemGroup^>                                            >> %_testproj_%
echo   ^<ItemGroup^>                                            >> %_testfltr_%
call :create_filter %_testdir_% "*.c;*.cpp" 0 "Source Files" "ClCompile" %_testproj_% %_testfltr_%
echo   ^</ItemGroup^>                                           >> %_testproj_%
echo   ^</ItemGroup^>                                           >> %_testfltr_%
)
:: if defined _testdir_

:: **********************************************
:: Finalize main and test projects
:: **********************************************

echo ^</Project^>                                               >> %_gdalfltr_%

echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets"/^>  >> %_gdalproj_%
echo ^</Project^>                                               >> %_gdalproj_%

if defined _testdir_ (
echo ^</Project^>                                               >> %_testfltr_%

echo   ^<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets"/^>  >> %_testproj_%
echo ^</Project^>                                               >> %_testproj_%
)
:: if defined _testdir_ (

:: *******************************************************
:: Generate .sln file with main and test projects attached
:: *******************************************************

echo Projects done!

echo Launch Visual Studio IDE
echo * Open project %_gdalproj_%
echo * Add  project %_testproj_%
echo * Configure Build Dependencies to build the main project before the test project.
echo * Save solution in .sln file
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
    set _proj_=%6
    set _fltr_=%7
    
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

    for %%d in (ci data debian doc html m4 pymod scripts swig) do (
        if "%_folder_%"=="%%d" (
            goto :end
        )
    )
    ::  *********************
    ::  Check if a folder is empty
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
        echo     ^<Filter Include="%_name_%"^>                   >> %_fltr_%
        echo       ^<Extensions^>%_mask_%^</Extensions^>         >> %_fltr_%
        echo     ^</Filter^>                                     >> %_fltr_%
    )

    ::  *********************
    ::  Add files
    ::  *********************
    
    if %_fonly_%==0 (
        for %%f in (%_mask_%) do (
            echo     ^<%_item_% Include="%_path_%\%%f"/^>        >> %_proj_%
            echo     ^<%_item_% Include="%_path_%\%%f"^>         >> %_fltr_%
            echo       ^<Filter^>%_name_%^</Filter^>             >> %_fltr_%
            echo     ^</%_item_%^>                               >> %_fltr_% 
        )
    )
  
    ::  *********************
    ::  Clib all the branches recursively
    ::  *********************

    for /D %%d in (%_path_%\*) do (
        cd %%d
        call :create_filter %%d "%_mask_%" %_fonly_% "%_name_%\%%~nxd" %_item_% %_proj_% %_fltr_%
        cd ..
    )

:: **********************************************
:end
:: **********************************************
