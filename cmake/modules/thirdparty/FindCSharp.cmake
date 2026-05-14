#
# A CMake Module for finding and using C# (.NET ).
#
# The following variables are set:
#   CSHARP_FOUND - set to ON if C# is found
#   CSHARP_VERSION - the version of the C# compiler (eg. "v4.0" or "2.10.2")
#   CSHARP_COMPILER - the path to the C# compiler executable (eg. "C:/Windows/Microsoft.NET/Framework/v4.0.30319/csc.exe" or "/usr/bin/gmcs")
#
# This file is based on the work of GDCM:
#   http://gdcm.svn.sf.net/viewvc/gdcm/trunk/CMake/FindCSharp.cmake
# Copyright (c) 2006-2010 Mathieu Malaterre <mathieu.malaterre@gmail.com>
#

# Make sure find package macros are included
include( FindPackageHandleStandardArgs )

unset( CSHARP_COMPILER CACHE )
unset( CSHARP_VERSION CACHE )
unset( CSHARP_FOUND CACHE )

find_package( Dotnet )

if( DOTNET_FOUND )
  set( CSHARP_FOUND ON CACHE BOOL "C# compiler found" FORCE )
  set( CSHARP_VERSION ${DOTNET_VERSION} CACHE STRING "C# .NET compiler version" FORCE )
  set( CSHARP_COMPILER ${DOTNET_EXE} CACHE STRING "Full path to .NET compiler" FORCE )
  set( CSHARP_INTERPRETER "" CACHE INTERNAL "Interpreter not required for .NET" FORCE )
else( )
  set( CSHARP_FOUND OFF CACHE BOOL "C# compiler found" FORCE )
  return()
endif( )

FIND_PACKAGE_HANDLE_STANDARD_ARGS(CSharp CSHARP_VERSION CSHARP_COMPILER)

mark_as_advanced( CSHARP_VERSION CSHARP_COMPILER )