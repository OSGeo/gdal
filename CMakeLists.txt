# CMake4GDAL project is distributed under MIT license. See accompanying file LICENSE.txt.
include(cmake/helpers/GdalCMakeMinimumRequired.cmake)
cmake_minimum_required(VERSION ${GDAL_CMAKE_VERSION_MIN}...${GDAL_CMAKE_VERSION_MAX})

project(gdal LANGUAGES C CXX)

set(GDAL_LIB_TARGET_NAME GDAL)

#
# setup cmake modules paths
include(cmake/modules/init.cmake)

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

include(GdalCAndCXXStandards)
include(CheckCompilerMachineOption)
include(CheckCompilerSIMDFeature)

option(USE_PRECOMPILED_HEADERS "Use precompiled headers (does not work with MSVC)" OFF)
if (USE_PRECOMPILED_HEADERS AND MSVC)
  message(FATAL_ERROR "USE_PRECOMPILED_HEADERS=ON does not currently work with Visual Studio C++ compiler")
endif()

include(Ccache)
include(CheckCXXSourceCompiles)

#
if ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "(i586|i686|x86|AMD64)")
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
    if (NOT HAVE_SSSE3_AT_COMPILE_TIME)
        set(HAVE_SSSE3_AT_COMPILE_TIME 1)
        add_definitions(-DHAVE_SSSE3_AT_COMPILE_TIME)
    endif()
    if (NOT ${flag} STREQUAL " ")
      set(GDAL_AVX2_FLAG ${flag})
    endif ()
  endif ()

  check_compiler_machine_option(flag FMA)
  if (NOT ${flag} STREQUAL "")
    set(HAVE_FMA_AT_COMPILE_TIME 1)
    if (NOT ${flag} STREQUAL " ")
      set(GDAL_FMA_FLAG ${flag})
    endif ()
  endif ()

  check_cxx_source_compiles("
    #include <immintrin.h>
    int main() {
        __m256d a,b,c;
        c = _mm256_fmadd_pd(a,b,c);
        return 0;
    }" HAVE_AVX2_FMA_WITHOUT_FLAG)

else()

  # Check ability to use Arm Neon optimizations
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/gcore")
  check_cxx_source_compiles("#include \"include_sse2neon.h\"\nint main() { return 0; }" SSE2NEON_COMPILES)
  cmake_pop_check_state()

endif ()

# Check whether std::float16_t is available and is working
include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
  "
    #include <cmath>
    #include <stdfloat>
    int main() {
      std::float16_t x = 0;
      using std::nextafter;
      std::float16_t y = nextafter(x, x);
      return y == 0 ? 0 : 1;
    }
  "
  HAVE_STD_FLOAT16_T
)

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
#   $ cmake -DPython_LOOKUP_VERSION=3.8.0 ..
#   $ cmake -DPython_FIND_VIRTUALENV=ONLY ..
#   $ cmake -DPython_ROOT=C:\Python38 ..
# ~~~
#
include(FeatureSummary)

find_package(SWIG)
set_package_properties(
  SWIG PROPERTIES
  DESCRIPTION
    "Software development tool that connects programs written in C and C++ with a variety of high-level programming languages."
  URL "http://swig.org/"
  TYPE RECOMMENDED)

# Workaround Intel MKL library outputting a message in stdout, which cause
# incorrect detection of numpy.get_include() and numpy.__version__
# See https://github.com/numpy/numpy/issues/23775
# Fix also proposed to upstream CMake FindPython per
# https://gitlab.kitware.com/cmake/cmake/-/merge_requests/9359/
if(DEFINED ENV{MKL_ENABLE_INSTRUCTIONS})
  set(BACKUP_ENV_VAR_MKL_ENABLE_INSTRUCTIONS ENV{MKL_ENABLE_INSTRUCTIONS})
endif()
set(ENV{MKL_ENABLE_INSTRUCTIONS} "SSE4_2")

if (Python_LOOKUP_VERSION)
    set(Python_FIND_STRATEGY VERSION)
    find_package(Python ${Python_LOOKUP_VERSION} EXACT COMPONENTS Interpreter Development NumPy)
else ()
    set(Python_FIND_STRATEGY LOCATION)
    find_package(Python 3.8 COMPONENTS Interpreter Development NumPy)
endif ()
set_package_properties(Python PROPERTIES PURPOSE "SWIG_PYTHON: Python binding")

if (Python_Interpreter_FOUND)
  file(
    GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/python_executable.txt"
    CONTENT "${Python_EXECUTABLE}")
  if(NOT Python_NumPy_FOUND)
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
  endif()
endif()

# Restore previous value of MKL_ENABLE_INSTRUCTIONS
if(DEFINED BACKUP_ENV_VAR_MKL_ENABLE_INSTRUCTIONS)
  set(ENV{MKL_ENABLE_INSTRUCTIONS} ${BACKUP_ENV_VAR_MKL_ENABLE_INSTRUCTIONS})
else()
  unset(ENV{MKL_ENABLE_INSTRUCTIONS})
endif()


if (SWIG_FOUND AND (Python_Interpreter_FOUND OR Python_FOUND))
  option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
  option(BUILD_PYTHON_STUBS "Generate Python stub files for Python binding" OFF)
else()
  option(BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
endif()

if (BUILD_PYTHON_BINDINGS)
    if (NOT (Python_Interpreter_FOUND OR Python_FOUND))
      message(FATAL_ERROR "Python is a requirement to build the Python bindings")
    endif()
    if (NOT SWIG_FOUND)
        message(FATAL_ERROR "SWIG is a requirement to build the Python bindings")
    endif()
endif()

option(BUILD_TESTING "Build the testing tree." ON)
# Make sure enable_testing() is defined before including gdal.cmake for
# Java and CSharp tests
if (BUILD_TESTING)
  enable_testing()
endif()

#
include(${CMAKE_CURRENT_SOURCE_DIR}/gdal.cmake)

if (BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/autotest")
  # unit tests
  add_subdirectory(autotest/cpp)
  if(BUILD_PYTHON_BINDINGS)
      add_subdirectory(autotest)
  endif()
endif ()
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fuzzers")
    # Google OSS-Fuzz project utilities
    add_subdirectory(fuzzers)
    if (BUILD_TESTING)
        add_subdirectory(fuzzers/tests)
    endif()
endif()
if (BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/perftests")
  add_subdirectory(perftests)
endif ()
# vim: ts=4 sw=4 sts=4 et
