# Distributed under the GDAL/OGR MIT/X style License.  See accompanying file LICENSE.TXT.

#[=======================================================================[.rst:
configure
---------

Configure function for GDAL/OGR and generate gdal_config.h

#]=======================================================================]

# Include all the necessary files for macros
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(TestBigEndian)
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)
# include (CompilerFlags)
include(CheckCXXSymbolExists)

set(GDAL_PREFIX ${CMAKE_INSTALL_PREFIX})

if (CMAKE_GENERATOR_TOOLSET MATCHES "v([0-9]+)_xp")
  add_definitions(-D_WIN32_WINNT=0x0501)
endif ()

check_function_exists(vsnprintf HAVE_VSNPRINTF)
check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(getcwd HAVE_GETCWD)

check_include_file("assert.h" HAVE_ASSERT_H)
check_include_file("fcntl.h" HAVE_FCNTL_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("locale.h" HAVE_LOCALE_H)
check_include_file("errno.h" HAVE_ERRNO_H)
check_include_file("direct.h" HAVE_DIRECT_H)
check_include_file("dlfcn.h" HAVE_DLFCN_H)

check_type_size("int" SIZEOF_INT)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)
check_type_size("void*" SIZEOF_VOIDP)
set(HAVE_LONG_LONG 1)

# check_include_file("ieeefp.h" HAVE_IEEEFP_H) if(HAVE_IEEEFP_H)
set(HAVE_IEEEFP TRUE)
# endif()

if (SQLITE3_FOUND AND NOT WITH_SQLITE3_EXTERNAL)
  set(SQLITE_COL_TEST_CODE
      "#ifdef __cplusplus
extern \"C\"
#endif
char sqlite3_column_table_name ();
int
main ()
{
return sqlite3_column_table_name ();
  return 0;
}
")
  check_c_source_compiles("${SQLITE_COL_TEST_CODE}" SQLITE_HAS_COLUMN_METADATA)
else ()
  set(SQLITE_HAS_COLUMN_METADATA ON)
endif ()

check_function_exists("atoll" HAVE_ATOLL)
check_function_exists("strtof" HAVE_DECL_STRTOF)

if (Iconv_FOUND)
  set(CMAKE_REQUIRED_INCLUDES ${Iconv_INCLUDE_DIR})
  set(CMAKE_REQUIRED_LIBRARIES ${Iconv_LIBRARY})
  if (MSVC)
    set(CMAKE_REQUIRED_FLAGS "/WX")
  else ()
    set(CMAKE_REQUIRED_FLAGS "-Werror")
  endif ()

  set(ICONV_CONST_TEST_CODE
      "#include <stdlib.h>
    #include <iconv.h>
    #ifdef __cplusplus
    extern \"C\"
    #endif

    int main(){
    #if defined(__STDC__) || defined(__cplusplus)
      iconv_t conv = 0;
      char* in = 0;
      size_t ilen = 0;
      char* out = 0;
      size_t olen = 0;
      size_t ret = iconv(conv, &in, &ilen, &out, &olen);
    #else
      size_t ret = iconv();
    #endif
      return 0;
    }")
  if (CMAKE_C_COMPILER_LOADED)
    check_c_source_compiles("${ICONV_CONST_TEST_CODE}" _ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
  elseif (CMAKE_CXX_COMPILER_LOADED)
    check_cxx_source_compiles("${ICONV_CONST_TEST_CODE}" _ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
  endif ()
  if (_ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
    set(ICONV_CPP_CONST "")
  else ()
    set(ICONV_CPP_CONST "const")
  endif ()
  unset(ICONV_CONST_TEST_CODE)
  unset(_ICONV_SECOND_ARGUMENT_IS_NOT_CONST)
  unset(CMAKE_REQUIRED_INCLUDES)
  unset(CMAKE_REQUIRED_LIBRARIES)
  unset(CMAKE_REQUIRED_FLAGS)
endif ()

if (MSVC)
  check_include_file("search.h" HAVE_SEARCH_H)
  check_function_exists(localtime_r HAVE_LOCALTIME_R)
  check_function_exists("fopen64" HAVE_FOPEN64)
  check_function_exists("stat64" HAVE_STAT64)

  if (NOT CPL_DISABLE_STDCALL)
    set(CPL_STDCALL __stdcall)
  endif ()

  set(HAVE_DOPRNT 0)
  set(HAVE_VPRINTF 1)
  set(HAVE_VSNPRINTF 1)
  set(HAVE_SNPRINTF 1)
  if (MSVC_VERSION LESS 1900)
    set(snprintf _snprintf)
  endif ()

  set(HAVE_GETCWD 1)
  if (NOT DEFINED (getcwd))
    set(getcwd _getcwd)
  endif ()

  set(HAVE_ASSERT_H 1)
  set(HAVE_FCNTL_H 1)
  set(HAVE_UNISTD_H 0)
  set(HAVE_STDINT_H 0)
  set(HAVE_SYS_TYPES_H 1)
  set(HAVE_LIBDL 0)
  set(HAVE_LOCALE_H 1)
  set(HAVE_FLOAT_H 1)
  set(HAVE_ERRNO_H 1)
  set(HAVE_SEARCH_H 1)
  set(HAVE_DIRECT_H 1)

  set(HAVE_LOCALTIME_R 0)
  set(HAVE_DLFCN_H 1)
  set(HOST_FILLORDER FILLORDER_LSB2MSB)
  set(HAVE_IEEEFP 1)

  if (NOT __cplusplus)
    if (NOT inline)
      set(inline __inline)
    endif ()
  endif ()

  set(lfind _lfind)

  set(VSI_STAT64 _stat64)
  set(VSI_STAT64_T __stat64)
else (MSVC)
  # linux, mac and mingw/windows
  test_big_endian(WORDS_BIGENDIAN)
  if (MINGW)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
  endif ()
  find_package(Threads)
  if (Threads_FOUND)
    set(_WITH_PT_OPTION_ON TRUE)
  else ()
    set(_WITH_PT_OPTION_ON FALSE)
  endif ()

  find_library(M_LIB m)

  option(GDAL_USE_CPL_MULTIPROC_PTHREAD "Set to ON if you want to use pthreads based multiprocessing support."
         ${_WITH_PT_OPTION_ON})
  set(CPL_MULTIPROC_PTHREAD ${GDAL_USE_CPL_MULTIPROC_PTHREAD})
  check_c_source_compiles(
    "
        #define _GNU_SOURCE
        #include <pthread.h>
        int main() { return (PTHREAD_MUTEX_RECURSIVE); }
        "
    HAVE_PTHREAD_MUTEX_RECURSIVE)

  check_c_source_compiles(
    "
        #define _GNU_SOURCE
        #include <pthread.h>
        int main() { return (PTHREAD_MUTEX_ADAPTIVE_NP); }
        "
    HAVE_PTHREAD_MUTEX_ADAPTIVE_NP)

  check_c_source_compiles(
    "
        #define _GNU_SOURCE
        #include <pthread.h>
        int main() { pthread_spinlock_t spin; return 1; }
        "
    HAVE_PTHREAD_SPINLOCK)

  check_c_source_compiles(
    "
        #define _GNU_SOURCE
        #include <sys/mman.h>
        int main() { return (mremap(0,0,0,0,0)); }
        "
    HAVE_5ARGS_MREMAP)

  check_include_file("inttypes.h" HAVE_INTTYPES_H)

  check_include_file("strings.h" HAVE_STRINGS_H)
  check_include_file("string.h" HAVE_STRING_H)

  check_function_exists(strtof HAVE_STRTOF)

  check_include_file("sys/stat.h" HAVE_SYS_STAT_H)

  check_function_exists(readlink HAVE_READLINK)
  check_function_exists(posix_spawnp HAVE_POSIX_SPAWNP)
  check_function_exists(vfork HAVE_VFORK)
  check_function_exists(mmap HAVE_MMAP)
  check_function_exists(statvfs HAVE_STATVFS)
  check_function_exists(lstat HAVE_LSTAT)
  check_function_exists(ftell64 HAVE_FTELL64)
  if (HAVE_FTELL64)
    set(VSI_FTELL64 "ftell64")
  else ()
    check_function_exists(ftello64 HAVE_FTELLO64)
    if (HAVE_FTELLO64)
      set(VSI_FTELL64 "ftello64")
    endif ()
  endif ()

  check_function_exists(fseek64 HAVE_FSEEK64)
  if (HAVE_FSEEK64)
    set(VSI_FSEEK64 "fseek64")
  else ()
    check_function_exists(fseeko64 HAVE_FSEEKO64)
    if (HAVE_FSEEKO64)
      set(VSI_FSEEK64 "fseeko64")
    endif ()
  endif ()

  if (NOT VSI_FTELL64 AND NOT VSI_FSEEK64)
    check_c_source_compiles(
      "
            #define _LARGEFILE64_SOURCE
            #include <stdio.h>
            int main() { long long off=0; fseeko64(NULL, off, SEEK_SET); off = ftello64(NULL); return 0; }
        "
      VSI_NEED_LARGEFILE64_SOURCE)

    if (VSI_NEED_LARGEFILE64_SOURCE)
      set(VSI_FTELL64 "ftello64")
      set(VSI_FSEEK64 "fseeko64")
    endif ()
  endif ()

  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    if (NOT VSI_FTELL64 AND NOT VSI_FSEEK64)
      set(VSI_FTELL64 "ftello")
      set(VSI_FSEEK64 "fseeko")
    endif ()
    set(VSI_STAT64 stat)
    set(VSI_STAT64_T stat)
  else ()
    if (NOT VSI_FTELL64 AND NOT VSI_FSEEK64)
      check_function_exists(ftello HAVE_FTELLO)
      if (HAVE_FTELLO)
        set(VSI_FTELL64 "ftello")
      endif ()

      check_function_exists(fseeko HAVE_FSEEKO)
      if (HAVE_FSEEKO)
        set(VSI_FSEEK64 "fseeko")
      endif ()
    endif ()
    check_function_exists(stat64 HAVE_STAT64)
    if (HAVE_STAT64)
      set(VSI_STAT64 stat64)
      set(VSI_STAT64_T stat64)
    else ()
      set(VSI_STAT64 stat)
      set(VSI_STAT64_T stat)
    endif ()
  endif ()

  check_c_source_compiles(
    "
        #if defined(__MINGW32__)
        #ifndef __MSVCRT_VERSION__
        #define __MSVCRT_VERSION__ 0x0601
        #endif
        #endif
        #include <sys/types.h>
        #include <sys/stat.h>
        int main() { struct _stat64 buf; _wstat64( \"\", &buf ); return 0; }
    "
    NO_UNIX_STDIO_64)

  if (NO_UNIX_STDIO_64)
    set(VSI_STAT64 _stat64)
    set(VSI_STAT64_T __stat64)
  endif ()

  check_function_exists(fopen64 HAVE_FOPEN64)
  if (HAVE_FOPEN64)
    set(VSI_FOPEN64 "fopen64")
  else ()
    set(VSI_FOPEN64 "fopen")
  endif ()

  check_function_exists(ftruncate64 HAVE_FTRUNCATE64)
  if (HAVE_FTRUNCATE64)
    set(VSI_FTRUNCATE64 "ftruncate64")
  else ()
    set(VSI_FTRUNCATE64 "ftruncate")
  endif ()

  set(UNIX_STDIO_64 TRUE)
  set(VSI_LARGE_API_SUPPORTED TRUE)

  check_c_source_compiles(
    "
        #define _XOPEN_SOURCE 700
        #include <locale.h>
        int main() {
            locale_t alocale = newlocale (LC_NUMERIC_MASK, \"C\", 0);
            locale_t oldlocale = uselocale(alocale);
            uselocale(oldlocale);
            freelocale(alocale);
            return 0;
        }
        "
    HAVE_USELOCALE)

  set(CMAKE_REQUIRED_FLAGS "-fvisibility=hidden")
  check_c_source_compiles(
    "
        int visible() { return 0; } __attribute__ ((visibility(\"default\")))
        int hidden() { return 0; }
        int main() { return 0; }
    "
    HAVE_HIDE_INTERNAL_SYMBOLS)
  unset(CMAKE_REQUIRED_FLAGS)

  check_c_source_compiles(
    "
        int main() { int i; __sync_add_and_fetch(&i, 1); __sync_sub_and_fetch(&i, 1); __sync_bool_compare_and_swap(&i, 0, 1); return 0; }
    "
    HAVE_GCC_ATOMIC_BUILTINS)

  check_c_source_compiles(
    "
        #include <sys/types.h>
        #include <sys/socket.h>
        #include <netdb.h>
        int main() { getaddrinfo(0,0,0,0); return 0; }
    "
    HAVE_GETADDRINFO)

  check_c_source_compiles(
    "
        #include <unistd.h>
        int main () { return (sysconf(_SC_PHYS_PAGES)); return 0; }
    "
    HAVE_SC_PHYS_PAGES)

  include(FindInt128)
  if (INT128_FOUND)
    add_definitions(-DHAVE_UINT128_T)
  endif ()

  if (HAVE_HIDE_INTERNAL_SYMBOLS)
    option(GDAL_HIDE_INTERNAL_SYMBOLS "Set to ON to hide internal symbols." ON)
    if (GDAL_HIDE_INTERNAL_SYMBOLS)
      set(USE_GCC_VISIBILITY_FLAG TRUE)
    endif ()
  endif ()

  if (HAVE_STDDEF_H AND HAVE_STDINT_H)
    set(STDC_HEADERS TRUE)
  endif (HAVE_STDDEF_H AND HAVE_STDINT_H)

  message(STATUS "checking if sprintf can be overloaded for GDAL compilation")
  check_cxx_source_compiles(
    "#define _XOPEN_SOURCE\n#include <vector>\n#include <stdio.h>\nextern \"C\"\n {int sprintf(char *str, const char* fmt, ...);}"
    DEPRECATE_SPRINTF)
  if (NOT DEPRECATE_SPRINTF)
    set(DONT_DEPRECATE_SPRINTF 1)
    add_definitions(-DDONT_DEPRECATE_SPRINTF)
  endif ()

  check_include_file("linux/userfaultfd.h" HAVE_USERFAULTFD_H)
endif ()

if (WORDS_BIGENDIAN)
  set(HOST_FILLORDER FILLORDER_MSB2LSB)
else ()
  set(HOST_FILLORDER FILLORDER_LSB2MSB)
endif ()

if (UNIX)
  if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(MACOSX_FRAMEWORK ON)
  else ()
    set(MACOSX_FRAMEWORK OFF)
  endif ()
endif ()

configure_file(${GDAL_CMAKE_TEMPLATE_PATH}/cpl_config.h.in ${PROJECT_BINARY_DIR}/port/cpl_config.h @ONLY)

# vim: ts=4 sw=4 sts=4 et
