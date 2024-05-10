# Distributed under the GDAL/OGR MIT style License.  See accompanying file LICENSE.TXT.

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
check_function_exists(getcwd HAVE_GETCWD)

check_include_file("fcntl.h" HAVE_FCNTL_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("locale.h" HAVE_LOCALE_H)
check_include_file("xlocale.h" HAVE_XLOCALE_H)
check_include_file("direct.h" HAVE_DIRECT_H)
check_include_file("dlfcn.h" HAVE_DLFCN_H)

check_type_size("int" SIZEOF_INT)
check_type_size("unsigned long" SIZEOF_UNSIGNED_LONG)
check_type_size("long int" SIZEOF_LONG_INT)
check_type_size("void*" SIZEOF_VOIDP)
check_type_size("size_t" SIZEOF_SIZE_T)

check_function_exists(ctime_r HAVE_CTIME_R)
check_function_exists(gmtime_r HAVE_GMTIME_R)
check_function_exists(localtime_r HAVE_LOCALTIME_R)

if(MSVC AND NOT BUILD_SHARED_LIBS)
  set(CPL_DISABLE_DLL 1)
endif()

if (MSVC)
  set(HAVE_VSNPRINTF 1)

  set(HAVE_FCNTL_H 1)
  set(HAVE_UNISTD_H 0)
  set(HAVE_SYS_TYPES_H 1)
  set(HAVE_LOCALE_H 1)
  set(HAVE_SEARCH_H 1)
  set(HAVE_DIRECT_H 1)

  set(HAVE_DLFCN_H 1)

  set(VSI_STAT64 _stat64)
  set(VSI_STAT64_T __stat64)

  # Condition compilation of port/cpl_aws_win32.cpp
  check_include_file_cxx("atlbase.h" HAVE_ATLBASE_H)
  if (NOT HAVE_ATLBASE_H)
    message(WARNING "Missing atlbase.h header: cpl_aws_win32.cpp (detection of AWS EC2 Windows hosts) will be missing")
  endif()

else ()
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
  mark_as_advanced(M_LIB)

  option(GDAL_USE_CPL_MULTIPROC_PTHREAD "Set to ON if you want to use pthreads based multiprocessing support."
         ${_WITH_PT_OPTION_ON})
  mark_as_advanced(GDAL_USE_CPL_MULTIPROC_PTHREAD)
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
        int main() { pthread_spinlock_t spin; return pthread_spin_lock(&spin); }
        "
    HAVE_PTHREAD_SPIN_LOCK)

  check_c_source_compiles(
    "
        #define _GNU_SOURCE
        #include <sys/mman.h>
        int main() { return (mremap(0,0,0,0,0) != 0); }
        "
    HAVE_5ARGS_MREMAP)

  check_c_source_compiles(
    "
        #include <pthread.h>
        int main() { pthread_atfork(0,0,0); 0; }
        "
    HAVE_PTHREAD_ATFORK)

  check_include_file("sys/stat.h" HAVE_SYS_STAT_H)

  check_function_exists(readlink HAVE_READLINK)
  check_function_exists(posix_spawnp HAVE_POSIX_SPAWNP)
  check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)
  check_function_exists(vfork HAVE_VFORK)
  check_function_exists(mmap HAVE_MMAP)
  check_function_exists(sigaction HAVE_SIGACTION)
  check_function_exists(statvfs HAVE_STATVFS)
  check_function_exists(statvfs64 HAVE_STATVFS64)
  check_function_exists(lstat HAVE_LSTAT)

  check_function_exists(getrlimit HAVE_GETRLIMIT)
  check_symbol_exists(RLIMIT_AS "sys/resource.h" HAVE_RLIMIT_AS)

  # For internal libjson-c
  check_include_file(sys/random.h HAVE_SYS_RANDOM_H)
  if (HAVE_SYS_RANDOM_H)
    check_symbol_exists(getrandom "sys/random.h" HAVE_GETRANDOM)
  endif()

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
        int main() { struct _stat64 buf; wchar_t path = 0; _wstat64( &path, &buf ); return 0; }
    "
    WINDOWS_STAT64)

  if (WINDOWS_STAT64)
    set(VSI_STAT64 _stat64)
    set(VSI_STAT64_T _stat64)
  endif ()

  check_function_exists(fopen64 HAVE_FOPEN64)
  if (HAVE_FOPEN64)
    set(VSI_FOPEN64 "fopen64")
  else ()
    set(VSI_FOPEN64 "fopen")
  endif ()

  check_type_size("off_t" SIZEOF_OFF_T)

  check_function_exists(pread64 HAVE_PREAD64)

  check_function_exists(ftruncate64 HAVE_FTRUNCATE64)
  if (HAVE_FTRUNCATE64)
    set(VSI_FTRUNCATE64 "ftruncate64")
  else ()
    set(VSI_FTRUNCATE64 "ftruncate")
  endif ()

  # For some reason, above tests detect xxxx64 symbols for iOS, which are not
  # available at build time.
  # This is also necessary for Mac Catalyst builds.
  # Cf https://lists.osgeo.org/pipermail/gdal-dev/2022-August/056174.html
  if (${CMAKE_SYSTEM_NAME} MATCHES "iOS|visionOS|tvOS|watchOS|Darwin")
    set(VSI_FOPEN64 "fopen")
    set(VSI_FTRUNCATE64 "ftruncate")
    set(VSI_FTELL64 "ftell")
    set(VSI_FSEEK64 "fseek")
    set(VSI_STAT64 "stat")
    set(VSI_STAT64_T "stat")
    unset(HAVE_FOPEN64)
    unset(HAVE_FOPEN64 CACHE)
    unset(HAVE_FTRUNCATE64)
    unset(HAVE_FTRUNCATE64 CACHE)
    unset(HAVE_FTELL64)
    unset(HAVE_FTELL64 CACHE)
    unset(HAVE_FSEEK64)
    unset(HAVE_FSEEK64 CACHE)
    unset(HAVE_STATVFS64)
    unset(HAVE_STATVFS64 CACHE)
    unset(HAVE_PREAD64)
    unset(HAVE_PREAD64 CACHE)
  endif()

  if( NOT HAVE_PREAD64 )
    check_c_source_compiles(
      "
         #include <sys/types.h>
         #include <sys/uio.h>
         #include <unistd.h>
         int main() { pread(0, NULL, 0, 0); return 0; }
        "
      HAVE_PREAD_BSD)
  endif()

  set(UNIX_STDIO_64 TRUE)

  set(INCLUDE_XLOCALE_H)
  if(HAVE_XLOCALE_H)
    set(INCLUDE_XLOCALE_H "#include <xlocale.h>")
  endif()
  check_c_source_compiles(
    "
        #define _XOPEN_SOURCE 700
        #include <locale.h>
        ${INCLUDE_XLOCALE_H}
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
        int main(int argc, char** argv) { (void)__builtin_bswap32(0); (void)__builtin_bswap64(0); return 0; }
    "
    HAVE_GCC_BSWAP)

  check_c_source_compiles(
    "
        #include <unistd.h>
        int main () { return (sysconf(_SC_PHYS_PAGES)); return 0; }
    "
    HAVE_SC_PHYS_PAGES)

  check_c_source_compiles(
    "
        #define _GNU_SOURCE
        #include <sched.h>
        int main () { return sched_getaffinity(0,0,0); }
    "
    HAVE_SCHED_GETAFFINITY)

  include(FindInt128)
  if (INT128_FOUND)
    set(HAVE_UINT128_T TRUE)
  endif ()

  if (HAVE_HIDE_INTERNAL_SYMBOLS)
    option(GDAL_HIDE_INTERNAL_SYMBOLS "Set to ON to hide internal symbols." ON)
    if (GDAL_HIDE_INTERNAL_SYMBOLS)
      set(USE_GCC_VISIBILITY_FLAG TRUE)
    endif ()
  endif ()

  message(STATUS "checking if sprintf can be overloaded for GDAL compilation")
  check_cxx_source_compiles(
    "#define _XOPEN_SOURCE\n#include <vector>\n#include <stdio.h>\nextern \"C\"\n {int sprintf(char *str, const char* fmt, ...);}"
    DEPRECATE_SPRINTF)
  if (NOT DEPRECATE_SPRINTF)
    set(DONT_DEPRECATE_SPRINTF 1)
    add_definitions(-DDONT_DEPRECATE_SPRINTF)
  endif ()

  check_cxx_source_compiles(
    "
    #include <shared_mutex>
    int main(int argc, const char * argv[]) {
        std::shared_mutex smtx;
        smtx.lock_shared();
        smtx.unlock_shared();
        return 0;
    }
    "
    HAVE_SHARED_MUTEX
  )

  check_include_file("linux/userfaultfd.h" HAVE_USERFAULTFD_H)
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(MACOSX_FRAMEWORK ${GDAL_ENABLE_MACOSX_FRAMEWORK})
else ()
  set(MACOSX_FRAMEWORK OFF)
endif ()

# vim: ts=4 sw=4 sts=4 et
