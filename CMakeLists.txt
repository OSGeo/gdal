# CMake4GDAL project is distributed under X/MIT license. See accompanying file LICENSE.txt.
cmake_minimum_required(VERSION 3.10)

# cmake version 3.12 or later
if (POLICY CMP0073)
  # Do not produce legacy _LIB_DEPENDS cache entries.
  cmake_policy(SET CMP0073 NEW)
endif ()
if (POLICY CMP0074)
  # find_package() uses <PackageName>_ROOT variables.
  cmake_policy(SET CMP0074 NEW)
endif ()
if (POLICY CMP0075)
  # Include file check macros honor CMAKE_REQUIRED_LIBRARIES.
  cmake_policy(SET CMP0075 NEW)
endif ()

# cmake version 3.13 or later
if (POLICY CMP0076)
  # The target_sources() command converts relative paths to absolute.
  cmake_policy(SET CMP0076 NEW)
endif ()
if (POLICY CMP0079)
  # target_link_libraries() allows use with targets in other directories.
  cmake_policy(SET CMP0079 NEW)
endif ()

# cmake version 3.15 or later
if (POLICY CMP0090)
  # export(PACKAGE) does not populate package registry by default.
  cmake_policy(SET CMP0090 NEW)
endif ()
if (POLICY CMP0092)
  # MSVC warning flags are not in CMAKE_<LANG>_FLAGS by default.
  cmake_policy(SET CMP0092 NEW)
endif ()
if (POLICY CMP0094)
  # Modules FindPython3, FindPython2 and FindPython use LOCATION for lookup strategy.
  cmake_policy(SET CMP0094 NEW)
endif ()

# cmake version 3.17 or later
if (POLICY CMP0102)
  # The mark_as_advanced() command no longer creates a cache entry if one does not already exist.
  cmake_policy(SET CMP0102 NEW)
endif ()

# cmake version 3.19 or later
if (POLICY CMP0110)
  # add_test() supports arbitrary characters in test names.
  cmake_policy(SET CMP0110 NEW)
endif ()
if (POLICY CMP0112)
  # Target file component generator expressions do not add target dependencies.
  cmake_policy(SET CMP0112 NEW)
endif ()
if (POLICY CMP0113)
  # Makefile Generators do not repeat custom commands from target dependencies.
  cmake_policy(SET CMP0113 NEW)
endif ()

# cmake version 3.20 or later
if (POLICY CMP0115)
  # Source file extensions must be explicit.
  cmake_policy(SET CMP0115 NEW)
endif ()
if (POLICY CMP0116)
  # Ninja generators transform DEPFILE s from add_custom_command().
  cmake_policy(SET CMP0116 NEW)
endif ()
if (POLICY CMP0117)
  # MSVC RTTI flag /GR is not added to CMAKE_CXX_FLAGS by default.
  cmake_policy(SET CMP0117 NEW)
endif ()

# cmake version 3.21 or later
if (POLICY CMP0124)
  # the scope of loop variables defined by the foreach() command is restricted to the loop only
  cmake_policy(SET CMP0124 NEW)
endif ()
if (POLICY CMP0125)
  # The find_file(), find_path(), find_library() and find_program() commands cache their result in the variable specified by their first argument
  cmake_policy(SET CMP0125 NEW)
endif ()
#
project(gdal LANGUAGES C CXX)
#
# setup cmake modules paths
include(cmake/modules/init.cmake)
set(CMAKE_CONFIGURATION_TYPES
    "Debug;Release"
    CACHE STRING "Configs" FORCE)
#
# template files path
set(GDAL_CMAKE_TEMPLATE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/template)
#
# Custom global properties
define_property(
  GLOBAL
  PROPERTY GDAL_FORMATS
  BRIEF_DOCS "Builtin format"
  FULL_DOCS "Builtin format")
define_property(
  GLOBAL
  PROPERTY OGR_FORMATS
  BRIEF_DOCS "Builtin ogr drivers"
  FULL_DOCS "Builtin ogr drivers")
define_property(
  GLOBAL
  PROPERTY PLUGIN_MODULES
  BRIEF_DOCS "Plugin modules"
  FULL_DOCS "Plugin modules")
define_property(
  TARGET
  PROPERTY PLUGIN_OUTPUT_DIR
  BRIEF_DOCS "Plugin modules build directories"
  FULL_DOCS "Plugin modules build directories")
#
# check compiler and set preferences.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#
if (MSVC)
  set(CMAKE_DEBUG_POSTFIX "d")
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  -wd4127 -wd4251 -wd4275 -wd4786 -wd4100 -wd4245 -wd4206 -wd4018 -wd4389")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -wd4127 -wd4251 -wd4275 -wd4786 -wd4100 -wd4245 -wd4206 -wd4018 -wd4389")
  add_definitions(-DNOMINMAX)
endif ()
#
include(CheckCompilerMachineOption)
include(CheckCompilerSIMDFeature)
#
if ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(x86|AMD64)")
  check_compiler_machine_option(flag SSE)
  if (NOT ${flag} EQUAL "")
    set(HAVE_SSE_AT_COMPILE_TIME 1)
    set(GDAL_SSE_FLAG "-msse")
  endif ()

  check_compiler_machine_option(flag SSSE3)
  if (NOT ${flag} EQUAL "")
    set(HAVE_SSSE3_AT_COMPILE_TIME 1)
    set(GDAL_SSSE3_FLAG "-mssse3")
  endif ()

  check_compiler_machine_option(flag AVX)
  if (NOT ${flag} EQUAL "")
    set(HAVE_AVX_AT_COMPILE_TIME 1)
    set(GDAL_AVX_FLAG "-mavx")
  endif ()

endif ()
#
option(CLANG_TIDY_ENABLED "Run clang-tidy with the compiler." OFF)
set(CLANG_TIDY_CHECKS
    "-*,clang-analyzer-alpha.unix.cstring.*"
    CACHE STRING "clang-tidy checks")
set(CLANG_TIDY_WARNINGS_AS_ERRORS
    "clang-analyzer-alpha.unix.cstring.*"
    CACHE STRING "clang-tidy warnings as errors.")
if (RUN_CLANG_TIDY)
  if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "RUN_CLANG_TIDY requires an out-of-source build!")
  endif ()
  find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
  if (NOT CLANG_TIDY_COMMAND)
    message(WARNING "RUN_CLANG_TIDY is ON but clang-tidy is not found!")
    set(CMAKE_CXX_CLANG_TIDY)
  else ()
    set(CMAKE_CXX_CLANG_TIDY
        "${CLANG_TIDY_COMMAND}" "-checks='${CLANG_TIDY_CHECKS}'" "-header-filter='${CMAKE_SOURCE_DIR}/gdal/*'"
        "-warnings-as-errors='${CLANG_TIDY_WARNINGS_AS_ERRORS}'")
  endif ()
endif ()
#
# Developer may want to specify some variable to find proper version.
# ~~~
# Priority is as same order as follows:
#   1. `Python_LOOKUP_VERSION`: specify minimum version to find.
#   2. 'Python_FIND_VIRTUALENV': specify 'ONLY' to use virtualenv activated.
#   3. `Python_ROOT`: specify installed location.
#   4. If nothing specified, use default behavior.
#
# for example
#   $ cmake -DPython_LOOKUP_VERSION=3.6 ..
#   $ cmake -DPython_FIND_VIRTUALENV=ONLY ..
#   $ cmake -DPython_ROOT=C:\Python36 ..
# ~~~
#
include(FeatureSummary)
if (Python_LOOKUP_VERSION)
  set(Python_FIND_STRATEGY VERSION)
  find_package(Python ${Python_LOOKUP_VERSION} COMPONENTS Interpreter Development NumPy)
else ()
  set(Python_FIND_STRATEGY LOCATION)
  find_package(Python COMPONENTS Interpreter Development NumPy)
endif ()
set_package_properties(Python PROPERTIES PURPOSE "SWIG_PYTHON: Python binding")
#
include(gdal.cmake)
# unit tests
add_subdirectory(autotest/cpp)
# please comment out if you want to exclude autotest from distribution.
add_subdirectory(autotest)
# Google OSS-Fuzz project utilities
add_subdirectory(fuzzers)
# Google OSS-Fuzz tests
add_subdirectory(fuzzers/tests)
# vim: ts=4 sw=4 sts=4 et
