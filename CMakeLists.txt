# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.
cmake_minimum_required(VERSION 3.9...3.23)

project(gdal LANGUAGES C CXX)
include(CTest)

set(GDAL_LIB_TARGET_NAME GDAL)

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

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
#
if (MSVC)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-DNOMINMAX)
endif ()
#
include(CheckCompilerMachineOption)
include(CheckCompilerSIMDFeature)
include(Ccache)

#
if ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(x86|AMD64)")
  check_compiler_machine_option(flag SSE)
  if (NOT ${flag} STREQUAL "")
    set(HAVE_SSE_AT_COMPILE_TIME 1)
    add_definitions(-DHAVE_SSE_AT_COMPILE_TIME)
    if (NOT ${flag} STREQUAL " ")
      set(GDAL_SSE_FLAG ${flag})
    endif ()
  endif ()

  check_compiler_machine_option(flag SSE2)
  if (NOT ${flag} STREQUAL "")
    set(HAVE_SSE2_AT_COMPILE_TIME 1)
    if (NOT ${flag} STREQUAL " ")
      set(GDAL_SSE2_FLAG ${flag})
    endif ()
  endif ()

  check_compiler_machine_option(flag SSSE3)
  if (NOT ${flag} STREQUAL "")
    set(HAVE_SSSE3_AT_COMPILE_TIME 1)
    add_definitions(-DHAVE_SSSE3_AT_COMPILE_TIME)
    if (NOT ${flag} STREQUAL " ")
      set(GDAL_SSSE3_FLAG ${flag})
    endif ()
  endif ()

  check_compiler_machine_option(flag SSE4_1)
  if (NOT ${flag} STREQUAL "")
    set(HAVE_SSE41_AT_COMPILE_TIME 1)
    if (NOT ${flag} STREQUAL " ")
      set(GDAL_SSE41_FLAG ${flag})
    endif ()
  endif ()

  check_compiler_machine_option(flag AVX)
  if (NOT ${flag} STREQUAL "")
    set(HAVE_AVX_AT_COMPILE_TIME 1)
    add_definitions(-DHAVE_AVX_AT_COMPILE_TIME)
    if (NOT ${flag} STREQUAL " ")
      set(GDAL_AVX_FLAG ${flag})
    endif ()
  endif ()

  check_compiler_machine_option(flag AVX2)
  if (NOT ${flag} STREQUAL "")
    set(HAVE_AVX2_AT_COMPILE_TIME 1)
    if (NOT ${flag} STREQUAL " ")
      set(GDAL_AVX2_FLAG ${flag})
    endif ()
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
  if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "RUN_CLANG_TIDY requires an out-of-source build!")
  endif ()
  find_program(CLANG_TIDY_COMMAND NAMES clang-tidy)
  if (NOT CLANG_TIDY_COMMAND)
    message(WARNING "RUN_CLANG_TIDY is ON but clang-tidy is not found!")
    set(CMAKE_CXX_CLANG_TIDY)
  else ()
    set(CMAKE_CXX_CLANG_TIDY
        "${CLANG_TIDY_COMMAND}" "-checks='${CLANG_TIDY_CHECKS}'" "-header-filter='${PROJECT_SOURCE_DIR}/*'"
        "-warnings-as-errors='${CLANG_TIDY_WARNINGS_AS_ERRORS}'")
  endif ()
endif ()

if (APPLE)
  # To avoid the issue mentioned at
  # https://stackoverflow.com/questions/36523911/osx-homebrew-cmake-libpng-version-mismatch-issue on github action
  # MacOSX workers that have the Mono framework installed, whose libpng headers will be used instead of the homebrew
  # ones
  get_property(
    result
    CACHE CMAKE_FIND_FRAMEWORK
    PROPERTY TYPE)
  if (NOT result)
    set(CMAKE_FIND_FRAMEWORK LAST)
    message(STATUS "Setting CMAKE_FIND_FRAMEWORK=${CMAKE_FIND_FRAMEWORK}")
  endif ()
endif ()

#
# Developer may want to specify some variable to find proper version.
# ~~~
# Priority is as same order as follows:
#   1. `Python_LOOKUP_VERSION`: specify major.minor version to find.
#   2. 'Python_FIND_VIRTUALENV': specify 'ONLY' to use virtualenv activated.
#   3. `Python_ROOT`: specify installed location.
#   4. If nothing specified, use default behavior.
#
# for example
#   $ cmake -DPython_LOOKUP_VERSION=3.6.0 ..
#   $ cmake -DPython_FIND_VIRTUALENV=ONLY ..
#   $ cmake -DPython_ROOT=C:\Python36 ..
# ~~~
#
include(FeatureSummary)
if (Python_LOOKUP_VERSION)
  set(Python_FIND_STRATEGY VERSION)
  find_package(Python ${Python_LOOKUP_VERSION} EXACT COMPONENTS Interpreter Development NumPy)
else ()
  set(Python_FIND_STRATEGY LOCATION)
  find_package(Python 3.6 COMPONENTS Interpreter Development NumPy)
endif ()
set_package_properties(Python PROPERTIES PURPOSE "SWIG_PYTHON: Python binding")

if (NOT Python_FOUND AND ${CMAKE_VERSION} VERSION_LESS "3.12.0")
  # On Ubuntu 18.04 with Python 3.6, the backported FindPython package we have in cmake/modules/3.16 doesn't manage to
  # find automatically Python (we can make it work by manually setting -DPython_LOOKUP_VERSION=3.6.9
  # -DPython_LIBRARY=/usr/lib/x86_64-linux-gnu/libpython3.6m.so
  # -DPython_NumPy_INCLUDE_DIR=/usr/lib/python3/dist-packages/numpy/core/include/numpy ) but this is a bit painful. So
  # use the deprecated method (since CMake 3.12), which works out of the box in that situation.
  find_package(PythonInterp)
  find_package(PythonLibs)
  if (PYTHONINTERP_FOUND AND "${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR}" VERSION_LESS "3.6")
    set(PYTHONINTERP_FOUND FALSE)
    set(PYTHONLIBS_FOUND FALSE)
  endif()
  if (PYTHONINTERP_FOUND)
    set(Python_Interpreter_FOUND TRUE)
    set(Python_EXECUTABLE ${PYTHON_EXECUTABLE})
    set(Python_VERSION_MAJOR ${PYTHON_VERSION_MAJOR})
    set(Python_VERSION_MINOR ${PYTHON_VERSION_MINOR})

    # Detect numpy
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/get_numpy.py
         "try:\n  import numpy\n  print(numpy.get_include())\nexcept:\n  pass\n")
    execute_process(
      COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/get_numpy.py
      OUTPUT_VARIABLE _numpy_include
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (NOT _numpy_include STREQUAL "")
      set(Python_NumPy_FOUND TRUE)
      set(Python_NumPy_INCLUDE_DIRS ${Python_NumPy_INCLUDE_DIRS})
    endif ()
  endif ()
  if (PYTHONLIBS_FOUND)
    set(Python_Development_FOUND TRUE)
    set(Python_LIBRARIES ${PYTHON_LIBRARIES})
    set(Python_INCLUDE_DIRS ${PYTHON_INCLUDE_DIRS})
  endif ()
endif ()

#
include(${CMAKE_CURRENT_SOURCE_DIR}/gdal.cmake)

if (BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/autotest")
  # unit tests
  add_subdirectory(autotest/cpp)
  add_subdirectory(autotest)
endif ()
# Google OSS-Fuzz project utilities
add_subdirectory(fuzzers)
if (BUILD_TESTING)
  # Google OSS-Fuzz tests
  add_subdirectory(fuzzers/tests)
endif ()
# vim: ts=4 sw=4 sts=4 et
