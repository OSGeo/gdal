dnl ***************************************************************************
dnl $Id$
dnl
dnl Project:  GDAL
dnl Purpose:  Configure source file.
dnl Author:   Frank Warmerdam, warmerdam@pobox.com
dnl
dnl ***************************************************************************
dnl Copyright (c) 2000, Frank Warmerdam
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a
dnl copy of this software and associated documentation files (the "Software"),
dnl to deal in the Software without restriction, including without limitation
dnl the rights to use, copy, modify, merge, publish, distribute, sublicense,
dnl and/or sell copies of the Software, and to permit persons to whom the
dnl Software is furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included
dnl in all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
dnl OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
dnl THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
dnl FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
dnl DEALINGS IN THE SOFTWARE.
dnl ***************************************************************************

dnl Disable configure caching ... it causes lots of hassles.
define([AC_CACHE_LOAD], )
define([AC_CACHE_SAVE], )

dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([GDALmake.opt.in])
AC_SUBST(PACKAGE, gdal)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_HEADERS([generated_headers/cpl_config.h:port/cpl_config.h.in])
AH_TOP([#ifndef CPL_CONFIG_H])
AH_TOP([#define CPL_CONFIG_H])
AH_BOTTOM([#include "cpl_config_extras.h"])
AH_BOTTOM([#endif])

AC_PREREQ([2.69])

dnl Compute the canonical host-system (the system we are building for)
dnl type variable $host
AC_CANONICAL_HOST

dnl Enable as much warnings as possible
AC_LANG_PUSH([C])
AX_COMPILER_VENDOR
AC_LANG_POP([C])
AX_CFLAGS_WARN_ALL(C_WFLAGS)
AC_LANG_PUSH([C++])
AX_COMPILER_VENDOR
AC_LANG_POP([C++])
AX_CXXFLAGS_WARN_ALL(CXX_WFLAGS)

dnl Remove -Wdeclaration-after-statement that is no longer appropriate in C99
C_WFLAGS=`echo "$C_WFLAGS " | sed "s/-Wdeclaration-after-statement //"`

dnl For ICC: it needs -we10006 instead of -Werror to turn unknown options to errors
dnl Some gcc/clang versions might succeed on this test, so also include -Werror in ERROR_ON_UNKNOWN_OPTIONS
AX_CHECK_COMPILE_FLAG([-Werror -we10006],[ERROR_ON_UNKNOWN_OPTIONS="-Werror -we10006"],[ERROR_ON_UNKNOWN_OPTIONS="-Werror"])

dnl A few ICC warnings to turn off
dnl warning #188: enumerated type mixed with another type  (needed on libcsf)
dnl warning #1684: conversion from pointer to same-sized integral type (potential portability problem)  (needed on frmts/mrf)
dnl warning #2259: non-pointer conversion from "size_t={unsigned long}" to "int" may lose significant bits
dnl warning #2304: non-explicit constructor with single argument may cause implicit type conversion
dnl warning #3280: declaration hides member
dnl remark #11074: Inlining inhibited by limit max-size
dnl remark #11076: To get full report use -qopt-report=4 -qopt-report-phase ipo
AX_CHECK_COMPILE_FLAG([-diag-disable 188,1684,2259,2304,3280,11074,11076],[C_WFLAGS="$C_WFLAGS -diag-disable 188,1684,2259,2304,3280,11074,11076" CXX_WFLAGS="$CXX_WFLAGS -diag-disable 188,1684,2259,2304,3280,11074,11076"],,[$ERROR_ON_UNKNOWN_OPTIONS])

AX_CHECK_COMPILE_FLAG([-Wextra],[C_WFLAGS="$C_WFLAGS -Wextra" CXX_WFLAGS="$CXX_WFLAGS -Wextra"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Winit-self],[C_WFLAGS="$C_WFLAGS -Winit-self" CXX_WFLAGS="$CXX_WFLAGS -Winit-self"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wunused-parameter], [C_WFLAGS="$C_WFLAGS -Wunused-parameter" CXX_WFLAGS="$CXX_WFLAGS -Wunused-parameter" NO_UNUSED_PARAMETER_FLAG="-Wno-unused-parameter"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wmissing-prototypes], [C_WFLAGS="$C_WFLAGS -Wmissing-prototypes"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wmissing-declarations], [C_WFLAGS="$C_WFLAGS -Wmissing-declarations"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wformat], [C_WFLAGS="$C_WFLAGS -Wformat" CXX_WFLAGS="$CXX_WFLAGS -Wformat"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wformat -Werror=format-security -Wno-format-nonliteral], [C_WFLAGS="$C_WFLAGS -Werror=format-security -Wno-format-nonliteral" CXX_WFLAGS="$CXX_WFLAGS -Werror=format-security -Wno-format-nonliteral"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wshorten-64-to-32], [C_WFLAGS="$C_WFLAGS -Wshorten-64-to-32" CXX_WFLAGS="$CXX_WFLAGS -Wshorten-64-to-32"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wlogical-op], [C_WFLAGS="$C_WFLAGS -Wlogical-op" CXX_WFLAGS="$CXX_WFLAGS -Wlogical-op" NO_LOGICAL_OP_FLAG="-Wno-logical-op"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wshadow], [C_WFLAGS="$C_WFLAGS -Wshadow" CXX_WFLAGS="$CXX_WFLAGS -Wshadow"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wmissing-include-dirs], [C_WFLAGS="$C_WFLAGS -Wmissing-include-dirs" CXX_WFLAGS="$CXX_WFLAGS -Wmissing-include-dirs"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl Error out on things that will fail with MSVC
AX_CHECK_COMPILE_FLAG([-Werror=vla], [C_WFLAGS="$C_WFLAGS -Werror=vla" CXX_WFLAGS="$CXX_WFLAGS -Werror=vla"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl -Wclobbered is not reliable on most gcc versions
dnl AX_CHECK_COMPILE_FLAG([-Wno-clobbered], [C_WFLAGS="$C_WFLAGS -Wno-clobbered" CXX_WFLAGS="$CXX_WFLAGS -Wno-clobbered"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl Warn when macros __TIME__, __DATE__ or __TIMESTAMP__ are encountered as they might prevent bit-wise-identical reproducible compilations.
AX_CHECK_COMPILE_FLAG([-Wdate-time], [C_WFLAGS="$C_WFLAGS -Wdate-time" CXX_WFLAGS="$CXX_WFLAGS -Wdate-time"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl GCC 6 warnings
AX_CHECK_COMPILE_FLAG([-Wnull-dereference], [C_WFLAGS="$C_WFLAGS -Wnull-dereference" CXX_WFLAGS="$CXX_WFLAGS -Wnull-dereference"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wduplicated-cond], [C_WFLAGS="$C_WFLAGS -Wduplicated-cond" CXX_WFLAGS="$CXX_WFLAGS -Wduplicated-cond"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl GCC 7 warnings
dnl Do not enable yet. Causes warning in alg/gdalthinplate.cpp due to armadillo templates
dnl AX_CHECK_COMPILE_FLAG([-Wduplicated-branches], [C_WFLAGS="$C_WFLAGS -Wduplicated-branches" CXX_WFLAGS="$CXX_WFLAGS -Wduplicated-branches"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl GCC 8 warnings
AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Wextra-semi], [CXX_WFLAGS="$CXX_WFLAGS -Wextra-semi"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AC_LANG_POP([C++])

AX_CHECK_COMPILE_FLAG([-Wno-sign-compare], [NO_SIGN_COMPARE="-Wno-sign-compare"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl clang >= 3.9
AX_CHECK_COMPILE_FLAG([-Wcomma], [C_WFLAGS="$C_WFLAGS -Wcomma" CXX_WFLAGS="$CXX_WFLAGS -Wcomma"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl clang and gcc 5
AX_CHECK_COMPILE_FLAG([-Wfloat-conversion], [C_WFLAGS="$C_WFLAGS -Wfloat-conversion" CXX_WFLAGS="$CXX_WFLAGS -Wfloat-conversion"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl clang >= 3.2
AX_CHECK_COMPILE_FLAG([-Wdocumentation -Wno-documentation-deprecated-sync], [C_WFLAGS="$C_WFLAGS -Wdocumentation -Wno-documentation-deprecated-sync" CXX_WFLAGS="$CXX_WFLAGS -Wdocumentation -Wno-documentation-deprecated-sync"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl C++ specific stuff

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Wunused-private-field], [CXX_WFLAGS="$CXX_WFLAGS -Wunused-private-field"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wmissing-declarations], [CXX_WFLAGS="$CXX_WFLAGS -Wmissing-declarations"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wnon-virtual-dtor], [CXX_WFLAGS="$CXX_WFLAGS -Wnon-virtual-dtor" NO_NON_VIRTUAL_DTOR_FLAG="-Wno-non-virtual-dtor"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Wold-style-cast], [WARN_OLD_STYLE_CAST="-Wold-style-cast"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AX_CHECK_COMPILE_FLAG([-Weffc++], [WARN_EFFCPLUSPLUS="-Weffc++"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl g++-4.8 complain a bit too much with -Weffc++
if test "$WARN_EFFCPLUSPLUS" != ""; then
    SAVED_CXXFLAGS=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $WARN_EFFCPLUSPLUS -Werror"
    AC_MSG_CHECKING([if -Weffc++ should be enabled])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[
    class Base {};
    class A: public Base {};
    ]])],
    [AC_MSG_RESULT([yes])],
    [WARN_EFFCPLUSPLUS=""]
    [AC_MSG_RESULT([no])])
    CXXFLAGS=$SAVED_CXXFLAGS
fi

dnl Clang enables -Woverloaded-virtual if -Wall is defined, but not GCC
AX_CHECK_COMPILE_FLAG([-Woverloaded-virtual], [CXX_WFLAGS="$CXX_WFLAGS -Woverloaded-virtual"],,[$ERROR_ON_UNKNOWN_OPTIONS])

dnl Forbid use of 'or', 'and', ... alias operators
AX_CHECK_COMPILE_FLAG([-fno-operator-names], [CXX_WFLAGS="$CXX_WFLAGS -fno-operator-names"],,[$ERROR_ON_UNKNOWN_OPTIONS])

HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT=no
AX_CHECK_COMPILE_FLAG([-Wzero-as-null-pointer-constant], [CXX_WFLAGS="$CXX_WFLAGS -Wzero-as-null-pointer-constant" HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT=yes],,[$ERROR_ON_UNKNOWN_OPTIONS])
if test "$HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT" = "yes"; then
AC_DEFINE_UNQUOTED(HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT, 1,
          [Define to 1 if the compiler supports -Wzero-as-null-pointer-constant])
fi

AC_LANG_POP([C++])

CXX14_SUPPORT=no
AC_ARG_WITH([cpp14],
	    AS_HELP_STRING([--with-cpp14],
	       [Enable C++14 compiler options]),,)

AC_MSG_CHECKING([if use C++14 compiler options])
if test "$with_cpp14" = "yes"; then
    dnl Check C++ compiler compatibility with C++14
    AC_MSG_CHECKING([C++ compiler compatibility with C++14])
    AX_CXX_COMPILE_STDCXX([14],[noext],[optional])

    if [test "$HAVE_CXX14" = "1"]; then
        CXX14_SUPPORT=yes
    fi
    AC_MSG_RESULT([enabled])
else
    AC_MSG_RESULT([disabled by user])
fi
AC_SUBST(CXX14_SUPPORT,$CXX14_SUPPORT)

if [test "$CXX14_SUPPORT" = "no"]; then
    AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])
fi

dnl Available in GCC 5.1
AC_LANG_PUSH([C++])
    AX_CHECK_COMPILE_FLAG([-Wsuggest-override], [CXX_WFLAGS="$CXX_WFLAGS -Wsuggest-override"],,[$ERROR_ON_UNKNOWN_OPTIONS])
AC_LANG_POP([C++])

dnl Enable -Wimplicit-fallthrough only if C++11 is enabled since CPL_FALLTHROUGH is only active then
AC_LANG_PUSH([C++])
SAVED_CXXFLAGS=$CXXFLAGS
CXXFLAGS="$CXXFLAGS $ERROR_ON_UNKNOWN_OPTIONS -Wimplicit-fallthrough"
AC_MSG_CHECKING([if -Wimplicit-fallthrough can be enabled])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
  [[#if !(__cplusplus >= 201103L && ( (defined(__clang__) && (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >=7))) || ((defined(__GNUC__) && __GNUC__ >= 7)) ) )
    #error "not (C++11 and (clang >= 3.7 or gcc >= 7))"
    #endif]])],
  [CXX_WFLAGS="$CXX_WFLAGS -Wimplicit-fallthrough"]
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])])
CXXFLAGS=$SAVED_CXXFLAGS
AC_LANG_POP([C++])

AC_SUBST(CXX_WFLAGS,$CXX_WFLAGS)
AC_SUBST(C_WFLAGS,$C_WFLAGS)
AC_SUBST(NO_UNUSED_PARAMETER_FLAG,$NO_UNUSED_PARAMETER_FLAG)
AC_SUBST(NO_SIGN_COMPARE,$NO_SIGN_COMPARE)
AC_SUBST(NO_NON_VIRTUAL_DTOR_FLAG,$NO_NON_VIRTUAL_DTOR_FLAG)
AC_SUBST(NO_LOGICAL_OP_FLAG,$NO_LOGICAL_OP_FLAG)
AC_SUBST(WARN_OLD_STYLE_CAST,$WARN_OLD_STYLE_CAST)
AC_SUBST(WARN_EFFCPLUSPLUS,$WARN_EFFCPLUSPLUS)

dnl Checks for programs.

dnl AC_PROG_CC_C99 is deprecated since autoconf 2.70
m4_if(m4_version_compare(m4_defn([AC_AUTOCONF_VERSION]), [2.70]),
     [-1], [AC_PROG_CC_C99], [AC_PROG_CC])

AC_PROG_CXX
LT_INIT([win32-dll])

dnl Check that CXX is really a working compiler
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
  [[#ifndef __cplusplus
    #error "broken C++"
    #endif]])],,
  [CXX=;])
AC_LANG_POP([C++])

if test "x$CXX" = x ; then
  AC_MSG_ERROR("You don't have a working C++ compiler.")
fi

AC_MSG_CHECKING([for std::isnan])
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
  [[#include <cmath>
    int CPLIsNan(float f) { return std::isnan(f); }]])],
  [
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_STD_IS_NAN, 1,
          [Define to 1 if you have the `std::isnan' function.])
  ], [
    AC_MSG_RESULT([no])
  ])
AC_LANG_POP([C++])

dnl switch between libtool and native build system
AC_ARG_WITH(libtool, [  --without-libtool     Don't use libtool to build the library],,)

if test "$with_libtool" = "no"; then
AC_PROG_RANLIB

dnl AC_COMPILER_PIC as expanded by autoconf-2.68 doesn't work for the C++ part with clang++ 3.0 that doesn't like compiling .c programs
dnl so we inline the expanded code and fix it at hand.
dnl AC_COMPILER_PIC
	echo 'void f(){}' > conftest.c
	if test -z "`${CC-cc} $CFLAGS -fPIC -c conftest.c 2>&1`"; then
	  CFLAGS="$CFLAGS -fPIC"
	fi
	echo 'void f(){}' > conftest.cpp
	if test -z "`${CXX-g++} $CXXFLAGS -fPIC -c conftest.cpp 2>&1`"; then
	  CXXFLAGS="$CXXFLAGS -fPIC"
	fi
	rm -f conftest*

dnl Trick so that LD_SHARED test works with sanitizer flags with clang
SAVED_CFLAGS=$CFLAGS
CFLAGS=`echo "$CFLAGS " | sed "s/-fsanitize=undefined //" | sed "s/-fsanitize=address //" | sed "s/-fsanitize=unsigned-integer-overflow //"`
AC_LD_SHARED
CFLAGS=$SAVED_CFLAGS
else
with_libtool=yes
fi
AC_SUBST(HAVE_LIBTOOL,$with_libtool)

dnl Checks for libraries.
AC_CHECK_LIB(dl,dlopen,,,)

dnl Needed on Solaris.
AC_CHECK_LIB(rt,nanosleep,,,)

dnl We don't need to add math library to all targets
case "${host_os}" in
    cygwin* | mingw32* | pw32* | beos* | darwin*)
        ;;
    *)
	AC_CHECK_LIB(m,sin,,,)
        ;;
esac

dnl Checks for header files.
AC_CHECK_HEADERS(fcntl.h unistd.h dlfcn.h locale.h direct.h)

dnl Check for userfaultfd support
AC_CHECK_HEADERS([linux/userfaultfd.h], [ENABLE_UFFD="yes"], [ENABLE_UFFD="no"], [])
AC_SUBST(ENABLE_UFFD,$ENABLE_UFFD)

dnl cpl_vsil_unix_stdio_64.h requires linux/fs.h on Linux, and this isn't
dnl installed by default on Alpine
case "${host_os}" in
    linux*)
        AC_CHECK_HEADERS([linux/fs.h], [], [AC_MSG_ERROR([Unable to find linux/fs.h header])])
        ;;
    *)
        ;;
esac

AC_C_BIGENDIAN

dnl Check for 64 bit file API
AC_UNIX_STDIO_64

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(unsigned long)
AC_CHECK_SIZEOF(void*)

dnl Checks for library functions.
AC_CHECK_FUNCS(vsnprintf)
AC_CHECK_FUNCS(getcwd)
AC_CHECK_FUNCS(readlink)
AC_CHECK_FUNCS(lstat)
AC_CHECK_FUNCS(posix_spawnp)
AC_CHECK_FUNCS(posix_memalign)
AC_CHECK_FUNCS(vfork)
AC_CHECK_FUNCS(mmap)
AC_CHECK_FUNCS(sigaction)
AC_CHECK_FUNCS(statvfs)
AC_CHECK_FUNCS(statvfs64)
AC_CHECK_FUNCS(getrlimit)

AC_MSG_CHECKING([for RLIMIT_AS])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/time.h>
#include <sys/resource.h>
  ]], [[
    return RLIMIT_AS;
  ]])],[
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_RLIMIT_AS, 1,
          [Define to 1 if you have the `RLIMIT_AS' constant.])
  ],[
    AC_MSG_RESULT([no])
])


dnl Make sure at least these are checked under C++.  Prototypes missing on
dnl some platforms.

AC_LANG_PUSH(C++)
AC_CHECK_FUNC_CUSTOM(gmtime_r,[#include <time.h>],[time_t t; struct tm ltime; t = time(0); gmtime_r( &t, &ltime );])
AC_CHECK_FUNC_CUSTOM(localtime_r,[#include <time.h>],[time_t t; struct tm ltime; t = time(0); localtime_r( &t, &ltime );])
AC_LANG_POP(C++)

AC_CHECK_HEADERS([xlocale.h],[GDAL_HAVE_XLOCALE_H=1])
if test "$GDAL_HAVE_XLOCALE_H" = "1"; then
  AC_DEFINE_UNQUOTED(GDAL_HAVE_XLOCALE_H, 1,
            [Define if you have the <xlocale.h> header file.])
fi

AC_MSG_CHECKING([for uselocale])
AC_LANG_PUSH(C++)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _XOPEN_SOURCE 700
#include <locale.h>
  ]], [[
    locale_t alocale = newlocale (LC_NUMERIC_MASK, "C", 0);
    locale_t oldlocale = uselocale(alocale);
    uselocale(oldlocale);
    freelocale(alocale);
    return 0;
  ]])],[
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_USELOCALE, 1,
          [Define to 1 if you have the `uselocale' function.])
  ],[
    AC_MSG_RESULT([no])
])
AC_LANG_POP(C++)


OPT_GDAL_FORMATS=
EXTRA_INCLUDES=

dnl ---------------------------------------------------------------------------
dnl Check for --enable-debug
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([to enable debug build])

AC_ARG_ENABLE(debug,
	      AS_HELP_STRING([--enable-debug],
			     [enable debugging (disabled by default)]),
	      [HAVE_DEBUG=$enableval], [HAVE_DEBUG=yes])

CFLAGS_NOFTRAPV="$CFLAGS"
CXXFLAGS_NOFTRAPV="$CXXFLAGS"

if test "x$enable_debug" = "xyes" ; then

  dnl Try to strip out optimization.

  CXXFLAGS=`echo "$CXXFLAGS " | sed "s/-O2 //"`
  CFLAGS=`echo "$CFLAGS " | sed "s/-O2 //"`
  CXXFLAGS=`echo "$CXXFLAGS" | sed "s/-O //"`
  CFLAGS=`echo "$CFLAGS" | sed "s/-O //"`

  dnl remove existing -g to avoid duplicates.

  CXXFLAGS=`echo "$CXXFLAGS" | sed "s/-g //"`
  CFLAGS=`echo "$CFLAGS" | sed "s/-g //"`

  dnl add -g and -DDEBUG

  CXXFLAGS="$CXXFLAGS -g -DDEBUG"
  CFLAGS="$CFLAGS -g -DDEBUG"

  CFLAGS_NOFTRAPV="$CFLAGS"
  CXXFLAGS_NOFTRAPV="$CXXFLAGS"

  AC_MSG_RESULT([yes, modified CFLAGS="$CFLAGS"])

  # Some clang version on 32 bits have linking error with 64bit operations (undefined reference to `__mulodi4')
  AC_MSG_CHECKING([whether -ftrapv can be used])
  rm -f testftrapv.c
  echo 'int main(int argc, char** argv) { return (long long)argc * argc; } ' > testftrapv.c
  if test -z "`${CC} ${CFLAGS} -ftrapv testftrapv.c -o testftrapv 2>&1`" ; then
    AC_MSG_RESULT([yes])
    CFLAGS="$CFLAGS -ftrapv"
    CXXFLAGS="$CXXFLAGS -ftrapv"
  else
    AC_MSG_RESULT([no])
  fi
  rm -f testftrapv.* testftrapv

else
  AC_MSG_RESULT([no, CFLAGS="$CFLAGS"])
fi

AC_SUBST(CFLAGS_NOFTRAPV,$CFLAGS_NOFTRAPV)
AC_SUBST(CXXFLAGS_NOFTRAPV,$CXXFLAGS_NOFTRAPV)

dnl ---------------------------------------------------------------------------
dnl Check if GCC 4.1 atomic builtins are really available
dnl (some GCC versions are broken w.r.t this on some exotic architectures...)
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([whether GCC 4.1 atomic builtins are available])

rm -f atomicbuiltins.c
echo 'int main(int argc, char** argv) { int i; __sync_add_and_fetch(&i, 1); __sync_sub_and_fetch(&i, 1); __sync_bool_compare_and_swap(&i, 0, 1); return 0; } ' > atomicbuiltins.c
if test -z "`${CC} ${CFLAGS} atomicbuiltins.c -o atomicbuiltins 2>&1`" ; then
    HAVE_GCC_ATOMIC_BUILTINS=yes
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GCC_ATOMIC_BUILTINS,1, [Define to 1 if GCC atomic builtins are available])
else
    HAVE_GCC_ATOMIC_BUILTINS=no
    AC_MSG_RESULT([no])
fi
rm -rf atomicbuiltins.* atomicbuiltins

AC_SUBST(HAVE_GCC_ATOMIC_BUILTINS,$HAVE_GCC_ATOMIC_BUILTINS)

dnl ---------------------------------------------------------------------------
dnl Check if GCC bswap builtins are available
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([whether GCC bswap builtins are available])

rm -f bswapbuiltins.c
echo 'int main(int argc, char** argv) { (void)__builtin_bswap32(0); (void)__builtin_bswap64(0); return 0; } ' >> bswapbuiltins.c
if test -z "`${CC} ${CFLAGS} bswapbuiltins.c -o bswapbuiltins 2>&1`" ; then
    HAVE_GCC_BSWAP=yes
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_GCC_BSWAP,1, [Define to 1 if GCC bswap builtins are available])
else
    HAVE_GCC_BSWAP=no
    AC_MSG_RESULT([no])
fi
rm -rf bswapbuiltins.* bswapbuiltins

AC_SUBST(HAVE_GCC_BSWAP,$HAVE_GCC_BSWAP)

dnl ---------------------------------------------------------------------------
dnl Check if __uint128_t is available and usable
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([for __uint128_t])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
  ]], [[
      unsigned long long v1 = 1024ULL;
      unsigned long long v2 = 0x8000000000000000ULL;
      __uint128_t u = ((__uint128_t)v1)*((__uint128_t)v2);
      return u == 0;
  ]])],[
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_UINT128_T, 1,
          [Define to 1 if the system has the type __uint128_t])
  ],[
    AC_MSG_RESULT([no])
])

dnl ---------------------------------------------------------------------------
dnl Check SSE availability
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(sse,
[  --with-sse[=ARG]        Detect SSE availability for some optimized routines (ARG=yes(default), no)],,)

AC_MSG_CHECKING([whether SSE is available at compile time])

if test "$with_sse" = "yes" -o "$with_sse" = ""; then

    rm -f detectsse.cpp
    echo '#ifdef __SSE__' > detectsse.cpp
    echo '#include <xmmintrin.h>' >> detectsse.cpp
    echo 'void foo() { float fEpsilon = 0.0000000000001f; __m128 xmm_small = _mm_load1_ps(&fEpsilon); }  int main() { return 0; }' >> detectsse.cpp
    echo '#else' >> detectsse.cpp
    echo 'some_error' >> detectsse.cpp
    echo '#endif' >> detectsse.cpp
    if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -o detectsse detectsse.cpp 2>&1`" ; then
        AC_MSG_RESULT([yes])
        SSEFLAGS=""
        HAVE_SSE_AT_COMPILE_TIME=yes
    else
        if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -msse -o detectsse detectsse.cpp 2>&1`" ; then
            AC_MSG_RESULT([yes])
            SSEFLAGS="-msse"
            HAVE_SSE_AT_COMPILE_TIME=yes
        else
            AC_MSG_RESULT([no])
            if test "$with_sse" = "yes"; then
                AC_MSG_ERROR([--with-sse was requested, but SSE is not available])
            fi
        fi
    fi

    dnl On Solaris, the presence of SSE instructions is flagged in the binary
    dnl and prevent it to run on non SSE hardware even if the instructions are
    dnl not executed. So if the user did not explicitly requires SSE, test that
    dnl we can run SSE binaries
    if test "$HAVE_SSE_AT_COMPILE_TIME" = "yes"; then
       case $host_os in
         solaris*)
           AC_MSG_CHECKING([whether SSE is available and needed at runtime])
           if ./detectsse; then
             AC_MSG_RESULT([yes])
           else
             AC_MSG_RESULT([no])
             if test "$with_sse" = "yes"; then
               echo "Caution: the generated binaries will not run on this system."
             else
               echo "Disabling SSE as it is not explicitly required"
               HAVE_SSE_AT_COMPILE_TIME=""
               SSEFLAGS=""
             fi
           fi
           ;;
       esac
    fi

    if test "$HAVE_SSE_AT_COMPILE_TIME" = "yes"; then
        CFLAGS="-DHAVE_SSE_AT_COMPILE_TIME $CFLAGS"
        CXXFLAGS="-DHAVE_SSE_AT_COMPILE_TIME $CXXFLAGS"
    fi

    rm -rf detectsse*
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(SSEFLAGS,$SSEFLAGS)

dnl ---------------------------------------------------------------------------
dnl Check SSSE3 availability
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(ssse3,
[  --with-ssse3[=ARG]        Detect SSSE3 availability for some optimized routines (ARG=yes(default), no)],,)

AC_MSG_CHECKING([whether SSSE3 is available at compile time])

if test "$with_ssse3" = "yes" -o "$with_ssse3" = ""; then

    rm -f detectssse3.cpp
    echo '#ifdef __SSSE3__' > detectssse3.cpp
    echo '#include <tmmintrin.h>' >> detectssse3.cpp
    echo 'void foo() { __m128i xmm_i = _mm_set1_epi16(0); xmm_i = _mm_shuffle_epi8(xmm_i, xmm_i); }  int main() { return 0; }' >> detectssse3.cpp
    echo '#else' >> detectssse3.cpp
    echo 'some_error' >> detectssse3.cpp
    echo '#endif' >> detectssse3.cpp
    if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -o detectssse3 detectssse3.cpp 2>&1`" ; then
        AC_MSG_RESULT([yes])
        SSSE3FLAGS=""
        HAVE_SSSE3_AT_COMPILE_TIME=yes
    else
        if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -mssse3 -o detectssse3 detectssse3.cpp 2>&1`" ; then
            AC_MSG_RESULT([yes])
            SSSE3FLAGS="-mssse3"
            HAVE_SSSE3_AT_COMPILE_TIME=yes
        else
            AC_MSG_RESULT([no])
            if test "$with_ssse3" = "yes"; then
                AC_MSG_ERROR([--with-ssse3 was requested, but SSSE3 is not available])
            fi
        fi
    fi

    dnl On Solaris, the presence of SSSE3 instructions is flagged in the binary
    dnl and prevent it to run on non SSSE3 hardware even if the instructions are
    dnl not executed. So if the user did not explicitly requires SSSE3, test that
    dnl we can run SSSE3 binaries
    if test "$HAVE_SSSE3_AT_COMPILE_TIME" = "yes"; then
       case $host_os in
         solaris*)
           AC_MSG_CHECKING([whether SSSE3 is available and needed at runtime])
           if ./detectssse3; then
             AC_MSG_RESULT([yes])
           else
             AC_MSG_RESULT([no])
             if test "$with_ssse3" = "yes"; then
               echo "Caution: the generated binaries will not run on this system."
             else
               echo "Disabling SSSE3 as it is not explicitly required"
               HAVE_SSSE3_AT_COMPILE_TIME=""
               SSSE3FLAGS=""
             fi
           fi
           ;;
       esac
    fi

    if test "$HAVE_SSSE3_AT_COMPILE_TIME" = "yes"; then
        CFLAGS="-DHAVE_SSSE3_AT_COMPILE_TIME $CFLAGS"
        CXXFLAGS="-DHAVE_SSSE3_AT_COMPILE_TIME $CXXFLAGS"
    fi

    rm -rf detectssse3*
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(SSSE3FLAGS,$SSSE3FLAGS)

dnl ---------------------------------------------------------------------------
dnl Check AVX availability
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(avx,
[  --with-avx[=ARG]        Detect AVX availability for some optimized routines (ARG=yes(default), no)],,)

AC_MSG_CHECKING([whether AVX is available at compile time])

if test "$with_avx" = "yes" -o "$with_avx" = ""; then

    rm -f detectavx.cpp
    echo '#ifdef __AVX__' > detectavx.cpp
    echo '#include <immintrin.h>' >> detectavx.cpp
    echo 'int foo() { unsigned int nXCRLow, nXCRHigh;' >> detectavx.cpp
    echo '__asm__ ("xgetbv" : "=a" (nXCRLow), "=d" (nXCRHigh) : "c" (0));' >> detectavx.cpp
    echo 'float fEpsilon = 0.0000000000001f;' >> detectavx.cpp
    echo '__m256 ymm_small = _mm256_set_ps(fEpsilon,fEpsilon,fEpsilon,fEpsilon,fEpsilon,fEpsilon,fEpsilon,fEpsilon);' >> detectavx.cpp
    echo 'return (int)nXCRLow + _mm256_movemask_ps(ymm_small); }' >> detectavx.cpp
    echo 'int main(int argc, char**) { if( argc == 0 ) return foo(); return 0; }' >> detectavx.cpp
    echo '#else' >> detectavx.cpp
    echo 'some_error' >> detectavx.cpp
    echo '#endif' >> detectavx.cpp
    if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -o detectavx detectavx.cpp 2>&1`" ; then
        AC_MSG_RESULT([yes])
        AVXFLAGS=""
        HAVE_AVX_AT_COMPILE_TIME=yes
    else
        if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -mavx -o detectavx detectavx.cpp 2>&1`" ; then
            AC_MSG_RESULT([yes])
            AVXFLAGS="-mavx"
            HAVE_AVX_AT_COMPILE_TIME=yes
        else
            AC_MSG_RESULT([no])
            if test "$with_avx" = "yes"; then
                AC_MSG_ERROR([--with-avx was requested, but AVX is not available])
            fi
        fi
    fi

    dnl On Solaris, the presence of AVX instructions is flagged in the binary
    dnl and prevent it to run on non AVX hardware even if the instructions are
    dnl not executed. So if the user did not explicitly requires AVX, test that
    dnl we can run AVX binaries
    if test "$HAVE_AVX_AT_COMPILE_TIME" = "yes"; then
       case $host_os in
         solaris*)
           AC_MSG_CHECKING([whether AVX is available and needed at runtime])
           if ./detectavx; then
             AC_MSG_RESULT([yes])
           else
             AC_MSG_RESULT([no])
             if test "$with_avx" = "yes"; then
               echo "Caution: the generated binaries will not run on this system."
             else
               echo "Disabling AVX as it is not explicitly required"
               AVXFLAGS=""
               HAVE_AVX_AT_COMPILE_TIME=""
             fi
           fi
           ;;
       esac
    fi

    if test "$HAVE_AVX_AT_COMPILE_TIME" = "yes"; then
        CFLAGS="-DHAVE_AVX_AT_COMPILE_TIME $CFLAGS"
        CXXFLAGS="-DHAVE_AVX_AT_COMPILE_TIME $CXXFLAGS"
    fi

    rm -rf detectavx*
else
    AC_MSG_RESULT([no])
fi

AC_SUBST(AVXFLAGS,$AVXFLAGS)

dnl ---------------------------------------------------------------------------
dnl Check for --enable-lto
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([to enable LTO (link time optimization) build])

AC_ARG_ENABLE(lto,
              AS_HELP_STRING([--enable-lto],
                             [enable LTO(link time optimization) (disabled by default)]))

CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT="$CXXFLAGS"
CXXFLAGS_NO_LTO_IF_SSSE3_NONDEFAULT="$CXXFLAGS"

if test "x$enable_lto" = "xyes" ; then

  dnl remove existing -g.

  CXXFLAGS=`echo "$CXXFLAGS" | sed "s/-g //"`
  CFLAGS=`echo "$CFLAGS" | sed "s/-g //"`

  dnl add -flto

  CXXFLAGS="$CXXFLAGS -flto"
  CFLAGS="$CFLAGS -flto"
  LDFLAGS="$LDFLAGS -flto"

  dnl in case we have avx available by default, then we can compile everything
  dnl with -flto
  if test "$HAVE_AVX_AT_COMPILE_TIME" = "yes"; then
    if test "$AVXFLAGS" = ""; then
        CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT="$CXXFLAGS"
    fi
  fi
  if test "$HAVE_SSSE3_AT_COMPILE_TIME" = "yes"; then
    if test "$SSSE3FLAGS" = ""; then
        CXXFLAGS_NO_LTO_IF_SSSE3_NONDEFAULT="$CXXFLAGS"
    fi
  fi

  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

AC_SUBST(CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT,$CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT)
AC_SUBST(CXXFLAGS_NO_LTO_IF_SSSE3_NONDEFAULT,$CXXFLAGS_NO_LTO_IF_SSSE3_NONDEFAULT)

dnl ---------------------------------------------------------------------------
dnl Check if we need -lws2_32 (mingw)
dnl ---------------------------------------------------------------------------

echo "#include <winsock2.h>" > test_ws2_32.c
echo "#include <ws2tcpip.h>" >> test_ws2_32.c
echo "void foo() {}" >> test_ws2_32.c

if test -z "`${CC} -c test_ws2_32.c 2>&1`" ; then
  LIBS="$LIBS -lws2_32"
fi
rm -f test_ws2_32*

dnl ---------------------------------------------------------------------------
dnl Check if we need -lpsapi (mingw)
dnl ---------------------------------------------------------------------------

echo "#include <windows.h>" > test_psapi.c
echo "#include <psapi.h>" >> test_psapi.c
echo "void foo() {}" >> test_psapi.c

if test -z "`${CC} -c test_psapi.c 2>&1`" ; then
  LIBS="$LIBS -lpsapi"
fi
rm -f test_psapi*


dnl ---------------------------------------------------------------------------
dnl Check if we have getaddrinfo
dnl ---------------------------------------------------------------------------

echo "#include <sys/types.h>" > test_getaddrinfo.c
echo "#include <sys/socket.h>" >> test_getaddrinfo.c
echo "#include <netdb.h>" >> test_getaddrinfo.c
echo "void test() { getaddrinfo(0,0,0,0); }"  >> test_getaddrinfo.c

HAVE_GETADDRINFO=NO
if test -z "`${CC} -c test_getaddrinfo.c 2>&1`" ; then
  AC_DEFINE_UNQUOTED(HAVE_GETADDRINFO,1, [Define to 1 if you have the `getaddrinfo' function.])
fi
rm -f test_getaddrinfo*

dnl ---------------------------------------------------------------------------
dnl Check if user requests hiding internal symbols
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(hide_internal_symbols,[  --with-hide-internal-symbols[=ARG] Try to hide internal symbols (ARG=yes/no)],,)

HAVE_HIDE_INTERNAL_SYMBOLS=no

if test "$with_hide_internal_symbols" = "yes"; then

    AC_MSG_CHECKING([whether we should hide internal symbols])

    rm -f hiddensymbols.c
    echo 'int visible() { return 0; } __attribute__ ((visibility("default")))' >> hiddensymbols.c
    echo 'int hidden() { return 0; }' >> hiddensymbols.c
    if test -z "`${CC} -fvisibility=hidden -c hiddensymbols.c 2>&1`" ; then
      HAVE_HIDE_INTERNAL_SYMBOLS=yes
    else
      HAVE_HIDE_INTERNAL_SYMBOLS=no
    fi
    if test "$HAVE_HIDE_INTERNAL_SYMBOLS" = "yes"; then
        AC_MSG_RESULT([yes])
        CFLAGS="$CFLAGS -fvisibility=hidden"
        CXXFLAGS="$CXXFLAGS -fvisibility=hidden"
        CFLAGS_NOFTRAPV="$CFLAGS_NOFTRAPV -fvisibility=hidden"
        CXXFLAGS_NOFTRAPV="$CXXFLAGS_NOFTRAPV -fvisibility=hidden"
        CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT="$CXXFLAGS_NO_LTO_IF_AVX_NONDEFAULT -fvisibility=hidden"
        CXXFLAGS_NO_LTO_IF_SSSE3_NONDEFAULT="$CXXFLAGS_NO_LTO_IF_SSSE3_NONDEFAULT -fvisibility=hidden"
    else
        AC_MSG_RESULT([no])
    fi
    rm -f hiddensymbols.*
else
  AC_MSG_CHECKING([whether we should hide internal symbols])
  AC_MSG_RESULT([no])
fi

if test "$HAVE_HIDE_INTERNAL_SYMBOLS" = "yes" ; then
  AC_DEFINE_UNQUOTED(USE_GCC_VISIBILITY_FLAG,1, [Define to 1 if you want to use the -fvisibility GCC flag])
fi

AC_SUBST(HAVE_HIDE_INTERNAL_SYMBOLS,$HAVE_HIDE_INTERNAL_SYMBOLS)

dnl ---------------------------------------------------------------------------
dnl Check if user requests renaming internal libtiff symbols
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(rename_internal_libtiff_symbols,[  --with-rename-internal-libtiff-symbols[=ARG] Prefix internal libtiff symbols with gdal_ (ARG=yes/no)],,)

RENAME_INTERNAL_LIBTIFF_SYMBOLS=no
if test "x$with_rename_internal_libtiff_symbols" = "xyes" -o "x$with_rename_internal_libtiff_symbols" = "x"; then
  RENAME_INTERNAL_LIBTIFF_SYMBOLS=yes
fi

AC_SUBST(RENAME_INTERNAL_LIBTIFF_SYMBOLS,$RENAME_INTERNAL_LIBTIFF_SYMBOLS)

dnl ---------------------------------------------------------------------------
dnl Check if user requests renaming internal libgeotiff symbols
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(rename_internal_libgeotiff_symbols,[  --with-rename-internal-libgeotiff-symbols[=ARG] Prefix internal libgeotiff symbols with gdal_ (ARG=yes/no)],,)

RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS=no
if test "x$with_rename_internal_libgeotiff_symbols" = "xyes" -o "x$with_rename_internal_libgeotiff_symbols" = "x"; then
   RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS=yes
fi

AC_SUBST(RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS,$RENAME_INTERNAL_LIBGEOTIFF_SYMBOLS)

dnl ---------------------------------------------------------------------------
dnl Check if user requests renaming internal shapelib symbols
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(rename_internal_shapelib_symbols,[  --with-rename-internal-shapelib-symbols[=ARG] Prefix internal shapelib symbols with gdal_ (ARG=yes/no)],,)

RENAME_INTERNAL_SHAPELIB_SYMBOLS=no
if test "x$with_rename_internal_shapelib_symbols" = "xyes" -o "x$with_rename_internal_shapelib_symbols" = "x"; then
    RENAME_INTERNAL_SHAPELIB_SYMBOLS=yes
fi
AC_SUBST(RENAME_INTERNAL_SHAPELIB_SYMBOLS,$RENAME_INTERNAL_SHAPELIB_SYMBOLS)


dnl ---------------------------------------------------------------------------
dnl Check if user requests /usr/local to be included.
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([for local include/lib path])

AC_ARG_WITH(local,[  --with-local[=dir]      Include /usr/local or other local tree for INCLUDE/LIBS],,)

if test "$with_local" != "no" -a "$with_local" != "" ; then

  if test "$with_local" = "yes" ; then
    ADD_PREFIX=/usr/local
  else
    ADD_PREFIX=$with_local
  fi

  AC_MSG_RESULT(adding $ADD_PREFIX)

  LIBS="$LIBS -L$ADD_PREFIX/lib"
  CPPFLAGS="$CPPFLAGS -I$ADD_PREFIX/include"
else
  AC_MSG_RESULT([none])
fi

dnl ---------------------------------------------------------------------
dnl ENABLE THREADS ONLY IF REQUESTED.
dnl ---------------------------------------------------------------------

THREAD_LIB=""
THREAD_FLAG=""
PTHREAD_ENABLED="no"

AC_ARG_WITH(threads,
[  --with-threads[=ARG]    Include thread safe support (ARG=yes(default), no or linkopt)],,)

if test "$with_threads" = "yes" -o "$with_threads" = "" ; then

  AC_CHECK_LIB(pthread,pthread_create,THREAD_FLAG=CPL_MULTIPROC_PTHREAD,,,)

  if test -n "$THREAD_FLAG" ; then
      THREAD_LIB="-lpthread"
  else
    # For Android, pthread_create is in Bionic libc
    unset ac_cv_lib_pthread_pthread_create
    AC_CHECK_LIB(c,pthread_create,THREAD_FLAG=CPL_MULTIPROC_PTHREAD,,,)
  fi

  AC_MSG_CHECKING([for PTHREAD_MUTEX_RECURSIVE])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <pthread.h>
  ]], [[
    return (PTHREAD_MUTEX_RECURSIVE);
  ]])],[
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_PTHREAD_MUTEX_RECURSIVE, 1,
          [Define to 1 if you have the `PTHREAD_MUTEX_RECURSIVE' constant.])
  ],[
    AC_MSG_RESULT([no])
  ])



  AC_MSG_CHECKING([for PTHREAD_MUTEX_ADAPTIVE_NP])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <pthread.h>
  ]], [[
    return PTHREAD_MUTEX_ADAPTIVE_NP;
  ]])],[
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_PTHREAD_MUTEX_ADAPTIVE_NP, 1,
          [Define to 1 if you have the `PTHREAD_MUTEX_ADAPTIVE_NP' constant.])
  ],[
    AC_MSG_RESULT([no])
  ])

  AC_MSG_CHECKING([for pthread_spinlock_t])

  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <pthread.h>
  ]], [[
    pthread_spinlock_t spin;
    return 1;
  ]])],[
    AC_MSG_RESULT([yes])
    AC_DEFINE_UNQUOTED(HAVE_PTHREAD_SPINLOCK, 1,
          [Define to 1 if you have the `pthread_spinlock_t' type.])
  ],[
    AC_MSG_RESULT([no])
  ])

elif test "$with_threads" != "no" ; then

  THREAD_FLAG=CPL_MULTIPROC_PTHREAD
  THREAD_LIB="$with_threads"

  AS_MESSAGE([checking whether we should include thread/mutex support......])
  AC_MSG_RESULT([        using threads with link options "$THREAD_LIB".])

else
  AS_MESSAGE([checking whether we should include thread/mutex support......])
  AC_MSG_RESULT([        thread safe support disabled.])
fi

if test "$THREAD_FLAG" = "CPL_MULTIPROC_PTHREAD" ; then
  AC_DEFINE_UNQUOTED(CPL_MULTIPROC_PTHREAD,1, [Define if you want to use pthreads based multiprocessing support])
  PTHREAD_ENABLED="yes"
fi

LIBS="$THREAD_LIB $LIBS"

dnl ---------------------------------------------------------------------------
dnl Check if mremap() with 5 args is available
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([for 5 args mremap()])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#define _GNU_SOURCE
#include <sys/mman.h>
]], [[
return (mremap(0,0,0,0,0));
]])],[
AC_MSG_RESULT([yes])
AC_DEFINE_UNQUOTED(HAVE_5ARGS_MREMAP, 1,
        [Define to 1 if you have the 5 args `mremap' function.])
],[
AC_MSG_RESULT([no])
])

dnl ---------------------------------------------------------------------------
dnl Check if _SC_PHYS_PAGES is available
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([for _SC_PHYS_PAGES])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
]], [[
return (sysconf(_SC_PHYS_PAGES));
]])],[
AC_MSG_RESULT([yes])
AC_DEFINE_UNQUOTED(HAVE_SC_PHYS_PAGES, 1,
        [Define to 1 if you have the _SC_PHYS_PAGES' constant.])
],[
AC_MSG_RESULT([no])
])

dnl '---------------------------------------------------------------------------
dnl Remove possible system library paths from a *_LIB variable to prevent other, perhaps unrelated, system libraries
dnl from shadowing custom libraries of the same name the user might have installed e.g. under /usr/local.
dnl System library paths are things like /lib, /usr/lib, /lib64, /usr/lib64, and /usr/lib/x86_64-linux-gnu.
dnl There may be others as well, that will need be addressed as they arise. We avoid wildcards such as
dnl /usr/lib/* to avoid potential cases such as /usr/lib/mysql/lib, which might not have an /etc/ld.so.conf.d
dnl entry and would not shadow any possible custom installs anyway:

AC_DEFUN([STRIP_SYSTEM_LIBRARY_PATHS],[
    if echo "x" | sed -E "s/x/y/" >/dev/null 2>/dev/null; then
        STRIPPED_LIBRARY_NAME="$(echo [$1] | sed -E 's:-L/(usr/)?(lib(64)?|lib/x86_64-linux-gnu)/*[[[:space:]]]+::')"
    elif echo "x" | sed -r "s/x/y/" >/dev/null 2>/dev/null; then
        dnl Compatibility with older GNU sed that doesn't support -E
        STRIPPED_LIBRARY_NAME="$(echo [$1] | sed -r 's:-L/(usr/)?(lib(64)?|lib/x86_64-linux-gnu)/*[[[:space:]]]+::')"
    else
        STRIPPED_LIBRARY_NAME="$(echo [$1])"
    fi
])

dnl ---------------------------------------------------------------------------
dnl Check if libz is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(libz,[  --with-libz[=ARG]       Include libz support (ARG=internal or libz directory)],,)

if test "$with_libz" = "external" -o "$with_libz" = "" -o "$with_libz" = "yes" ; then

  AC_CHECK_LIB(z,deflateInit_,LIBZ_SETTING=external,LIBZ_SETTING=internal,)

  if test "$LIBZ_SETTING" = "external" ; then
    AC_CHECK_LIB(z,inflateCopy,LIBZ_SETTING=external,LIBZ_SETTING=internal,)
    if test "$LIBZ_SETTING" = "external" ; then
        AC_MSG_RESULT([using pre-installed libz])
    else
        AC_MSG_RESULT([using internal libz code as inflateCopy is missing])
    fi
  else
    AC_MSG_RESULT([using internal libz code as deflateInit_ is missing])
  fi

elif test "$with_libz" != "no" -a "$with_libz" != "internal" ; then

  LIBS="-L$with_libz -L$with_libz/lib $LIBS"

  AC_CHECK_LIB(z,deflateInit_,LIBZ_SETTING=external,LIBZ_SETTING=internal,-L$with_libz -L$with_libz/lib -lz)
  if test "$LIBZ_SETTING" = "external" ; then
    AC_CHECK_LIB(z,inflateCopy,LIBZ_SETTING=external,LIBZ_SETTING=internal,-L$with_libz -L$with_libz/lib -lz)
     if test "$LIBZ_SETTING" = "external" ; then
        AC_MSG_RESULT([using libz library from $with_libz])
    else
        AC_MSG_RESULT([using internal libz code as inflateCopy from $with_libz is missing])
    fi
  else
    AC_MSG_RESULT([using internal libz code as deflateInit_ from $with_libz is missing])
  fi

fi

if test "$LIBZ_SETTING" = "external" ; then
  LIBS="-lz $LIBS"
  if test "$with_libz" != "" -a "$with_libz" != "yes" -a "$with_libz" != "external" ; then
    EXTRA_INCLUDES="-I$with_libz -I$with_libz/include $EXTRA_INCLUDES"
  fi
elif test "$with_libz" = "no" ; then
  LIBZ_SETTING=no
  AC_MSG_RESULT([not using libz at all.])
else
  LIBZ_SETTING=internal
  OPT_GDAL_FORMATS="zlib $OPT_GDAL_FORMATS"
  AC_MSG_RESULT([using internal libz code.])
fi

AC_SUBST(LIBZ_SETTING,$LIBZ_SETTING)

dnl ---------------------------------------------------------------------------
dnl Check for libdeflate.
dnl ---------------------------------------------------------------------------

LIBDEFLATE_SETTING=no

AC_ARG_WITH(libdeflate,
            AS_HELP_STRING([--with-libdeflate],
                            [enable libdeflate usage (optional for faster Deflate support (still requires zlib) (ARG=yes/no/prefix), enabled by default)]),,)

if test "$with_libdeflate" != "no" ; then

  if test "$with_libdeflate" != "" -a "$with_libdeflate" != "yes"; then
    AC_CHECK_LIB(deflate, libdeflate_zlib_decompress, [libdeflate_lib=yes], [libdeflate_lib=no],-L$with_libdeflate/lib)
  else
    AC_CHECK_LIB(deflate, libdeflate_zlib_decompress, [libdeflate_lib=yes], [libdeflate_lib=no])
  fi

  if test "$libdeflate_lib" = "no" -a "$with_libdeflate" = "yes"; then
    AC_MSG_ERROR([libdeflate library not found])
  fi

  if test "$with_libdeflate" != "" -a "$with_libdeflate" != "yes"; then
    if test -r "$with_libdeflate/include/libdeflate.h" ; then
      libdeflate_h=yes
    else
      libdeflate_h=no
    fi
  else
    AC_CHECK_HEADER(libdeflate.h, [libdeflate_h=yes], [libdeflate_h=no])
  fi
  if test "$libdeflate_h" = "no" -a "$with_libdeflate" = "yes"; then
    AC_MSG_ERROR([libdeflate headers not found])
  fi

  if test "$libdeflate_lib" = "yes" -a "$libdeflate_h" = "yes" ; then
    LIBDEFLATE_SETTING=yes
  fi

fi

if test "$LIBDEFLATE_SETTING" = "yes" ; then
  if test "$with_libdeflate" != "" -a "$with_libdeflate" != "yes"; then
    EXTRA_INCLUDES="-I$with_libdeflate/include $EXTRA_INCLUDES"
    LIBS="-L$with_libdeflate/lib -ldeflate $LIBS"
  else
    LIBS="-ldeflate $LIBS"
  fi
fi

AC_SUBST(LIBDEFLATE_SETTING,$LIBDEFLATE_SETTING)

dnl ---------------------------------------------------------------------------
dnl Check for iconv/libiconv.
dnl A rationale for this macro is described in "GNU `gettext' utilities"
dnl manual at
dnl http://www.gnu.org/software/gettext/manual/html_node/AM_005fICONV.html
dnl ---------------------------------------------------------------------------
AM_ICONV

dnl Extra test needed for GCC 4.5 on Solaris 11, where there is
dnl a different behavior if the tests are compiled with gcc or g++.
dnl So we introduce a ICONV_CPP_CONST that must be used instead of
dnl ICONV_CONST when used from .cpp files.
if test "$am_func_iconv" = "yes"; then
    rm -f testiconv.*
    echo '#include <iconv.h>' > testiconv.cpp
    echo 'int main(int argc, char** argv) { iconv_t cd = 0; return iconv (cd, (const char **) 0, 0, 0, 0); } ' >> testiconv.cpp
    if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} testiconv.cpp -c 2>&1`" ; then
        AC_MSG_RESULT([using ICONV_CPP_CONST="const"])
        ICONV_CPP_CONST="const"
    else
        AC_MSG_RESULT([using ICONV_CPP_CONST=""])
        ICONV_CPP_CONST=""
    fi
    rm -f testiconv.*

    AC_DEFINE_UNQUOTED(ICONV_CPP_CONST,$ICONV_CPP_CONST, [For.cpp files, define as const if the declaration of iconv() needs const.])
fi

dnl ---------------------------------------------------------------------------
dnl Bash completions
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([bash-completion],
  AS_HELP_STRING([--with-bash-completion[=ARG]],
    [Install Bash completions (ARG=yes or path)]),,)

if test "x$with_bash_completion" = "xno" -o "x$with_bash_completion" = "x"; then
  AC_MSG_NOTICE([Bash completions not requested])
elif test "x$with_bash_completion" = "xyes"; then
  PKG_CHECK_VAR(bashcompdir, [bash-completion], [completionsdir], ,
    bashcompdir="${sysconfdir}/bash_completion.d")
  AC_SUBST(bashcompdir)
else
  AC_SUBST(bashcompdir, $with_bash_completion)
fi

dnl ---------------------------------------------------------------------------
dnl Select a JPEG Library to use, or disable driver.
dnl libtiff could depend on it so it must appear before.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(jpeg,[  --with-jpeg[=ARG]       Include JPEG support (ARG=internal, no or path)],,)

if test "$with_jpeg" = "no" ; then

  JPEG_SETTING=no

  echo "jpeg support disabled."

elif test "$with_jpeg" = "yes" -o "$with_jpeg" = "" ; then

  AC_CHECK_LIB(jpeg,jpeg_read_scanlines,JPEG_SETTING=external,JPEG_SETTING=internal,)
  AC_CHECK_HEADERS(jpeglib.h)

  if test "$JPEG_SETTING" = "external" -a "$ac_cv_header_jpeglib_h" = "no" ; then
    JPEG_SETTING=internal
  fi

  if test "$JPEG_SETTING" = "external" -a "$TIFF_SETTING" = "internal" ; then
    AC_MSG_CHECKING([for width_in_blocks in jpeglib.h])

    rm -f conftest.c
    echo '#include <stdio.h>' >> conftest.c
    echo '#include "jpeglib.h"' >> conftest.c
    echo 'int main() { jpeg_component_info *comptr=0; int i; i = comptr->width_in_blocks; }' >> conftest.c
    if test -z "`${CC} ${CPPFLAGS} ${CFLAGS} -c conftest.c 2>&1`" ; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
      JPEG_SETTING=internal
    fi
    rm -f conftest*
  fi

  if test "$JPEG_SETTING" = "external" ; then
    LIBS="-ljpeg $LIBS"
    echo "using pre-installed libjpeg."
  else
    echo "using internal jpeg code."
  fi

elif test "$with_jpeg" = "internal" ; then

  JPEG_SETTING=internal

  echo "using internal jpeg code."

else

  JPEG_SETTING=external
  LIBS="-L$with_jpeg -L$with_jpeg/lib -ljpeg $LIBS"
  EXTRA_INCLUDES="-I$with_jpeg -I$with_jpeg/include $EXTRA_INCLUDES"

  echo "using libjpeg from $with_jpeg."

fi

AC_SUBST(JPEG_SETTING,$JPEG_SETTING)

if test "$JPEG_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="jpeg $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Check if user requests renaming internal libjpeg symbols
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(rename_internal_libjpeg_symbols,[  --with-rename-internal-libjpeg-symbols[=ARG] Prefix internal libjpeg symbols with gdal_ (ARG=yes/no)],,)

RENAME_INTERNAL_LIBJPEG_SYMBOLS=no
if test "x$with_rename_internal_libjpeg_symbols" = "xyes" -o "x$with_rename_internal_libjpeg_symbols" = "x"; then
    RENAME_INTERNAL_LIBJPEG_SYMBOLS=yes
fi

AC_SUBST(RENAME_INTERNAL_LIBJPEG_SYMBOLS,$RENAME_INTERNAL_LIBJPEG_SYMBOLS)

dnl ---------------------------------------------------------------------------
dnl Select a libtiff library to use.
dnl Proj depends on it so it must appear before.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(libtiff,[  --with-libtiff=ARG    Libtiff library to use (ARG=internal, yes or path)],,)

AC_MSG_CHECKING([for libtiff])

if test "x${with_libtiff}" = "xyes" -o "x${with_libtiff}" = "x" ; then

  dnl Only automatically pick up the external libtiff if it is >= 4.0.
  AC_CHECK_LIB(tiff,TIFFScanlineSize64,TIFF_SETTING=external HAVE_BIGTIFF=yes,TIFF_SETTING=internal HAVE_BIGTIFF=yes,)

  if test "$TIFF_SETTING" = "external" ; then
    LIBS="-ltiff $LIBS"
    AC_MSG_RESULT([using pre-installed libtiff.])
  else
    AC_MSG_RESULT([using internal TIFF code.])
  fi

elif test "x${with_libtiff}" = "xno" ; then

  AC_MSG_ERROR([libtiff is a required dependency])

elif test "x${with_libtiff}" = "xinternal" ; then

  TIFF_SETTING=internal
  HAVE_BIGTIFF=yes

  AC_MSG_RESULT([using internal TIFF code.])

else

  TIFF_SETTING=external
  if test -r "$with_libtiff/tiff.h" ; then
    LIBS="-L$with_libtiff -ltiff $LIBS"
    EXTRA_INCLUDES="-I$with_libtiff $EXTRA_INCLUDES"
  else
    LIBS="-L$with_libtiff/lib -ltiff $LIBS"
    EXTRA_INCLUDES="-I$with_libtiff/include $EXTRA_INCLUDES"
  fi

  AC_MSG_RESULT([using libtiff from ${with_libtiff}.])

  dnl Check for the BigTIFF enabled library (libtiff >= 4.0)
  AC_CHECK_LIB(tiff,TIFFScanlineSize64,HAVE_BIGTIFF=yes,HAVE_BIGTIFF=no,)

fi

if test "${HAVE_BIGTIFF}" != "yes" ; then
  AC_MSG_ERROR([libtiff >= 4.0 is required.])
fi

AC_SUBST(TIFF_SETTING,${TIFF_SETTING})

dnl ---------------------------------------------------------------------------
dnl Check for curl (i.e. for wcs).
dnl Proj depends on it so it must appear before.
dnl ---------------------------------------------------------------------------
CURL_SETTING=no
CURL_INC=
CURL_LIB=

AC_ARG_WITH(curl,
    [  --with-curl[=ARG]       Include curl (ARG=path to curl-config.)],,,)

dnl Clear some cache variables
unset ac_cv_path_LIBCURL

if test "`basename xx/$with_curl`" = "curl-config" ; then
  LIBCURL_CONFIG="$with_curl"
elif test "$with_curl" = "no" ; then
  LIBCURL_CONFIG=no
else
  AC_PATH_PROG(LIBCURL_CONFIG, curl-config, no)
fi

if test "$LIBCURL_CONFIG" != "no" ; then

  CURL_VERNUM=`$LIBCURL_CONFIG --vernum`
  CURL_VER=`$LIBCURL_CONFIG --version | awk '{print $2}'`

  AC_MSG_RESULT([        found libcurl version $CURL_VER])

  AC_CHECK_LIB(curl,curl_global_init,CURL_SETTING=yes,CURL_SETTING=no,`$LIBCURL_CONFIG --libs`)

fi

AC_SUBST(CURL_SETTING,$CURL_SETTING)
AC_SUBST(CURL_INC,    $CURL_INC)
AC_SUBST(CURL_LIB,    $CURL_LIB)

dnl ---------------------------------------------------------------------------
dnl Check for SQLite
dnl Proj depends on it so it must appear before.
dnl ---------------------------------------------------------------------------

SQLITE3_REQ_VERSION="3.0.0"
AX_LIB_SQLITE3($SQLITE3_REQ_VERSION)

if test "$HAVE_SQLITE3" = "yes"; then
    LIBS="$SQLITE3_LDFLAGS $LIBS"
    AC_CHECK_LIB(sqlite3,sqlite3_column_table_name,SQLITE_HAS_COLUMN_METADATA=yes,SQLITE_HAS_COLUMN_METADATA=no,$LIBS)
fi

AC_SUBST([SQLITE_INC], $SQLITE3_CFLAGS)
AC_SUBST([HAVE_SQLITE], $HAVE_SQLITE3)
AC_SUBST([SQLITE_HAS_COLUMN_METADATA], $SQLITE_HAS_COLUMN_METADATA)
HAVE_SQLITE=$HAVE_SQLITE3

dnl ---------------------------------------------------------------------------
dnl PROJ.6 related stuff
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([for PROJ >= 6 library])

AC_ARG_WITH(proj,[  --with-proj=ARG Compile with PROJ.x (ARG=yes or path)],,)

AC_ARG_WITH(proj-extra-lib-for-test,
    [  --with-proj-extra-lib-for-test=ARG   Additional libraries to pass the detection test, but not used for libgdal linking (i.e. -lcurl -ltiff ...). Mainly for static libproj],,,)

PROJ_INCLUDE=""
if test "x$with_proj" = "xno" ; then

  AC_MSG_ERROR([PROJ >= 6 is a required dependency.])

else

  if test "x$with_proj" = "xyes" -o "x$with_proj" = "x"; then
    ORIG_LIBS="$LIBS"
    LIBS="-lproj $with_proj_extra_lib_for_test $ORIG_LIBS"
    AC_LANG_PUSH([C++])
    AC_CHECK_LIB(proj,proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
    AC_LANG_POP([C++])
    if test "$PROJ_FOUND" = "no"; then
        AC_LANG_PUSH([C++])
        AC_CHECK_LIB(proj,internal_proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
        AC_LANG_POP([C++])
        if test "$PROJ_FOUND" = "yes"; then
            PROJ_INCLUDE="-DPROJ_RENAME_SYMBOLS"
        fi
    fi
    if test "$PROJ_FOUND" = "no"; then
        LIBS="-linternalproj $ORIG_LIBS"
        AC_LANG_PUSH([C++])
        AC_CHECK_LIB(internalproj,internal_proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
        AC_LANG_POP([C++])
        if test "$PROJ_FOUND" = "yes"; then
            PROJ_INCLUDE="-DPROJ_RENAME_SYMBOLS"
        fi
    fi
    if test "$PROJ_FOUND" = "no"; then
        AC_MSG_ERROR([PROJ 6 symbols not found])
    fi
    AC_CHECK_HEADERS([proj.h])
    if test "$ac_cv_header_proj_h" = "yes" ; then
        AC_MSG_NOTICE([proj.h found])
    else
        AC_MSG_ERROR([proj.h not found])
    fi
  else
    ORIG_LIBS="$LIBS"
    LIBS="-L$with_proj/lib -lproj $with_proj_extra_lib_for_test $ORIG_LIBS"
    AC_LANG_PUSH([C++])
    AC_CHECK_LIB(proj,proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
    AC_LANG_POP([C++])
    if test "$PROJ_FOUND" = "no"; then
        LIBS="-L$with_proj/lib -lproj $with_proj_extra_lib_for_test $ORIG_LIBS"
        unset ac_cv_lib_proj_proj_create_from_wkt
        AC_LANG_PUSH([C++])
        AC_CHECK_LIB(proj,proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
        AC_LANG_POP([C++])
    fi
    if test "$PROJ_FOUND" = "no"; then
        LIBS="-L$with_proj/lib64 -lproj $with_proj_extra_lib_for_test $ORIG_LIBS"
        unset ac_cv_lib_proj_proj_create_from_wkt
        AC_LANG_PUSH([C++])
        AC_CHECK_LIB(proj,proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
        AC_LANG_POP([C++])
    fi
    if test "$PROJ_FOUND" = "no"; then
        LIBS="-L$with_proj/lib -lproj $with_proj_extra_lib_for_test $ORIG_LIBS"
        AC_LANG_PUSH([C++])
        AC_CHECK_LIB(proj,internal_proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
        AC_LANG_POP([C++])
        if test "$PROJ_FOUND" = "no"; then
            LIBS="-L$with_proj/lib -lproj $with_proj_extra_lib_for_test $ORIG_LIBS"
            unset ac_cv_lib_proj_internal_proj_create_from_wkt
            AC_LANG_PUSH([C++])
            AC_CHECK_LIB(proj,internal_proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
            AC_LANG_POP([C++])
        fi
        if test "$PROJ_FOUND" = "yes"; then
            PROJ_INCLUDE="-DPROJ_RENAME_SYMBOLS"
        fi
    fi
    if test "$PROJ_FOUND" = "no"; then
        LIBS="-L$with_proj/lib -linternalproj $with_proj_extra_lib_for_test $ORIG_LIBS"
        AC_LANG_PUSH([C++])
        AC_CHECK_LIB(internalproj,internal_proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
        AC_LANG_POP([C++])
        if test "$PROJ_FOUND" = "no"; then
            LIBS="-L$with_proj/lib -linternalproj $with_proj_extra_lib_for_test $ORIG_LIBS"
            unset ac_cv_lib_internal_proj_internal_proj_create_from_wkt
            AC_LANG_PUSH([C++])
            AC_CHECK_LIB(internalproj,internal_proj_create_from_wkt,PROJ_FOUND=yes,PROJ_FOUND=no,)
            AC_LANG_POP([C++])
        fi
        if test "$PROJ_FOUND" = "yes"; then
            PROJ_INCLUDE="-DPROJ_RENAME_SYMBOLS"
        fi
    fi
    if test "$PROJ_FOUND" = "no"; then
        AC_MSG_ERROR([PROJ 6 symbols not found])
    fi
    if test -r "$with_proj/include/proj.h" ; then
        PROJ_INCLUDE="$PROJ_INCLUDE -I$with_proj/include"
        AC_MSG_NOTICE([proj.h found])
    else
        AC_MSG_ERROR([proj.h not found])
    fi
  fi
fi

AC_SUBST(PROJ_INCLUDE)

dnl ---------------------------------------------------------------------------
dnl Check for SpatiaLite.
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING(for spatialite)

AC_ARG_WITH(spatialite,
    [  --with-spatialite=ARG Include SpatiaLite support (ARG=no(default), yes, dlopen (only supported for Spatialite >= 4.1.2) or path)],
    ,,)

AC_ARG_WITH(spatialite-soname,
    [  --with-spatialite-soname=ARG Spatialite shared object name (e.g. libspatialite.so), only used if --with-spatialite=dlopen],
    ,,)

HAVE_SPATIALITE=no

if test -z "$with_spatialite" -o "$with_spatialite" = "no"; then
    AC_MSG_RESULT(disabled)
elif test "$with_spatialite" = "yes"; then
    AC_CHECK_HEADERS(sqlite3.h)
    if test "$ac_cv_header_sqlite3_h" = "yes"; then
        AC_MSG_CHECKING([for spatialite.h in /usr/include or /usr/local/include])
        if test -f "/usr/include/spatialite.h" -o -f "/usr/local/include/spatialite.h"; then
            AC_MSG_RESULT(found)
            AC_CHECK_LIB(spatialite,spatialite_init,SPATIALITE_INIT_FOUND=yes,SPATIALITE_INIT_FOUND=no,-lsqlite3)
            if test "$SPATIALITE_INIT_FOUND" = "yes"; then
                HAVE_SPATIALITE=yes
                SPATIALITE_LIBS="-lspatialite -lsqlite3"
                LIBS="$LIBS $SPATIALITE_LIBS"
                HAVE_SQLITE3=yes
            fi
        else
            AC_MSG_RESULT(not found : spatialite support disabled)
        fi
    fi
elif test "$with_spatialite" = "dlopen"; then
  HAVE_SPATIALITE=dlopen
  AC_MSG_RESULT(dlopen)

  if test "$with_spatialite_soname" != ""; then
      SPATIALITE_SONAME="$with_spatialite_soname"
  else
      SPATIALITE_SONAME="spatialite.so"
  fi
else
    AC_CHECK_LIB(spatialite,spatialite_init,SPATIALITE_INIT_FOUND=yes,SPATIALITE_INIT_FOUND=no,-L$with_spatialite/lib -lspatialite)

    if test -f "$with_spatialite/include/spatialite.h" -a \
        "$SPATIALITE_INIT_FOUND" = "yes"; then

        if test "$HAVE_SQLITE3" = "yes"; then
            SPATIALITE_INC="-I$with_spatialite/include"
            HAVE_SPATIALITE=yes
            SPATIALITE_LIBS="-L$with_spatialite/lib -lspatialite"
            LIBS="$SQLITE3_LDFLAGS $LIBS $SPATIALITE_LIBS"
            AC_MSG_RESULT(spatialite enabled)
        else
            AC_MSG_RESULT(spatialite disabled)
        fi
    else
        AC_MSG_RESULT(disabled)
    fi
fi

if test "$HAVE_SPATIALITE" = "yes"; then
    AC_CHECK_LIB(spatialite,spatialite_target_cpu,SPATIALITE_412_OR_LATER=yes,SPATIALITE_412_OR_LATER=no)
fi

AC_SUBST([HAVE_SPATIALITE], $HAVE_SPATIALITE)
AC_SUBST([SPATIALITE_SONAME], $SPATIALITE_SONAME)
AC_SUBST([SPATIALITE_INC], $SPATIALITE_INC)
AC_SUBST([SPATIALITE_412_OR_LATER], $SPATIALITE_412_OR_LATER)

dnl ---------------------------------------------------------------------------
dnl Check if liblzma is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(liblzma,[  --with-liblzma[=ARG]       Include liblzma support (ARG=yes/no)],,)

if test "$with_liblzma" = "yes" ; then
  AC_CHECK_LIB(lzma,lzma_code,LIBLZMA_SETTING=yes,LIBLZMA_SETTING=no,)
  AC_CHECK_HEADERS(lzma.h)

  if test "$LIBLZMA_SETTING" = "yes" -a "$ac_cv_header_lzma_h" = "yes" ; then
    LIBS="-llzma $LIBS"
  else
    LIBLZMA_SETTING=no
  fi
else
    LIBLZMA_SETTING=no
fi

AC_SUBST(LIBLZMA_SETTING,$LIBLZMA_SETTING)


dnl ---------------------------------------------------------------------------
dnl Check if zstd is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(zstd,[  --with-zstd[=ARG]       Include zstd support (ARG=yes/no/installation_prefix)],,)

if test "$with_zstd" = "" -o "$with_zstd" = "yes" ; then
  AC_CHECK_LIB(zstd,ZSTD_decompressStream,ZSTD_SETTING=yes,ZSTD_SETTING=no,)

  if test "$ZSTD_SETTING" = "yes" ; then
    LIBS="-lzstd $LIBS"
  else
    if test "$with_zstd" = "yes" ; then
      AC_MSG_ERROR([libzstd not found])
    else
      echo "libzstd not found - ZSTD support disabled"
    fi
  fi
elif test "$with_zstd" != "" -a "$with_zstd" != "no"; then

  AC_CHECK_LIB(zstd,ZSTD_decompressStream,ZSTD_SETTING=yes,ZSTD_SETTING=no,-L$with_zstd/lib)

  if test "$ZSTD_SETTING" = "yes" -a -f "$with_zstd/include/zstd.h" ; then
    LIBS="-L$with_zstd/lib -lzstd $LIBS"
    EXTRA_INCLUDES="-I$with_zstd/include $EXTRA_INCLUDES"
  else
    AC_MSG_ERROR([libzstd not found])
  fi

else
    ZSTD_SETTING=no
fi

AC_SUBST(ZSTD_SETTING,$ZSTD_SETTING)


dnl ---------------------------------------------------------------------------
dnl Check if blosc is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(blosc,[  --with-blosc[=ARG]       Include blosc support (ARG=yes/no/installation_prefix)],,)

if test "$with_blosc" = "" -o "$with_blosc" = "yes" ; then
  AC_CHECK_LIB(blosc,blosc_cbuffer_validate,HAVE_BLOSC=yes,HAVE_BLOSC=no,)

  if test "$HAVE_BLOSC" = "yes" ; then
    LIBS="-lblosc $LIBS"
  else
    if test "$with_blosc" = "yes" ; then
      AC_MSG_ERROR([libblosc not found])
    else
      echo "libblosc not found - BLOSC support disabled"
    fi
  fi
elif test "$with_blosc" != "" -a "$with_blosc" != "no"; then

  AC_CHECK_LIB(blosc,blosc_cbuffer_validate,HAVE_BLOSC=yes,HAVE_BLOSC=no,-L$with_blosc/lib)

  if test "$HAVE_BLOSC" = "yes" -a -f "$with_blosc/include/blosc.h" ; then
    LIBS="-L$with_blosc/lib -lblosc $LIBS"
    EXTRA_INCLUDES="-I$with_blosc/include $EXTRA_INCLUDES"
  else
    AC_MSG_ERROR([libblosc not found])
  fi

else
    HAVE_BLOSC=no
fi

AC_SUBST(HAVE_BLOSC,$HAVE_BLOSC)

dnl ---------------------------------------------------------------------------
dnl Check if lz4 is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(lz4,[  --with-lz4[=ARG]       Include lz4 support (ARG=yes/no/installation_prefix)],,)

if test "$with_lz4" = "" -o "$with_lz4" = "yes" ; then
  AC_CHECK_LIB(lz4,LZ4_decompress_safe,HAVE_LZ4=yes,HAVE_LZ4=no,)

  if test "$HAVE_LZ4" = "yes" ; then
    LIBS="-llz4 $LIBS"
  else
    if test "$with_lz4" = "yes" ; then
      AC_MSG_ERROR([liblz4 not found])
    else
      echo "liblz4 not found - LZ4 support disabled"
    fi
  fi
elif test "$with_lz4" != "" -a "$with_lz4" != "no"; then

  AC_CHECK_LIB(lz4,LZ4_decompress_safe,HAVE_LZ4=yes,HAVE_LZ4=no,-L$with_lz4/lib)

  if test "$HAVE_LZ4" = "yes" -a -f "$with_lz4/include/lz4.h" ; then
    LIBS="-L$with_lz4/lib -llz4 $LIBS"
    EXTRA_INCLUDES="-I$with_lz4/include $EXTRA_INCLUDES"
  else
    AC_MSG_ERROR([liblz4 not found])
  fi

else
    HAVE_LZ4=no
fi

AC_SUBST(HAVE_LZ4,$HAVE_LZ4)

dnl ---------------------------------------------------------------------------
dnl Set up drivers and formats
dnl ---------------------------------------------------------------------------

GDALFORMATS_ENABLED=
GDALFORMATS_DISABLED=
OGRFORMATS_ENABLED=
OGRFORMATS_ENABLED_CFLAGS=
OGRFORMATS_DISABLED=

AC_DEFUN([INTERNAL_FORMATS],[aaigrid adrg aigrid airsar arg blx bmp bsb cals ceos ceos2 coasp cosar ctg dimap dted elas envisat ers esric fit gff gsg gxf hf2 idrisi ilwis ingr iris iso8211 jaxapalsar jdem kmlsuperoverlay l1b leveller map mrf msgn ngsgeoid nitf northwood pds prf r raw rmf rs2 safe saga sdts sentinel2 sgi sigdem srtmhgt stacit stacta terragen tga til tsx usgsdem xpm xyz zarr zmap])
AC_DEFUN([INTERNAL_OPT_FORMATS],[grib ozi pdf rik])
AC_DEFUN([INTERNAL_DRIVERS],[arcgen avc cad csv dgn dxf edigeo flatgeobuf geoconcept georss gml gmt gpsbabel gpx gtm jml mapml mvt ntf openfilegdb pgdump rec s57 selafin shape svg sxf tiger vdv wasp])dnl
AC_DEFUN([CURL_FORMATS],[eeda plmosaic wcs wms wmts daas ogcapi])dnl
AC_DEFUN([CURL_DRIVERS],[amigocloud carto cloudant couchdb csw elastic ngw plscenes wfs])dnl
AC_DEFUN([SQLITE_FORMATS],[rasterlite mbtiles])dnl
AC_DEFUN([SQLITE_DRIVERS],[gpkg vfk osm])dnl
AC_DEFUN([EXPAT_DRIVERS],[ods xlsx lvbag])dnl
AC_DEFUN([XERCES_DRIVERS],[nas ili gmlas])dnl
AC_DEFUN([ODBC_DRIVERS],[odbc pgeo mssqlspatial geomedia walk])dnl

AC_ARG_ENABLE(all-optional-drivers, AS_HELP_STRING([--disable-all-optional-drivers], [disable all non-mandatory internal drivers. Individual drivers can be added with --enable-driver-XXX]))
if test "x$enable_all_optional_drivers" = "xno"; then :
  all_drivers_disabled=yes
else
  all_drivers_disabled=no
fi

m4_define([ARG_DRIVER_ENABLED],[dnl
cur_driver_enabled=yes
requested=m4_join([_],[$enable_driver],$1)
if test "$all_drivers_disabled" = "yes"; then :
  if test "x$requested" = "xyes"; then :
    cur_driver_enabled=yes
  else
    cur_driver_enabled=no
  fi
else
  if test "x$requested" != "xno"; then :
    cur_driver_enabled=yes
  else
    cur_driver_enabled=no
  fi
fi
])



m4_foreach_w([frmt], INTERNAL_FORMATS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt driver support (enabled by default)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  GDALFORMATS_ENABLED="$GDALFORMATS_ENABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  GDALFORMATS_DISABLED="$GDALFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

m4_foreach_w([frmt],INTERNAL_OPT_FORMATS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt format support (enabled by default, requires dependency)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  GDALFORMATS_DISABLED="$GDALFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

m4_define([_OGRDEFINE],[m4_join([],[-D],m4_toupper([$1]),[_ENABLED])])

m4_foreach_w([frmt],INTERNAL_DRIVERS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt driver support (enabled by default)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  OGRFORMATS_ENABLED="$OGRFORMATS_ENABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
  OGRFORMATS_ENABLED_CFLAGS="$OGRFORMATS_ENABLED_CFLAGS _OGRDEFINE(frmt)"
else
  OGRFORMATS_DISABLED="$OGRFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

AC_DEFUN([INTERNAL_DRIVERS_WITH_SAME_NAME_AS_FORMAT],[idrisi pds sdts])dnl
m4_foreach_w([frmt],INTERNAL_DRIVERS_WITH_SAME_NAME_AS_FORMAT,
[
driver_enabled=m4_join([_],[$INTERNAL_FORMAT],frmt,[ENABLED])
if test "x$driver_enabled" != "xno"; then :
  OGRFORMATS_ENABLED="$OGRFORMATS_ENABLED frmt"
  OGRFORMATS_ENABLED_CFLAGS="$OGRFORMATS_ENABLED_CFLAGS _OGRDEFINE(frmt)"
else
  OGRFORMATS_DISABLED="$OGRFORMATS_DISABLED frmt"
fi
])

m4_foreach_w([frmt],CURL_FORMATS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt format support (enabled by default, requires curl)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  GDALFORMATS_DISABLED="$GDALFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

m4_foreach_w([frmt],CURL_DRIVERS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt driver support (enabled by default, requires curl)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  OGRFORMATS_DISABLED="$OGRFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

m4_foreach_w([frmt],SQLITE_FORMATS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt format support (enabled by default, requires sqlite3)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  GDALFORMATS_DISABLED="$GDALFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

m4_foreach_w([frmt],SQLITE_DRIVERS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt driver support (enabled by default, requires sqlite3)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  OGRFORMATS_DISABLED="$OGRFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

m4_foreach_w([frmt],EXPAT_DRIVERS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt driver support (enabled by default, requires expat)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  OGRFORMATS_DISABLED="$OGRFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

m4_foreach_w([frmt],XERCES_DRIVERS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt driver support (enabled by default, requires xerces-c)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  OGRFORMATS_DISABLED="$OGRFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

m4_foreach_w([frmt],ODBC_DRIVERS,
[
AC_ARG_ENABLE(driver-frmt, AS_HELP_STRING([--disable-driver-frmt], [disable frmt driver support (enabled by default, requires odbc)]))dnl
ARG_DRIVER_ENABLED(frmt)
if test "$cur_driver_enabled" = "yes"; then :
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=yes
else
  OGRFORMATS_DISABLED="$OGRFORMATS_DISABLED frmt"
  m4_join([_],[INTERNAL_FORMAT],frmt,[ENABLED])=no
fi
])

AC_SUBST(OGRFORMATS_ENABLED,$OGRFORMATS_ENABLED)
AC_SUBST(OGRFORMATS_ENABLED_CFLAGS,$OGRFORMATS_ENABLED_CFLAGS)
AC_SUBST(GDALFORMATS_ENABLED,$GDALFORMATS_ENABLED)

if test "$LIBZ_SETTING" != "no" ; then
  if test "x$INTERNAL_FORMAT_rik_ENABLED" = "xyes"; then
      GDALFORMATS_ENABLED="$GDALFORMATS_ENABLED rik"
  fi
  if test "x$INTERNAL_FORMAT_ozi_ENABLED" = "xyes"; then
      GDALFORMATS_ENABLED="$GDALFORMATS_ENABLED ozi"
  fi
fi

if test "$CURL_SETTING" = "yes" ; then

  CURL_INC=`$LIBCURL_CONFIG --cflags`
  CURL_LIB=`$LIBCURL_CONFIG --libs`
m4_foreach_w([frmt],CURL_FORMATS,[
  driver_enabled=m4_join([_],[$INTERNAL_FORMAT],frmt,[ENABLED])
  if test "x$driver_enabled" = "xyes"; then
    GDALFORMATS_ENABLED="$GDALFORMATS_ENABLED frmt"
  fi
])
m4_foreach_w([frmt],CURL_DRIVERS,[
  driver_enabled=m4_join([_],[$INTERNAL_FORMAT],frmt,[ENABLED])
  if test "x$driver_enabled" = "xyes"; then
    OGRFORMATS_ENABLED="$OGRFORMATS_ENABLED frmt"
    OGRFORMATS_ENABLED_CFLAGS="$OGRFORMATS_ENABLED_CFLAGS _OGRDEFINE(frmt)"
  fi
])

fi

if test "$HAVE_SQLITE3" = "yes"; then
m4_foreach_w([frmt],SQLITE_FORMATS,[
  driver_enabled=m4_join([_],[$INTERNAL_FORMAT],frmt,[ENABLED])
  if test "x$driver_enabled" = "xyes"; then
    GDALFORMATS_ENABLED="$GDALFORMATS_ENABLED frmt"
  fi
])

m4_foreach_w([frmt],SQLITE_DRIVERS,[
  driver_enabled=m4_join([_],[$INTERNAL_FORMAT],frmt,[ENABLED])
  if test "x$driver_enabled" = "xyes"; then
    OGRFORMATS_ENABLED="$OGRFORMATS_ENABLED frmt"
    OGRFORMATS_ENABLED_CFLAGS="$OGRFORMATS_ENABLED_CFLAGS _OGRDEFINE(frmt)"
  fi
])

fi

dnl ---------------------------------------------------------------------------
dnl Select an PostgreSQL Library to use, or disable driver.
dnl ---------------------------------------------------------------------------

PG_CONFIG=no

AC_ARG_WITH(pg,
	    AS_HELP_STRING([--with-pg[=ARG]],
	       [Include PostgreSQL GDAL/OGR Support (ARG=yes,no)]),,)

if test "x$with_pg" = "xyes" -o "x$with_pg" = "x" ; then
  PG_CONFIG=yes
elif test "x$with_pg" != "xno"; then
  AC_MSG_ERROR([Only --with-pg=yes/no supported])
fi

AC_MSG_CHECKING([for PostgreSQL])

if test "x$PG_CONFIG" = "xno" ; then

  HAVE_PG=no
  PG_LIB=
  PG_INC=

  AC_MSG_RESULT([no])

else

  PKG_PROG_PKG_CONFIG([0.21])
  PKG_CHECK_MODULES([PQ],[libpq > 9.1], [HAVE_PG=yes], [HAVE_PG=no])

  if test "${HAVE_PG}" = "yes" ; then
    PG_LIB="${PQ_LIBS}"
    PG_INC="${PQ_CFLAGS}"
    SAVED_LIBS="${LIBS}"
    LIBS="${PG_LIB}"
    AC_CHECK_LIB(pq,PQconnectdb,HAVE_PG=yes,HAVE_PG=no)
    LIBS="${SAVED_LIBS}"
    if test "${HAVE_PG}" = "yes" ; then
      LIBS="${PG_LIB} ${LIBS}"
    fi
  else
    if test "x$with_pg" = "xyes"; then
      AC_MSG_ERROR([--with-pg was requested, but libpq is not available])
    fi
  fi

fi

AC_SUBST(HAVE_PG,$HAVE_PG)
AC_SUBST(PG_INC,$PG_INC)
AC_SUBST(PG_LIB,$PG_LIB)

dnl ---------------------------------------------------------------------------
dnl Check if we should build with GRASS support.
dnl ---------------------------------------------------------------------------

GRASS_SETTING=no
GRASS_INCLUDE=
HAVE_GRASS=no
export GRASS_INCLUDE GRASS_SETTING

AC_ARG_WITH(grass,[  --with-grass[=ARG]      Include GRASS support (GRASS 5.7+, ARG=GRASS install tree dir)],,)

AC_ARG_WITH(libgrass,[  --with-libgrass[=ARG]   Include GRASS support based on libgrass (GRASS 5.0+)],,)

# default use of grass libraries off since we can't easily auto-find them.
if test "$with_grass" = "" ; then
  with_grass=no
fi

# Check for GRASS 5.7.0 or later library use.
# TODO: separate libs for rasters and vectors
if test "$with_grass" != "yes" -a "$with_grass" != "no" ; then

  AC_CHECK_LIB(grass_gis,G_is_initialized,GRASS_SETTING=grass70+,GRASS_SETTING=no,-L$with_grass/lib -lgrass_datetime)
  if test "$GRASS_SETTING" = "no" ; then
    AC_CHECK_LIB(grass_gis,G_asprintf,GRASS_SETTING=grass57+,GRASS_SETTING=no,-L$with_grass/lib -lgrass_datetime)
  fi

  if test "$GRASS_SETTING" != "no" ; then
    if test "$GRASS_SETTING" = "grass70+" ; then
      G_RASTLIBS="-lgrass_raster -lgrass_imagery"
      G_VECTLIBS="-lgrass_vector -lgrass_dig2 -lgrass_dgl -lgrass_rtree -lgrass_linkm -lgrass_dbmiclient -lgrass_dbmibase"
      LIBS="-L$with_grass/lib $G_VECTLIBS $G_RASTLIBS -lgrass_gproj -lgrass_gmath -lgrass_gis -lgrass_datetime $LIBS"
    else
      G_RASTLIBS="-lgrass_I"
      G_VECTLIBS="-lgrass_vect -lgrass_dig2 -lgrass_dgl -lgrass_rtree -lgrass_linkm -lgrass_dbmiclient -lgrass_dbmibase"
      LIBS="-L$with_grass/lib $G_VECTLIBS $G_RASTLIBS -lgrass_gproj -lgrass_vask -lgrass_gmath -lgrass_gis -lgrass_datetime $LIBS"
    fi
    GRASS_INCLUDE="-I$with_grass/include"
    GRASS_GISBASE="$with_grass"
    HAVE_GRASS=yes
  else
    AC_MSG_ERROR([--with-grass=$with_grass requested, but libraries not found!])
  fi

# Check if libgrass is disabled.
elif test "$with_libgrass" = "no" ; then

  echo "GRASS support disabled."

elif test "$with_libgrass" = "yes" -o "$with_libgrass" = "" ; then

  AC_CHECK_LIB(grass5,G_gisinit_2,GRASS_SETTING=libgrass,GRASS_SETTING=no,)

  if test "$GRASS_SETTING" = "libgrass" ; then
    LIBS="-lgrass5 $LIBS"
  fi

else

  AC_CHECK_LIB(grass5,G_gisinit_2,GRASS_SETTING=libgrass,GRASS_SETTING=no,-L$with_libgrass/lib)

  if test "$GRASS_SETTING" = "libgrass" ; then
    LIBS="-L$with_libgrass -L$with_libgrass/lib -lgrass5 $LIBS"
    GRASS_INCLUDE="-I$with_libgrass -I$with_libgrass/include $EXTRA_INCLUDES"
  else
    AC_MSG_ERROR([--with-libgrass=$with_grass requested, but libgrass5 not found!])
  fi
fi

AC_SUBST(GRASS_SETTING,$GRASS_SETTING)
AC_SUBST(GRASS_INCLUDE,$GRASS_INCLUDE)
AC_SUBST(GRASS_GISBASE,$GRASS_GISBASE)
AC_SUBST(HAVE_GRASS)

if test "$GRASS_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="grass $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Check if cfitsio library is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(cfitsio,[  --with-cfitsio[=ARG]    Include FITS support (ARG=no or libcfitsio path)],,)

if test "$with_cfitsio" = "no" ; then

  FITS_SETTING=no

  echo "FITS support disabled."

elif test "$with_cfitsio" = "yes" -o "$with_cfitsio" = "" ; then

  AC_CHECK_LIB(cfitsio,ffopen,FITS_SETTING=external,FITS_SETTING=no,)

  if test "$FITS_SETTING" = "external" ; then
    LIBS="-lcfitsio $LIBS"
    echo "using pre-installed libcfitsio."
  else
    echo "libcfitsio not found - FITS support disabled"
  fi

dnl Fedora has cfitsio headers in /usr/include/cfitsio
  if test "$FITS_SETTING" = "external" -a -d /usr/include/cfitsio ; then
    EXTRA_INCLUDES="-I/usr/include/cfitsio $EXTRA_INCLUDES"
  fi

else

  FITS_SETTING=external
  LIBS="-L$with_cfitsio -L$with_cfitsio/lib -lcfitsio $LIBS"
  EXTRA_INCLUDES="-I$with_cfitsio -I$with_cfitsio/include $EXTRA_INCLUDES"

  echo "using libcfitsio from $with_cfitsio."
fi

AC_SUBST(FITS_SETTING,$FITS_SETTING)

if test "$FITS_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="fits $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Check if PCRaster (libcsf) library is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(pcraster,[  --with-pcraster[=ARG]   Include PCRaster (libcsf) support (ARG=internal, no or path)],,)

if test "$with_pcraster" = "no" ; then

  PCRASTER_SETTING=no

  echo "PCRaster support disabled."

elif test "$with_pcraster" = "yes" -o "$with_pcraster" = "" ; then

  AC_CHECK_LIB(csf,Mopen,PCRASTER_SETTING=external,PCRASTER_SETTING=internal,$LIBS)
  AC_CHECK_HEADERS(csf.h)

  if test "$PCRASTER_SETTING" = "external" -a "$ac_cv_header_csf_h" = "no" ; then
    PCRASTER_SETTING=internal
  fi

  if test "$PCRASTER_SETTING" = "external" ; then
    LIBS="-lcsf $LIBS"
    echo "using pre-installed libcsf."
  else
    echo "using internal csf code."
  fi

elif test "$with_pcraster" = "internal" ; then

  PCRASTER_SETTING=internal

  echo "using internal csf code."

else

  PCRASTER_SETTING=external
  LIBS="-L$with_pcraster/lib -lcsf $LIBS"
  EXTRA_INCLUDES="-I$with_pcraster/include $EXTRA_INCLUDES"

  echo "using libcsf from $with_pcraster."

fi

AC_SUBST(PCRASTER_SETTING,$PCRASTER_SETTING)

if test "$PCRASTER_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="pcraster $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Select a PNG Library to use, or disable driver.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([png], [  --with-png[=ARG]        Include PNG support (ARG=internal, no or path)],,)

AC_MSG_CHECKING([for libpng])

if test "$with_png" = "no" ; then

  PNG_SETTING=no

  echo "png support disabled."

elif test "$with_png" = "yes" -o "$with_png" = "" ; then

  AC_CHECK_LIB([png], [png_set_IHDR], [PNG_SETTING=external], [PNG_SETTING=internal], [$LIBS])
  AC_CHECK_HEADERS([png.h])

  if test "$PNG_SETTING" = "external" -a "$ac_cv_header_png_h" = "no" ; then
    PNG_SETTING=internal
  fi
  if test "$PNG_SETTING" = "external" ; then
    LIBS="-lpng $LIBS"
    echo "using pre-installed libpng."
  else
    echo "using internal png code."
  fi

elif test "$with_png" = "internal" ; then

  PNG_SETTING=internal

  echo "using internal png code."

else

  PNG_SETTING=external
  LIBS="-L$with_png -L$with_png/lib -lpng $LIBS"
  EXTRA_INCLUDES="-I$with_png -I$with_png/include $EXTRA_INCLUDES"

  echo "using libpng from $with_png."

fi

AC_SUBST([PNG_SETTING], [$PNG_SETTING])

if test "$PNG_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="png $OPT_GDAL_FORMATS"
  if test "x$INTERNAL_FORMAT_grib_ENABLED" = "xyes"; then
  	GDALFORMATS_ENABLED="$GDALFORMATS_ENABLED grib"
  fi
fi

dnl ---------------------------------------------------------------------------
dnl Check if user requests renaming internal libpng symbols
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(rename_internal_libpng_symbols,[  --with-rename-internal-libpng-symbols[=ARG] Prefix internal libpng symbols with gdal_ (ARG=yes/no)],,)

RENAME_INTERNAL_PNG_SYMBOLS=no
if test "x$with_rename_internal_libpng_symbols" = "xyes" -o "x$with_rename_internal_libpng_symbols" = "x"; then
    RENAME_INTERNAL_PNG_SYMBOLS=yes
fi

AC_SUBST(RENAME_INTERNAL_PNG_SYMBOLS,$RENAME_INTERNAL_PNG_SYMBOLS)

dnl ---------------------------------------------------------------------------
dnl Enable DDS driver.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([dds], [  --with-dds[=ARG]        Include DDS support (ARG=no, or path)],,)

AC_MSG_CHECKING([for libcrunch])

if test "$with_dds" = "no" -o "$with_dds" = ""; then

  DDS_SETTING="no"
  CRUNCHDIR=""

  echo "dds support disabled."

else

  DDS_SETTING=yes
  CRUNCHDIR="$with_dds"
  LIBS="-L$with_dds/lib/ -lcrunch $LIBS"
  echo "using libcrunch from $with_dds."

fi

AC_SUBST(CRUNCHDIR,$CRUNCHDIR)
AC_SUBST([DDS_SETTING], [$DDS_SETTING])

if test "$DDS_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="dds $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Check if GTA library is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([gta],[  --with-gta[=ARG]        Include GTA support (ARG=no or libgta tree prefix)],,)

if test "$with_gta" = "no" ; then

  GTA_SETTING=no

  echo "GTA support disabled."

elif test "$with_gta" = "yes" -o "$with_gta" = "" ; then

  AC_CHECK_LIB([gta], [gta_version], [GTA_SETTING=yes], [GTA_SETTING=no],)

  if test "$GTA_SETTING" = "yes" ; then
    LIBS="-lgta $LIBS"
    echo "using pre-installed libgta."
  else
    echo "libgta not found - GTA support disabled"
  fi

else

  GTA_SETTING=yes
  LIBS="-L$with_gta -L$with_gta/lib -lgta $LIBS"
  EXTRA_INCLUDES="-I$with_gta -I$with_gta/include $EXTRA_INCLUDES"

  echo "using libgta from $with_gta."
fi

AC_SUBST([GTA_SETTING], [$GTA_SETTING])

if test "$GTA_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="gta $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Select PCIDSK options.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([pcidsk], [  --with-pcidsk[=ARG]     Path to external PCIDSK SDK or internal (default)],,)

AC_MSG_CHECKING([for PCIDSK])

PCIDSK_LIB=
PCIDSK_INCLUDE=

if test "$with_pcidsk" = "no" ; then

  PCIDSK_SETTING=no

  echo "pcidsk support disabled."

elif test "$with_pcidsk" = "" -o "$with_pcidsk" = "yes" -o "$with_pcidsk" = "internal" ; then

  PCIDSK_SETTING=internal

  echo "using internal libpcidsk."

else

  PCIDSK_SETTING=external
  PCIDSK_LIB="-L$with_pcidsk/lib -lpcidsk"
  PCIDSK_INCLUDE="-I$with_pcidsk/include -I$with_pcidsk/include/pcidsk"

  echo "using libpcidsk from $with_pcidsk."

fi

if test "$with_pcidsk" != "no" ; then

  AC_SUBST([PCIDSK_SETTING], [$PCIDSK_SETTING])
  AC_SUBST([PCIDSK_LIB], [$PCIDSK_LIB])
  AC_SUBST([PCIDSK_INCLUDE], [$PCIDSK_INCLUDE])

fi

dnl ---------------------------------------------------------------------------
dnl Select a libgeotiff library to use.
dnl ---------------------------------------------------------------------------

GEOTIFF_INCLUDE=
AC_ARG_WITH(geotiff,[  --with-geotiff=ARG    Libgeotiff library to use (ARG=internal, yes or path)],,)

if test "$with_geotiff" = "yes" -o "$with_geotiff" = "" ; then

  if test "$TIFF_SETTING" = "internal" ; then
    GEOTIFF_SETTING=internal
  else
    dnl We now require libgeotiff 1.5.0
    AC_CHECK_LIB(geotiff,GTIFAttachPROJContext,GEOTIFF_SETTING=external,GEOTIFF_SETTING=internal)

  fi

  if test "$GEOTIFF_SETTING" = "external" ; then

    dnl Now search for headers
    if test -r /usr/include/geotiff.h ; then
      GEOTIFF_INCLUDE=
    dnl Debian (at least some versions of it) install in /usr/include/geotiff
    elif test -r /usr/include/geotiff/geotiff.h ; then
      GEOTIFF_INCLUDE="-I/usr/include/geotiff"
    dnl Fedora and OpenSuse in /usr/include/libgeotiff (#4706)
    elif test -r /usr/include/libgeotiff/geotiff.h ; then
      GEOTIFF_INCLUDE="-I/usr/include/libgeotiff"
    else
      AC_CHECK_HEADERS([geotiff.h])
      if test "$ac_cv_header_geotiff_h" = "no" ; then
        AC_MSG_ERROR([cannot find geotiff.h])
      fi
    fi

    LIBS="-lgeotiff $LIBS"
    echo "using pre-installed libgeotiff."

  else

    echo "using internal GeoTIFF code."

  fi

elif test "$with_geotiff" = "internal" ; then

  GEOTIFF_SETTING=internal

  echo "using internal GeoTIFF code."

elif test "x${with_geotiff}" = "xno" ; then

  AC_MSG_ERROR([libgeotiff is a required dependency])

else

  GEOTIFF_SETTING=external

  dnl We now require libgeotiff 1.5.0
  dnl first check if $with_geotiff/lib has the library:
  AC_CHECK_LIB(geotiff,GTIFAttachPROJContext,GEOTIFF_SETTING=external,GEOTIFF_SETTING=not_found,-L$with_geotiff/lib)

  if test $GEOTIFF_SETTING = "external" ; then
    LIBS="-L$with_geotiff/lib -lgeotiff $LIBS"
    if test  -d $with_geotiff/include ; then
      EXTRA_INCLUDES="-I$with_geotiff/include $EXTRA_INCLUDES"
    fi
  else
    dnl check if $with_geotiff itself contains the header and library (e.g. as an uninstalled build directory would)
    AC_CHECK_LIB(geotiff,GTIFAttachPROJContext,GEOTIFF_SETTING=external,AC_MSG_ERROR([We require at least GeoTIFF 1.5.0. Consider using the one supplied with GDAL]),-L$with_geotiff)
    if test $GEOTIFF_SETTING = "external" ; then
      LIBS="-L$with_geotiff -lgeotiff $LIBS"
      EXTRA_INCLUDES="-I$with_geotiff $EXTRA_INCLUDES"
    fi
  fi

  echo "using libgeotiff from $with_geotiff."
fi

AC_SUBST(GEOTIFF_SETTING,$GEOTIFF_SETTING)
AC_SUBST(GEOTIFF_INCLUDE,$GEOTIFF_INCLUDE)


dnl ---------------------------------------------------------------------------
dnl Check for CharLS
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(charls,
            AS_HELP_STRING([--with-charls],[Include JPEG-Lossless support]),,)

HAVE_CHARLS=no
CHARLS_INC=

if test "$with_charls" = "yes" -o "$with_charls" = "" ; then
  dnl CharLS 2.1
  AC_CHECK_HEADERS([charls/version.h])
  if test "$ac_cv_header_charls_version_h" = "yes"; then
    AC_LANG_PUSH([C++])
    AC_CHECK_LIB(charls,JpegLsDecode,HAVE_CHARLS=yes)
    AC_LANG_POP([C++])
    if test "$HAVE_CHARLS" = "yes"; then
        LIBS="-lcharls $LIBS"
        CHARLS_INC="-DCHARLS_2_1"
    else
        AC_LANG_PUSH([C++])
        AC_CHECK_LIB(CharLS,JpegLsDecode,HAVE_CHARLS=yes)
        AC_LANG_POP([C++])
        if test "$HAVE_CHARLS" = "yes"; then
            LIBS="-lCharLS $LIBS"
            CHARLS_INC="-DCHARLS_2_1"
        fi
    fi
  else
    dnl CharLS 1.x
    AC_CHECK_HEADERS([CharLS/interface.h])
    if test "$ac_cv_header_CharLS_interface_h" = "yes"; then
      AC_CHECK_LIB(CharLS,JpegLsDecode,HAVE_CHARLS=yes)
      if test "$HAVE_CHARLS" = "yes"; then
          LIBS="-lCharLS $LIBS"
          CHARLS_INC="-DCHARLS_INTERFACE_H"
      fi
    else
      dnl CharLS 2.0
      AC_CHECK_HEADERS([CharLS/charls.h])
      if test "$ac_cv_header_CharLS_charls_h" = "yes"; then
        AC_LANG_PUSH([C++])
        AC_CHECK_LIB(CharLS,JpegLsDecode,HAVE_CHARLS=yes)
        AC_LANG_POP([C++])
        if test "$HAVE_CHARLS" = "yes"; then
            LIBS="-lCharLS $LIBS"
            CHARLS_INC="-DCHARLS_2"
        fi
      fi
    fi
  fi
fi

AC_SUBST(HAVE_CHARLS,$HAVE_CHARLS)
AC_SUBST(CHARLS_INC,$CHARLS_INC)

dnl ---------------------------------------------------------------------------
dnl Check for JPEG 12 bit
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([jpeg12],
	    AS_HELP_STRING([--without-jpeg12],
	       [Disable JPEG 8/12bit TIFF support]),,)

TIFF_JPEG12_ENABLED=no
AC_MSG_CHECKING([for jpeg12])
if test "$with_jpeg12" =  no ; then
    JPEG12_ENABLED=no
    AC_MSG_RESULT([disabled by user])

elif test "$JPEG_SETTING" = "no" -a "$with_jpeg12" = ""; then
    JPEG12_ENABLED=no
    AC_MSG_RESULT([disabled, JPEG disabled])

elif test "$TIFF_SETTING" = "internal" ; then
    AC_MSG_RESULT([enabled])
    JPEG12_ENABLED=yes
    TIFF_JPEG12_ENABLED=yes

else
    AC_MSG_RESULT([enabled])
    JPEG12_ENABLED=yes

fi

AC_SUBST(JPEG12_ENABLED,$JPEG12_ENABLED)
AC_SUBST(TIFF_JPEG12_ENABLED,$TIFF_JPEG12_ENABLED)

dnl ---------------------------------------------------------------------------
dnl Select a GIF Library to use, or disable driver.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(gif,[  --with-gif[=ARG]        Include GIF support (ARG=internal, no or path)],,)

if test "$with_gif" = "no" ; then

  GIF_SETTING=no

  echo "gif support disabled."

elif test "$with_gif" = "yes" -o "$with_gif" = "" ; then

  AC_CHECK_LIB(gif,DGifOpenFileName,GIF_SETTING=external,GIF_SETTING=internal,)
  AC_CHECK_HEADERS(gif_lib.h)

  if test "$GIF_SETTING" = "external" ; then
    LIBS="-lgif $LIBS"
    echo "using pre-installed libgif."
  else
    echo "using internal gif code."
  fi

elif test "$with_gif" = "internal" ; then

  GIF_SETTING=internal

  echo "using internal gif code."

else

  GIF_SETTING=external
  LIBS="-L$with_gif -L$with_gif/lib -lgif $LIBS"
  EXTRA_INCLUDES="-I$with_gif -I$with_gif/include $EXTRA_INCLUDES"

  echo "using libgif from $with_gif."

fi

AC_SUBST(GIF_SETTING,$GIF_SETTING)

if test "$GIF_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="gif $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Select an OGDI Library to use, or disable driver.
dnl ---------------------------------------------------------------------------

OGDI_INCLUDE=
export OGDI_INCLUDE

AC_ARG_WITH(ogdi,[  --with-ogdi[=ARG]       Include OGDI support (ARG=path)],,)

if test "$with_ogdi" = "no" ; then

  HAVE_OGDI=no

  echo "ogdi support disabled."

elif test "$with_ogdi" = "yes" -o "$with_ogdi" = "" ; then

  PKG_CHECK_MODULES([OGDI], [ogdi], [HAVE_OGDI=yes], [:])
  if test "$HAVE_OGDI" = yes; then
    OGDI_INCLUDE=$OGDI_CFLAGS
    LIBS="$OGDI_LIBS $LIBS"
  else
    AC_CHECK_HEADERS(ecs.h)
    if test "$ac_cv_header_ecs_h" = "no" ; then
      if test -f "/usr/include/ogdi/ecs.h"; then
          OGDI_INCLUDE="-I/usr/include/ogdi"
      elif test -f "/usr/local/include/ogdi/ecs.h"; then
          OGDI_INCLUDE="-I/usr/local/include/ogdi"
      elif test -f "/usr/include/ecs.h"; then
          OGDI_INCLUDE="-I/usr/include"
      elif test -f "/usr/local/include/ecs.h"; then
          OGDI_INCLUDE="-I/usr/local/include"
      fi
    fi

    if test "$OGDI_INCLUDE" != "" -o "$ac_cv_header_ecs_h" = "yes"; then
      AC_CHECK_LIB(ogdi,cln_GetLayerCapabilities,HAVE_OGDI=yes,HAVE_OGDI=no,)
      if test "$HAVE_OGDI" = "yes" ; then
        LIBS="-logdi $LIBS"
      else
        dnl For backward compatibility. Retry with ogdi31 as a name
        AC_CHECK_LIB(ogdi31,cln_GetLayerCapabilities,HAVE_OGDI=yes,HAVE_OGDI=no,)
        if test "$HAVE_OGDI" = "yes" ; then
          LIBS="-logdi31 $LIBS"
        fi
      fi
    else
      HAVE_OGDI=no
    fi
  fi

else

  AC_CHECK_LIB(ogdi,cln_GetLayerCapabilities,HAVE_OGDI=yes,HAVE_OGDI=no,-L$with_ogdi -L$with_ogdi/lib -logdi)
  if test "$HAVE_OGDI" = "yes" ; then
    if test -f "$with_ogdi/ecs.h" -o -f "$with_ogdi/include/ecs.h"; then
        LIBS="-L$with_ogdi -L$with_ogdi/lib -logdi $LIBS"
        OGDI_INCLUDE="-I$with_ogdi -I$with_ogdi/include"

        echo "using libogdi from $with_ogdi."
     else
        HAVE_OGDI=no
        AC_MSG_ERROR([ecs.h not found.])
     fi
  else
    dnl For backward compatibility. Retry with ogdi31 as a name
    AC_CHECK_LIB(ogdi31,cln_GetLayerCapabilities,HAVE_OGDI=yes,HAVE_OGDI=no,-L$with_ogdi -L$with_ogdi/lib -logdi31)
    if test "$HAVE_OGDI" = "yes" ; then
      if test -f "$with_ogdi/ecs.h" -o -f "$with_ogdi/include/ecs.h"; then
        LIBS="-L$with_ogdi -L$with_ogdi/lib -logdi31 $LIBS"
        OGDI_INCLUDE="-I$with_ogdi -I$with_ogdi/include"

        echo "using libogdi31 from $with_ogdi."
      else
        HAVE_OGDI=no
        AC_MSG_ERROR([ecs.h not found.])
      fi
    else
      AC_MSG_ERROR([libogdi not found.])
    fi
  fi

fi

AC_SUBST(HAVE_OGDI,$HAVE_OGDI)
AC_SUBST(OGDI_INCLUDE,$OGDI_INCLUDE)

dnl ---------------------------------------------------------------------------
dnl Select FME_HOME or disable FME support.
dnl ---------------------------------------------------------------------------

X_FME_HOME=
export X_FME_HOME

AC_ARG_WITH(fme,[  --with-fme[=ARG]        Include FMEObjects support (ARG=FME_HOME path)],,)

AC_MSG_CHECKING([for FMEObjects])

if test "$with_fme" = "no" ; then

  AC_MSG_RESULT([disabled by user])

elif test "$with_fme" = "yes" ; then

  if test "$FME_HOME" = "" ; then
    AC_MSG_ERROR([no, FME_HOME not defined!])
  elif test -f $FME_HOME/fmeobjects/cpp/isession.h ; then
    AC_MSG_RESULT([yes])
    X_FME_HOME=$FME_HOME
  else
    AC_MSG_ERROR([FME_HOME defined, but $FME_HOME/fmeobjects/cpp/issesion.h not found.])
  fi

elif test "$with_fme" = "" ; then

  if test "$FME_HOME" = "" ; then
    AC_MSG_RESULT([no])
  elif test -f $FME_HOME/fmeobjects/cpp/isession.h ; then
    AC_MSG_RESULT([yes])
    X_FME_HOME=$FME_HOME
  else
    AC_MSG_RESULT([no, FME_HOME defined, but $FME_HOME/fmeobjects/cpp/issesion.h not found.])
  fi

else

  if test -f $with_fme/fmeobjects/cpp/isession.h ; then
    AC_MSG_RESULT([yes])
    X_FME_HOME=$with_fme
  else
    AC_MSG_ERROR([$with_fme/fmeobjects/cpp/issesion.h not found!])
  fi

fi

AC_SUBST(X_FME_HOME,$X_FME_HOME)

dnl ---------------------------------------------------------------------------
dnl Select a SOSI lib to use, or disable driver.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(sosi,[  --with-sosi[=ARG]        Include SOSI support (ARG=SOSI lib Path, yes or no)],,)

if test -z "$with_sosi" -o "$with_sosi" = "no" ; then

  SOSI_ENABLED=no

  echo "SOSI support disabled."

elif test "$with_sosi" = "yes" ; then

    AC_MSG_CHECKING([for libfyba])

    rm -f testfyba.*
    rm -f testfyba
    echo "#include <fyba.h>" > testfyba.cpp
    echo "int main() { LC_Init(); return 0; }" >> testfyba.cpp
    if test  -r /usr/include/fyba -a -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -I/usr/include/fyba -DLINUX testfyba.cpp -lfyba -lfygm -lfyut -o testfyba 2>&1`" ; then
        AC_MSG_RESULT([found])
        SOSI_ENABLED=yes
        SOSI_LIB="-lfyba -lfygm -lfyut"
        SOSI_INC="-I/usr/include/fyba"
    else
        AC_MSG_RESULT([not found])
        AC_MSG_CHECKING([for libfyba.a, libfygm.a and libfyut.a in /usr/local/lib])
        if test -r /usr/local/lib/libfyba.a -a -r /usr/local/lib/libfygm.a -a -r /usr/local/lib/libfyut.a ; then
            AC_MSG_RESULT([found])
            SOSI_ENABLED=yes
            SOSI_LIB="/usr/local/lib/libfyba.a /usr/local/lib/libfygm.a /usr/local/lib/libfyut.a"
        else
            AC_MSG_ERROR([not found.])
        fi

        AC_MSG_CHECKING([for fyba.h, fygm.h and fyut.h in /usr/local/include/fyba])
        if test -r /usr/local/include/fyba/fyba.h -a -r /usr/local/include/fyba/fygm.h -a -r /usr/local/include/fyba/fyut.h ; then
            AC_MSG_RESULT([found])
            SOSI_ENABLED=yes
            SOSI_INC="-I/usr/local/include/fyba"
        else
            AC_MSG_ERROR([not found.])
        fi
    fi

    rm -f testfyba.*
    rm -f testfyba
else

  AC_MSG_CHECKING([for libfyba.a, libfygm.a and libfyut.a in $with_sosi/lib])
  if test -r $with_sosi/lib/libfyba.a -a -r $with_sosi/lib/libfygm.a -a -r $with_sosi/lib/libfyut.a ; then
    AC_MSG_RESULT([found.])
    SOSI_LIB="$with_sosi/lib/libfyba.a $with_sosi/lib/libfygm.a $with_sosi/lib/libfyut.a"
    SOSI_ENABLED=yes
  else
    AC_MSG_ERROR([not found.])
  fi

  AC_MSG_CHECKING([for fyba.h in $with_sosi/include/fyba])
  if test -r $with_sosi/include/fyba/fyba.h -a -r $with_sosi/include/fyba/fygm.h -a -r $with_sosi/include/fyba/fyut.h ; then
    AC_MSG_RESULT([found.])
    SOSI_INC="-I$with_sosi/include/fyba"
    SOSI_ENABLED=yes
  else
    AC_MSG_ERROR([not found.])
  fi

fi

AC_SUBST(SOSI_ENABLED, $SOSI_ENABLED)
AC_SUBST(SOSI_LIB, $SOSI_LIB)
AC_SUBST(SOSI_INC, $SOSI_INC)

dnl ---------------------------------------------------------------------------
dnl MongoCXX driver
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(mongocxx,[  --with-mongocxx[=ARG]        Include MongoCXX support (ARG=Path, yes or no)],,)

MONGODB_ENABLED=no

if test "$with_mongocxx" = "no" ; then

  echo "MONGODB support disabled."

elif test "$with_mongocxx" = "yes" -o "$with_mongocxx" = "" ; then

    AC_MSG_CHECKING([for MongoCXX])

    rm -f testmongocxx.*
    rm -f testmongocxx
    echo "#include <mongo/client/dbclient.h>" > testmongocxx.cpp
    echo "using namespace mongo;" >> testmongocxx.cpp
    echo "int main() { client::initialize(client::Options()); return 0; }" >> testmongocxx.cpp
    if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} testmongocxx.cpp -lmongoclient -lboost_system -lboost_thread -lboost_regex -o testmongocxx 2>&1`" ; then
        AC_MSG_RESULT([MongoCXX found])
        MONGODB_ENABLED="yes"
        MONGODB_LIB=" -lmongoclient -lboost_system -lboost_thread -lboost_regex"
    else
        if test "$with_mongocxx" = "yes"; then
            AC_MSG_ERROR([MongoCXX not found.])
        else
            AC_MSG_RESULT([MongoCXX not found.])
        fi
    fi
    rm -f testmongocxx.*
    rm -f testmongocxx

else

  AC_ARG_WITH(boost-lib-path,
          [  --with-boost-lib-path=ARG   Path to boost libraries for mongocxx client],,,)

  AC_MSG_CHECKING([for libmongoclient.so in in $with_mongocxx/lib])
  MONGODB_ENABLED=yes
  if test -r $with_mongocxx/lib/libmongoclient.so; then
    AC_MSG_RESULT([found.])
    MONGODB_LIB="-L$with_mongocxx/lib -lmongoclient"
  elif test -r $with_mongocxx/lib/libmongoclient.dylib; then
    AC_MSG_RESULT([found.])
    MONGODB_LIB="-L$with_mongocxx/lib -lmongoclient"
  else
    AC_MSG_ERROR([not found.])
  fi

  if test "$with_boost_lib_path" = "" ; then
    MONGODB_LIB="$MONGODB_LIB -lboost_system -lboost_thread -lboost_regex"
  else
    MONGODB_LIB="$MONGODB_LIB -L$with_boost_lib_path -lboost_system -lboost_thread -lboost_regex"
  fi

  AC_MSG_CHECKING([for mongo/client/dbclient.h in $with_mongocxx/include])
  if test -r $with_mongocxx/include/mongo/client/dbclient.h ; then
    AC_MSG_RESULT([found.])
    MONGODB_INC="-I$with_mongocxx/include"
  else
    AC_MSG_ERROR([not found.])
  fi

fi

AC_SUBST(MONGODB_ENABLED, $MONGODB_ENABLED)
AC_SUBST(MONGODB_LIB, $MONGODB_LIB)
AC_SUBST(MONGODB_INC, $MONGODB_INC)


dnl ---------------------------------------------------------------------------
dnl MongoCXXv3 driver
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(mongocxxv3,[  --with-mongocxxv3[=ARG]        Include MongoCXXv3 support (ARG=yes or no)],,)

MONGOCXXV3_ENABLED=no

if test "$with_mongocxxv3" = "no" ; then

  echo "MONGODBv3 support disabled."

elif test "$with_mongocxxv3" = "yes" -o "$with_mongocxxv3" = "" ; then

  PKG_PROG_PKG_CONFIG([0.21])
  PKG_CHECK_MODULES([MONGOCXXV3], [libmongocxx >= 3.4.0], [MONGOCXXV3_ENABLED=yes], [MONGOCXXV3_ENABLED=no])

  if test "$MONGOCXXV3_ENABLED" = "no" -a "$with_mongocxxv3" = "yes" ; then
    AC_MSG_ERROR([mongocxxv3 requested but no found])
  fi

fi

AC_SUBST(MONGOCXXV3_ENABLED, $MONGOCXXV3_ENABLED)
AC_SUBST(MONGOCXXV3_LIBS, $MONGOCXXV3_LIBS)
AC_SUBST(MONGOCXXV3_CFLAGS, $MONGOCXXV3_CFLAGS)


dnl ---------------------------------------------------------------------------
dnl Select an HDF4 Library to use, or disable driver.
dnl
dnl We assume the user will have libjpeg and libz from other sources when
dnl linking against static HDF4 libraries.
dnl ---------------------------------------------------------------------------

HDF4_INCLUDE=""

AC_ARG_WITH(hdf4,[  --with-hdf4[=ARG]       Include HDF4 support (ARG=path)],,)

if test x"$with_hdf4" = x"no" ; then

  HAVE_HDF4=no

  echo "hdf4 support disabled."

else

  if test x"$with_hdf4" = x"yes" -o x"$with_hdf4" = x"" ; then
dnl Fedora has hdf static libraries in /usr/lib/hdf
    if test -d /usr/lib64/hdf; then
      HDF_LIB_DIR="/usr/lib64/hdf"
    elif test -d /usr/lib/hdf; then
      HDF_LIB_DIR="/usr/lib/hdf"
    else
      HDF_LIB_DIR=""
    fi
  else
    if test -d $with_hdf4/lib ; then
      HDF_LIB_DIR="$with_hdf4/lib"
    else
      HDF_LIB_DIR="$with_hdf4"
    fi
  fi

  ORIG_LIBS="$LIBS"
  if test "$HDF_LIB_DIR" != "" ; then
    LIBS="-L$HDF_LIB_DIR $LIBS"
  fi

dnl Debian supplies the HDF4 library which does not conflict with NetCDF.
dnl Test for Debian flavor first. Hint: install the libhdf4-alt-dev package.
  AC_CHECK_LIB(mfhdfalt,SDreaddata,HDF_LIB_NAME="-lmfhdfalt -ldfalt",HDF_LIB_NAME=missing,-ldfalt)

dnl If it fails, test again for normal libmfhdf/libdf
  if test "$HDF_LIB_NAME" = "missing" ; then
    unset ac_cv_lib_mfhdf_SDreaddata
    AC_CHECK_LIB(mfhdf,SDreaddata,HDF_LIB_NAME="-lmfhdf -ldf",HDF_LIB_NAME=missing,-ldf)
    if test "$HDF_LIB_NAME" = "missing" ; then
      unset ac_cv_lib_mfhdf_SDreaddata
      AC_CHECK_LIB(hdf4,SDreaddata,HDF_LIB_NAME=-lhdf4,HDF_LIB_NAME=missing,)
    fi
  fi

dnl Test again, with -ldf, -ljpeg and -lz this time.
  if test "$HDF_LIB_NAME" = "missing" ; then
    unset ac_cv_lib_mfhdf_SDreaddata
    AC_CHECK_LIB(mfhdf,SDreaddata,HDF_LIB_NAME="-lmfhdf -ldf",HDF_LIB_NAME=missing,-ldf -ljpeg -lz)
  fi

dnl Not found... again, with -lsz.
  if test "$HDF_LIB_NAME" = "missing" ; then
    unset ac_cv_lib_mfhdf_SDreaddata
    AC_CHECK_LIB(mfhdf,SDreaddata,HDF_LIB_NAME="-lmfhdf -ldf -lsz",HDF_LIB_NAME=missing,-ldf -lsz -ljpeg -lz)
  fi

  if test "$HDF_LIB_NAME" != "missing" ; then

    if test "$HDF_LIB_DIR" != "" ; then
      LIBS="-L$HDF_LIB_DIR $HDF_LIB_NAME $ORIG_LIBS"
    else
      LIBS="$HDF_LIB_NAME $ORIG_LIBS"
    fi

    dnl HDF4 library newer than 4.2.5 has a SDreset_maxopenfiles/SDget_maxopenfiles interface
    dnl which allows opening many HDF files simultaneously (the max number of files was previously
    dnl hardcoded and too low, smth. like 32). Search for it and use if available.
    AC_CHECK_LIB(mfhdfalt,SDget_maxopenfiles,HDF4_HAS_MAXOPENFILES=yes,HDF4_HAS_MAXOPENFILES=no,$HDF_LIB_NAME)
    if test "$HDF4_HAS_MAXOPENFILES" = "no" ; then
      AC_CHECK_LIB(mfhdf,SDget_maxopenfiles,HDF4_HAS_MAXOPENFILES=yes,HDF4_HAS_MAXOPENFILES=no,$HDF_LIB_NAME)
    fi

  dnl Now search for headers
    if test "$with_hdf4" = "yes" -o "$with_hdf4" = "" -a -r /usr/include/hdf/hdf.h ; then
      HDF4_INCLUDE="-I/usr/include/hdf"
    elif test -r "$with_hdf4/hdf/hdf.h" ; then
      HDF4_INCLUDE="-I$with_hdf4/hdf"
    elif test -r "$with_hdf4/include/hdf/hdf.h" ; then
      HDF4_INCLUDE="-I$with_hdf4/include/hdf"
    elif test -r "$with_hdf4/include/hdf.h" ; then
      HDF4_INCLUDE="-I$with_hdf4/include"
    elif test -r "$with_hdf4/hdf.h" ; then
      HDF4_INCLUDE="-I$with_hdf4"
    fi

    HAVE_HDF4=yes

  else

    if test x"$with_hdf4" != x"" ; then
      AC_MSG_ERROR([HDF4 support requested with arg "$with_hdf4", but neither hdf4 nor mfhdf lib found])
    else
      LIBS="$ORIG_LIBS"
      HAVE_HDF4=no
    fi

  fi

fi

AC_SUBST(HAVE_HDF4,$HAVE_HDF4)
AC_SUBST(HDF4_INCLUDE,$HDF4_INCLUDE)
AC_SUBST(HDF4_HAS_MAXOPENFILES,$HDF4_HAS_MAXOPENFILES)

if test "$HAVE_HDF4" != "no" ; then
  OPT_GDAL_FORMATS="hdf4 $OPT_GDAL_FORMATS"
fi


dnl ---------------------------------------------------------------------------
dnl Select an HDF5 Library to use, or disable driver.
dnl
dnl ---------------------------------------------------------------------------

HDF5_INCLUDE=""

AC_ARG_WITH(hdf5,[  --with-hdf5[=ARG]       Include HDF5 support (ARG=path)],,)

if test "$with_hdf5" = "no" ; then

  HAVE_HDF5=no

  echo "hdf5 support disabled."

elif test "$with_hdf5" = "yes" -o "$with_hdf5" = "" ; then

  HDF5_CFLAGS=""
  HDF5_LIBS=""
  PKG_PROG_PKG_CONFIG([0.21]) # check and set $PKG_CONFIG
  PKG_CHECK_MODULES([HDF5], [hdf5], [HAVE_HDF5=yes], [HAVE_HDF5=no])

  if test "$HAVE_HDF5" = "yes"; then

    # Test that the package found is for the right architecture
    saved_LIBS="$LIBS"
    LIBS="$HDF5_LIBS"
    AC_CHECK_LIB(hdf5,H5Fopen, [HAVE_HDF5=yes], [HAVE_HDF5=no])
    LIBS="$saved_LIBS"

    if test "$HAVE_HDF5" = "yes"; then
        LIBS="$HDF5_LIBS $LIBS"
        HDF5_INCLUDE="$HDF5_CFLAGS"
    fi

  else

    AC_CHECK_LIB(hdf5,H5Fopen,HAVE_HDF5=yes,HAVE_HDF5=no,)

    if test "$HAVE_HDF5" = "yes" ; then
        LIBS="-lhdf5 $LIBS"
    fi

    dnl Some Linux distros install hdf include files here.
    dnl if test "$HAVE_HDF5" = "yes" -a -r /usr/include/hdf5.h ; then
    dnl    HDF5_INCLUDE="-I/usr/include"
    dnl fi
  fi

else

  if test -d $with_hdf5/lib ; then
    HDF5_LIB_DIR=$with_hdf5/lib
  else
    HDF5_LIB_DIR=$with_hdf5
  fi

  ORIG_LIBS="$LIBS"
  LIBS="-L$HDF5_LIB_DIR $LIBS -lhdf5"

  AC_CHECK_LIB(hdf5,H5Fopen,HDF5_LIB_NAME="-lhdf5 ",HDF5_LIB_NAME=missing,-lhdf5)

  if test "$HDF5_LIB_NAME" = "missing" ; then
    AC_MSG_ERROR([HDF5 support requested with arg $with_hdf5, but no hdf5 lib found])
  fi

  LIBS="-L$HDF5_LIB_DIR $HDF5_LIB_NAME $ORIG_LIBS"

  if test -r "$with_hdf5/hdf5/hdf5.h" ; then
    HDF5_INCLUDE="-I$with_hdf5/hdf5"
  elif test -r "$with_hdf5/include/hdf5/hdf5.h" ; then
    HDF5_INCLUDE="-I$with_hdf5/include/hdf5"
  elif test -r "$with_hdf5/include/hdf5.h" ; then
    HDF5_INCLUDE="-I$with_hdf5/include"
  elif test -r "$with_hdf5/hdf.h" ; then
    HDF5_INCLUDE="-I$with_hdf5"
  fi

  HAVE_HDF5=yes
fi

AC_SUBST(HAVE_HDF5,$HAVE_HDF5)
AC_SUBST(HDF5_INCLUDE,$HDF5_INCLUDE)

if test "$HAVE_HDF5" != "no" ; then
  OPT_GDAL_FORMATS="hdf5 $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Check if kealib library is available.
dnl ---------------------------------------------------------------------------

KEA_CONFIG=no

AC_ARG_WITH(kea,[  --with-kea[=ARG]      Include kealib (ARG=path to kea-config) [[default=yes]]],,)

if test "$with_kea" = "yes" -o "x$with_kea" = "x" ; then
  AC_PATH_PROG(KEA_CONFIG, kea-config, no)
else
   KEA_CONFIG=$with_kea
fi

AC_MSG_CHECKING([for kea])

if test "$KEA_CONFIG" = "no" ; then

  HAVE_KEA=no
  KEA_LIB=
  KEA_INC=

  AC_MSG_RESULT([no])

else
  if test -d $KEA_CONFIG ; then
      AC_MSG_RESULT([no])
      AC_MSG_ERROR([--with-kea argument is a directory.  It should be the path to the kea_config script, often somewhere like /usr/local/bin/kea_config.])
  fi

  if test \! -x $KEA_CONFIG ; then
      AC_MSG_RESULT([no])
      AC_MSG_ERROR([--with-kea argument is a not an executable file.  It should be the path to the kea_config script, often somewhere like /usr/local/bin/kea_config.])
  fi

  HAVE_KEA=yes
  KEA_LIB="`$KEA_CONFIG --libs --hdflibs`"
  KEA_INC="`$KEA_CONFIG --cflags --hdfcflags`"
  AC_MSG_RESULT([yes])
fi

AC_SUBST(HAVE_KEA,$HAVE_KEA)
AC_SUBST(KEA_INC,$KEA_INC)
AC_SUBST(KEA_LIB,$KEA_LIB)

if test "$HAVE_KEA" != "no" ; then
  OPT_GDAL_FORMATS="kea $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Check if netcdf library is available.
dnl ---------------------------------------------------------------------------

NETCDF_SETTING=
NETCDF_MEM=
NETCDF_ROOT=
NETCDF_HAS_NC4=
NETCDF_HAS_HDF4=
NETCDF_NCCONFIG=

AC_ARG_WITH([netcdf],[  --with-netcdf[=ARG]     Include netCDF support (ARG=no or netCDF tree prefix)],,)

if test "$with_netcdf" = "no" ; then

  NETCDF_SETTING=no
  NETCDF_MEM=no

  echo "netCDF support disabled."

else

  dnl find nc-config location
  unset ac_cv_path_NETCDF_NCCONFIG
  if test "$with_netcdf" = "yes" -o "$with_netcdf" = "" ; then
    AC_PATH_PROG(NETCDF_NCCONFIG, nc-config, no)
  else
    tmp_path="$with_netcdf/bin$PATH_SEPARATOR$with_netcdf"
    AC_PATH_PROG(NETCDF_NCCONFIG, nc-config, no, $tmp_path)
  fi

  dnl test nc-config
  if test "$NETCDF_NCCONFIG" = "no" ; then
    echo "        did not find nc-config, some features may be missing"
    echo "        use --with-netcdf=/path/to/netcdf or add nc-config to PATH"
    NETCDF_NCCONFIG=
  elif test "`$NETCDF_NCCONFIG --version`" = "" ; then
    echo "did not get netCDF version from $NETCDF_NCCONFIG ... using fallback"
    NETCDF_NCCONFIG=
  fi

  dnl test linking using flags from nc-config
  if test "$NETCDF_NCCONFIG" != "" ; then

    AC_MSG_CHECKING([libnetcdf compiler and linker flags with nc-config])
    NETCDF_VERSION=`$NETCDF_NCCONFIG --version`
    NETCDF_PREFIX=`$NETCDF_NCCONFIG --prefix`
    NETCDF_INCLUDEDIR=`$NETCDF_NCCONFIG --includedir`
    NETCDF_LIBS=`$NETCDF_NCCONFIG --libs`
    NETCDF_STATIC_LIBS=`$NETCDF_NCCONFIG --static --libs`
    STRIP_SYSTEM_LIBRARY_PATHS("${NETCDF_LIBS}")
    NETCDF_LIBS="$STRIPPED_LIBRARY_NAME"
    echo ""
    AC_MSG_RESULT([ got version="$NETCDF_VERSION", prefix="$NETCDF_PREFIX",
 libs="$NETCDF_LIBS", includedir="$NETCDF_INCLUDEDIR"])

    SAVED_LIBS="${LIBS}"
    LIBS="$NETCDF_LIBS"
    AC_CHECK_LIB([netcdf], [nc_open], [NETCDF_SETTING=yes], [NETCDF_SETTING=no],)
    if test "$NETCDF_SETTING" = "no" ; then
      unset ac_cv_lib_netcdf_nc_open
      LIBS="$NETCDF_STATIC_LIBS"
      AC_CHECK_LIB([netcdf], [nc_open], [NETCDF_SETTING=yes], [NETCDF_SETTING=no],)
    fi
    LIBS="${SAVED_LIBS}"

    if test "$NETCDF_SETTING" = "yes" ; then
      EXTRA_INCLUDES="-I$NETCDF_INCLUDEDIR $EXTRA_INCLUDES"
      NETCDF_ROOT=$NETCDF_PREFIX
      LIBS="$NETCDF_LIBS $LIBS"
    fi

dnl previous behavior without nc-config

dnl test linking using default settings
elif test "$with_netcdf" = "yes" -o "$with_netcdf" = "" ; then

  AC_CHECK_LIB([netcdf], [nc_open], [NETCDF_SETTING=yes], [NETCDF_SETTING=no],)

  if test "$NETCDF_SETTING" = "yes" ; then

    dnl Fedora has netcdf headers in /usr/include/netcdf
    if test -d /usr/include/netcdf ; then
      EXTRA_INCLUDES="-I/usr/include/netcdf $EXTRA_INCLUDES"
      NETCDF_ROOT="/usr"
    dnl RHEL 5 has netcdf headers in /usr/include/netcdf-3
    elif test -d /usr/include/netcdf-3 ; then
      EXTRA_INCLUDES="-I/usr/include/netcdf-3 $EXTRA_INCLUDES"
      NETCDF_ROOT="/usr"
    dnl ubuntu and fedora have netcdf headers in /usr/include
    elif test -f /usr/include/netcdf.h ; then
      NETCDF_ROOT="/usr"
    dnl try /usr/local
    elif test -f /usr/local/include/netcdf.h ; then
      NETCDF_ROOT="/usr/local"
    dnl print warning if include cannot be found
    else
      echo "using pre-installed libnetcdf."
      echo -n "libnetcdf is installed but its location cannot be found, "
      echo "use --with-netcdf=/path_to_netcdf for proper support"
    fi
    NETCDF_INCLUDEDIR="$NETCDF_ROOT/include/"

    LIBS="-lnetcdf $LIBS"
    if test "$NETCDF_ROOT" != "" ; then
      echo "using pre-installed libnetcdf from "$NETCDF_ROOT
    fi

  else
    echo "libnetcdf not found ... netCDF support disabled"
  fi

dnl test linking using --with_netcdf dir
else

  AC_CHECK_LIB([netcdf], [nc_open], [NETCDF_SETTING=yes], [NETCDF_SETTING=no],-L$with_netcdf -L$with_netcdf/lib)

  if test "$NETCDF_SETTING" = "yes" ; then

    NETCDF_SETTING=yes
    NETCDF_ROOT="$with_netcdf"

    if test -d $with_netcdf/lib ; then
      LIBS="-L$with_netcdf/lib -lnetcdf $LIBS"
      EXTRA_INCLUDES="-I$with_netcdf/include $EXTRA_INCLUDES"
    else
      LIBS="-L$with_netcdf -lnetcdf $LIBS"
      EXTRA_INCLUDES="-I$with_netcdf $EXTRA_INCLUDES"
    fi
    NETCDF_INCLUDEDIR="$with_netcdf/include/"

    echo "using libnetcdf from $with_netcdf"

  else
    echo "libnetcdf not found in "$with_netcdf" ... netCDF support disabled"
  fi

fi

dnl test for NC4 and HDF4 support with nc-config
  if test "$NETCDF_SETTING" = "yes" ; then

    if test "$NETCDF_NCCONFIG" != "" ; then

      AC_MSG_CHECKING([for netcdf-4 (and HDF5) support in libnetcdf])
        if test "x$($NETCDF_NCCONFIG --has-nc4)" = "xyes"; then :
          AC_MSG_RESULT([yes])
          NETCDF_HAS_NC4=yes
        else
          AC_MSG_RESULT([no])
          NETCDF_HAS_NC4=no
        fi

      AC_MSG_CHECKING([for HDF4 support in libnetcdf])
        if test "x$($NETCDF_NCCONFIG --has-hdf4)" = "xyes"; then :
          AC_MSG_RESULT([yes])
          NETCDF_HAS_HDF4=yes
        else
          AC_MSG_RESULT([no])
          NETCDF_HAS_HDF4=no
        fi

    else
      NETCDF_HAS_NC4=no
      NETCDF_HAS_HDF4=no
    fi
fi

fi

dnl export results

AC_SUBST([NETCDF_SETTING], [$NETCDF_SETTING])

if test "$NETCDF_SETTING" != "no" ; then

   NETCDF_MEM=no
   if test -f "$NETCDF_INCLUDEDIR/netcdf_mem.h"; then
       NETCDF_MEM=yes;
   fi

   AC_SUBST([NETCDF_MEM], [$NETCDF_MEM])
   AC_SUBST([NETCDF_ROOT], [$NETCDF_ROOT])
   AC_SUBST([NETCDF_HAS_NC4], [$NETCDF_HAS_NC4])
   AC_SUBST([NETCDF_HAS_HDF4], [$NETCDF_HAS_HDF4])

   OPT_GDAL_FORMATS="netcdf $OPT_GDAL_FORMATS"

fi


dnl ---------------------------------------------------------------------------
dnl Select a JasPer Library to use, or disable driver.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(jasper,[  --with-jasper[=ARG]     Include JPEG-2000 support via JasPer library (ARG=path)],,)

if test "$with_jasper" = "no" ; then

  HAVE_JASPER=no

  AC_MSG_NOTICE([JasPer (JPEG2000) support disabled.])

elif test "$with_jasper" = "yes" -o "$with_jasper" = "" ; then

  AC_CHECK_LIB(jasper,jpc_decode,HAVE_JASPER=yes,HAVE_JASPER=no,)
  AC_CHECK_LIB(jasper,jp2_decode,HAVE_JASPER=yes,HAVE_JASPER=no,)
  AC_CHECK_LIB(jasper,pgx_decode,HAVE_JASPER=yes,HAVE_JASPER=no,)

  if test "$HAVE_JASPER" = "yes" ; then
    LIBS="-ljasper $LIBS"
  fi
else

  HAVE_JASPER=yes
  LIBS="-L$with_jasper -L$with_jasper/lib -ljasper $LIBS"
  EXTRA_INCLUDES="-I$with_jasper -I$with_jasper/include $EXTRA_INCLUDES"

  AC_MSG_NOTICE([using JasPer library from $with_jasper.])
fi

if test "$HAVE_JASPER" != "no" ; then
  OPT_GDAL_FORMATS="jpeg2000 $OPT_GDAL_FORMATS"

  dnl Test whether we have UUID JasPer hack

  AC_CHECK_LIB(jasper,jp2_encode_uuid,HAVE_JASPER_UUID=yes,HAVE_JASPER_UUID=no,)

  if test "$HAVE_JASPER_UUID" = "yes" ; then
    AC_MSG_NOTICE([hacked JasPer version found (JasPer UUID), GeoJP2 enabled.])
    JASPER_FLAGS=-DHAVE_JASPER_UUID
  else
    AC_MSG_NOTICE([hacked JasPer version not found, GeoJP2 disabled.])
  fi
fi

AC_SUBST([HAVE_JASPER],$HAVE_JASPER)
AC_SUBST([JASPER_FLAGS],$JASPER_FLAGS)


dnl ---------------------------------------------------------------------------
dnl Select a OpenJPEG Library to use, or disable driver.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(openjpeg,[  --with-openjpeg     Include JPEG-2000 support via OpenJPEG 2.x library],,)

HAVE_OPENJPEG=no

if test "$with_openjpeg" = "no" ; then

  AC_MSG_NOTICE([OpenJPEG (JPEG2000) support disabled.])

else

  PKG_PROG_PKG_CONFIG([0.21])
  PKG_CHECK_MODULES([OPENJPEG], [libopenjp2 >= 2.1.0],
        [OPENJPEG_VERSION=`$PKG_CONFIG --modversion libopenjp2`],
        [OPENJPEG_VERSION=;])

  if test -n "$OPENJPEG_CFLAGS"; then

    # Test that the package found is for the right architecture
    saved_LIBS="$LIBS"
    LIBS="$OPENJPEG_LIBS"
    AC_CHECK_LIB(openjp2,opj_setup_decoder, [HAVE_OPENJPEG=yes], [HAVE_OPENJPEG=no])
    LIBS="$saved_LIBS"

    if test "$HAVE_OPENJPEG" = "yes"; then
        EXTRA_INCLUDES="$OPENJPEG_CFLAGS $EXTRA_INCLUDES"
        LIBS="$OPENJPEG_LIBS $LIBS"
    fi
  fi
fi

if test "$HAVE_OPENJPEG" != "no" ; then
  OPT_GDAL_FORMATS="openjpeg $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Select a FGDB API to use, or disable driver.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(fgdb,[  --with-fgdb[=ARG]        Include ESRI File Geodatabase support (ARG=FGDP API Path, yes or no)],,)

FGDB_ENABLED=no
FGDB_LIB=

if test "$with_fgdb" = "no" ; then

  echo "FGDB support disabled."

elif test "$with_fgdb" = "yes" -o "$with_fgdb" = "" ; then

    AC_MSG_CHECKING([for FileGDBAPI])

    rm -f testfgdb.*
    rm -f testfgdb
    echo "#include <FileGDBAPI.h>" > testfgdb.cpp
    echo "using namespace FileGDBAPI;" >> testfgdb.cpp
    echo "int main() { Geodatabase oDB; std::wstring osStr; ::OpenGeodatabase(osStr, oDB); return 0; }" >> testfgdb.cpp
    if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} testfgdb.cpp -lFileGDBAPI -lfgdbunixrtl -o testfgdb 2>&1`" ; then
        AC_MSG_RESULT([FileGDBAPI found])
        FGDB_ENABLED="yes"
        FGDB_LIB="-lFileGDBAPI -lfgdbunixrtl"
    else
        if test "$with_fgdb" = "yes"; then
            AC_MSG_ERROR([FileGDBAPI not found.])
        else
            AC_MSG_RESULT([FileGDBAPI not found.])
        fi
    fi
    rm -f testfgdb.*
    rm -f testfgdb

else

  AC_MSG_CHECKING([for libFileGDBAPI.so in in $with_fgdb/lib])
  FGDB_ENABLED=yes
  if test -r $with_fgdb/lib/libFileGDBAPI.so -a -r $with_fgdb/lib/libfgdbunixrtl.so ; then
    AC_MSG_RESULT([found.])
    FGDB_LIB="-L$with_fgdb/lib -lFileGDBAPI -lfgdbunixrtl"
  elif test -r $with_fgdb/lib/libFileGDBAPI.dylib -a -r $with_fgdb/lib/libfgdbunixrtl.dylib ; then
    AC_MSG_RESULT([found.])
    FGDB_LIB="-L$with_fgdb/lib -lFileGDBAPI -lfgdbunixrtl"
  else
    AC_MSG_ERROR([not found.])
  fi

  AC_MSG_CHECKING([for FileGDBAPI.h in $with_fgdb/include])
  if test -r $with_fgdb/include/FileGDBAPI.h ; then
    AC_MSG_RESULT([found.])
    FGDB_INC="-I$with_fgdb/include"
  else
    AC_MSG_ERROR([not found.])
  fi

fi

if test "$FGDB_LIB" != ""; then
  dnl pj_datums is not a function but an object, but for linking that doesn't matter.
  AC_CHECK_LIB(FileGDBAPI,pj_datums,FGDB_HAS_PROJ4=yes,FGDB_HAS_PROJ4=no,$FGDB_LIB)
fi

AC_SUBST(FGDB_ENABLED, $FGDB_ENABLED)
AC_SUBST(FGDB_LIB, $FGDB_LIB)
AC_SUBST(FGDB_INC, $FGDB_INC)

dnl ---------------------------------------------------------------------------
dnl Select a ECW Library to use, or disable driver.
dnl ---------------------------------------------------------------------------
ECW_FLAGS=
ECW_LIBS=
ECW_INCLUDE=
ECW_54=

AC_ARG_WITH(ecw,[  --with-ecw[=ARG]        Include ECW support (ARG=ECW SDK Path, yes or no)],,)

if test ! -z "`uname | grep Darwin`" ; then
  CARBON_FRAMEWORK="-framework Carbon"
else
  CARBON_FRAMEWORK=
fi

if test "$with_ecw" = "no" ; then

  ECW_SETTING=no

  echo "ECW support disabled."

elif test "$with_ecw" = "yes" -o "$with_ecw" = "" ; then

  AC_CHECK_LIB(NCSEcw,NCScbmOpenFileView,ECW_SETTING=yes,ECW_SETTING=no,-lNCSCnet -lNCSUtil)

  if test "$ECW_SETTING" = "yes" ; then
    ECW_LIBS="-lNCSEcw -lNCSEcwC -lNCSCnet -lNCSUtil"
  fi

  if test "$ECW_SETTING" = "no" ; then
    AC_CHECK_LIB(ecwj2,NCScbmOpenFileView,ECW_SETTING=yes,ECW_SETTING=no,$CARBON_FRAMEWORK)
    if test "$ECW_SETTING" = "yes" ; then
      ECW_LIBS="-lecwj2 $CARBON_FRAMEWORK"
    fi
  fi
else

  AC_MSG_CHECKING([for libNCSEcw.a or libecwj2])
  ECW_ARCH=x86
  ECW_CONF="release"
  ECW_FLAGS="-DLINUX -DX86 -DPOSIX -DHAVE_COMPRESS -DECW_COMPRESS_RW_SDK_VERSION"
  ECW_FRAMEWORK_COCOA=""
  if test "`arch`" = "x86_64" ; then
    ECW_ARCH="x64"
  fi
  if test ! -z "`uname | grep Darwin`" ; then
    ECW_ARCH=""
    ECW_CONF=""
    ECW_FLAGS=""
    ECW_FRAMEWORK_COCOA=" -framework Cocoa "
  fi

  ECW_SETTING=yes
  if test -r $with_ecw/lib/libNCSCnet.so -o -r $with_ecw/lib/libNCSCnet.dylib ; then
    ECW_LIBS="-L$with_ecw/lib -lNCSEcw -lNCSEcwC -lNCSCnet -lNCSUtil"
    AC_MSG_RESULT([found in $with_ecw/lib.])
  elif test -r $with_ecw/lib/libNCSCNet.so -o -r $with_ecw/lib/libNCSCNet.dylib ; then
    ECW_LIBS="-L$with_ecw/lib -lNCSEcw -lNCSEcwC -lNCSCNet -lNCSUtil"
    AC_MSG_RESULT([found in $with_ecw/lib.])
  elif test -r $with_ecw/bin/libNCSEcw.so -o -r $with_ecw/bin/libNCSEcw.dylib ; then
    ECW_LIBS="-L$with_ecw/bin -lNCSEcw -lNCSEcwC -lNCSCnet -lNCSUtil"
    AC_MSG_RESULT([found in $with_ecw/bin.])
  elif test -r $with_ecw/lib/libecwj2.dylib ; then
    ECW_LIBS="-L$with_ecw/lib -lecwj2 $CARBON_FRAMEWORK"
    AC_MSG_RESULT([found libecwj2 in $with_ecw/lib.])

  # ECW SDK 5.0 style and also for the case where license type is included in path i.e. specific license type is requested.
  elif test -r $with_ecw/lib/$ECW_ARCH/$ECW_CONF/libNCSEcw.a ; then
    # Test if we must use the newabi/cpp11abi version (SDK 5.4+)
    if test -r $with_ecw/lib/newabi/$ECW_ARCH/$ECW_CONF/libNCSEcw.a || test -r $with_ecw/lib/cpp11abi/$ECW_ARCH/$ECW_CONF/libNCSEcw.a; then
        if test -d $with_ecw/lib/newabi; then
            ECW_ABIDIR=newabi
        else
            ECW_ABIDIR=cpp11abi
        fi
        echo "#include <string>" > testnewabi.cpp
        echo "namespace NCS { class CString { public: static std::wstring Utf8Decode (const std::string &sUtf8); }; }" >> testnewabi.cpp
        echo "int main() { return static_cast<int>(NCS::CString::Utf8Decode(std::string()).size()); }" >> testnewabi.cpp
        if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} testnewabi.cpp -L$with_ecw/lib/$ECW_ABIDIR/$ECW_ARCH/$ECW_CONF -lNCSEcw -o testnewabi 2>&1`" ; then
            ECW_LIBDIR=$with_ecw/lib/$ECW_ABIDIR/$ECW_ARCH/$ECW_CONF
            ECW_LIBS="-L$ECW_LIBDIR -lNCSEcw $ECW_FRAMEWORK_COCOA"
            with_ecw=$with_ecw/$ecw_license_type
            ECW_54="yes"
            AC_MSG_RESULT([found Intergraph 5.4+ SDK with ${ECW_ABIDIR} in ${ECW_LIBDIR}.])
            rm -f testnewabi.*
            rm -f testnewabi
            break
        fi
        rm -f testnewabi.*
        rm -f testnewabi
    fi
    if test "$ECW_LIBS" = ""; then
        ECW_LIBDIR=$with_ecw/lib/$ECW_ARCH/$ECW_CONF
        ECW_LIBS="-L$ECW_LIBDIR -lNCSEcw $ECW_FRAMEWORK_COCOA"
        AC_MSG_RESULT([found Intergraph 5.x+ SDK in ${ECW_LIBDIR}.])
    fi
 # ECWJP2 SDK 5.1 style
  elif test -d $with_ecw; then
    for ecw_license_type in "Desktop_Read-Write" "Server_Read-Only_EndUser" "Server_Read-Only" "Server_Read-Write" "Desktop_Read-Only"
      do
        # Test if we must use the newabi/cpp11abi version (SDK 5.4+)
        if test -r $with_ecw/$ecw_license_type/lib/newabi/$ECW_ARCH/$ECW_CONF/libNCSEcw.a || test -r $with_ecw/$ecw_license_type/lib/cpp11abi/$ECW_ARCH/$ECW_CONF/libNCSEcw.a; then
            if test -d $with_ecw/lib/newabi; then
                ECW_ABIDIR=newabi
            else
                ECW_ABIDIR=cpp11abi
            fi
            echo "#include <string>" > testnewabi.cpp
            echo "namespace NCS { class CString { public: static std::wstring Utf8Decode (const std::string &sUtf8); }; }" >> testnewabi.cpp
            echo "int main() { return static_cast<int>(NCS::CString::Utf8Decode(std::string()).size()); }" >> testnewabi.cpp
            if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} testnewabi.cpp -L$with_ecw/$ecw_license_type/lib/$ECW_ABIDIR/$ECW_ARCH/$ECW_CONF -lNCSEcw -o testnewabi 2>&1`" ; then
                ECW_LIBDIR=$with_ecw/$ecw_license_type/lib/$ECW_ABIDIR/$ECW_ARCH/$ECW_CONF
                ECW_LIBS="-L$ECW_LIBDIR -lNCSEcw $ECW_FRAMEWORK_COCOA"
                with_ecw=$with_ecw/$ecw_license_type
                AC_MSG_RESULT([found Intergraph 5.4+ SDK with ${ECW_ABIDIR} in ${ECW_LIBDIR}.])
                ECW_54="yes"
                rm -f testnewabi.*
                rm -f testnewabi
                break
            fi
            rm -f testnewabi.*
            rm -f testnewabi
        fi
        if test "$ECW_LIBS" = ""; then
            ECW_LIBDIR=$with_ecw/$ecw_license_type/lib/$ECW_ARCH/$ECW_CONF
            if test -r $ECW_LIBDIR/libNCSEcw.a; then
                ECW_LIBS="-L$ECW_LIBDIR -lNCSEcw $ECW_FRAMEWORK_COCOA"
                with_ecw=$with_ecw/$ecw_license_type
                AC_MSG_RESULT([found Intergraph 5.x+ SDK in ${ECW_LIBDIR}.])
                break
            fi
        fi
      done
 else
    AC_MSG_ERROR([not found in $with_ecw.])
  fi

  AC_MSG_CHECKING([for NCSECWClient.h in $with_ecw/include])
  if test -r $with_ecw/include/NCSECWClient.h ; then
    AC_MSG_RESULT([found.])
    ECW_INCLUDE="-I$with_ecw/include"
  else
    AC_MSG_ERROR([not found.])
  fi
  AC_MSG_CHECKING([for ECWJP2BuildNumber.h in $with_ecw/include])
  if test -r $with_ecw/include/ECWJP2BuildNumber.h ; then
    AC_MSG_RESULT([found.])
    ECW_FLAGS="-DHAVE_ECW_BUILDNUMBER_H $ECW_FLAGS"
  else
    AC_MSG_RESULT([not found.])
  fi
fi

AC_SUBST(ECW_SETTING,$ECW_SETTING)
AC_SUBST(ECW_LIBS,$ECW_LIBS)
AC_SUBST(ECW_FLAGS,$ECW_FLAGS)
AC_SUBST(ECW_INCLUDE,$ECW_INCLUDE)

if test "$ECW_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="ecw $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Select Kakadu library or disable driver.
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([for Kakadu JPEG2000 support])

AC_ARG_WITH(kakadu,[  --with-kakadu[=ARG]     Include Kakadu/JPEG2000 support],,)

if test "$with_kakadu" = "no" -o "$with_kakadu" = "" ; then
  KAKDIR=
  AC_MSG_RESULT([not requested.])
  HAVE_KAKADU=no
elif test "$with_kakadu" = "yes" ; then
  AC_MSG_ERROR([
For JPEG2000 support using Kakadu you need provide the path to the Kakadu
build directory.  Note that Kakadu is *not* free software.])
else
  KAKDIR=$with_kakadu
  OPT_GDAL_FORMATS="jp2kak jpipkak $OPT_GDAL_FORMATS"
  LIBS="$LIBS -L$with_kakadu/lib -lkdu"
  AC_MSG_RESULT([requested.])
  HAVE_KAKADU=yes
fi

AC_SUBST(KAKDIR,$KAKDIR)

dnl ---------------------------------------------------------------------------
dnl Select MrSID library or disable driver.
dnl ---------------------------------------------------------------------------
MRSID_FLAGS=
HAVE_MRSID=no

AC_ARG_WITH(mrsid,[  --with-mrsid[=ARG]      Include MrSID support (ARG=path to MrSID DSDK or no)],,)

AC_ARG_WITH(jp2mrsid,[  --with-jp2mrsid[=ARG]   Enable MrSID JPEG2000 support (ARG=yes/no)],,)

if test "x$with_mrsid" = "xno"  -o "x$with_mrsid" = "x" ; then

  HAVE_MRSID=no

  AC_MSG_NOTICE([MrSID support disabled.])

else

  MRSID_BASE="$with_mrsid/include"

  AC_MSG_CHECKING([for lt_base.h in $MRSID_BASE/support])
  if test -r "$MRSID_BASE/support/lt_base.h" ; then

    AC_MSG_RESULT([found MrSID DSDK version 4.x or newer.])
    HAVE_MRSID=yes
    MRSID_INCLUDE="-I$MRSID_BASE/base -I$MRSID_BASE/metadata -I$MRSID_BASE/mrsid_readers -I$MRSID_BASE/j2k_readers -I$MRSID_BASE/support"

  else

    AC_MSG_RESULT([not found.])

    AC_MSG_CHECKING([for lt_base.h in $MRSID_BASE])
    if test -r "$MRSID_BASE/lt_base.h" ; then

      AC_MSG_RESULT([found MrSID DSDK version 7.x or newer.]);
      HAVE_MRSID=yes
      MRSID_INCLUDE="-I$MRSID_BASE"

    fi

  fi

  if test $HAVE_MRSID = yes ; then

    MRSID_LIBS="-lpthread"

    if test -r "$with_mrsid/lib/libltiesdk.a" ; then # v8+ esdk contains dsdk
      _LIBPART=lib
      MRSID_LIBS="-lltiesdk $MRSID_LIBS"
    elif test -e "$with_mrsid/lib/libltidsdk.a" \
           -o -e "$with_mrsid/lib/libltidsdk.so" \
           -o -e "$with_mrsid/lib/libltidsdk.dylib" ; then
      _LIBPART=lib
      MRSID_LIBS="-lltidsdk $MRSID_LIBS"
    else
      _LIBPART=lib/Release
      MRSID_LIBS="-lltidsdk $MRSID_LIBS"
    fi

    AC_MSG_CHECKING([for MG3ImageWriter.h in $with_mrsid/include/mrsid_writers])
    if test -r "$with_mrsid/include/mrsid_writers/MG3ImageWriter.h" ; then
      AC_MSG_RESULT([found MrSID ESDK version 4.x or newer.])
      MRSID_FLAGS="-DMRSID_ESDK $MRSID_FLAGS"
      MRSID_INCLUDE="-I$with_mrsid/include/mrsid_writers -I$with_mrsid/include/j2k_writers $MRSID_INCLUDE"
      if test -r $with_mrsid/3rd-party/lib/Release/libcryptopp.a ; then
        MRSID_LIBS="-lltiesdk -lcryptopp -lxmlparse $MRSID_LIBS"
      else
        MRSID_LIBS="-lltiesdk -lxmlparse $MRSID_LIBS"
      fi
    else
      AC_MSG_RESULT([no encoding support.])
    fi

    AC_MSG_CHECKING([for MrSID JPEG2000 support])
    if test "x$with_jp2mrsid" = "xyes" -a "$HAVE_KAKADU" = "yes" ; then
      # SDK v8 or later don't seem to conflict with Kakadu
      major_version=`cat $with_mrsid/include/lti_version.h | grep MAJOR | sed 's/#define LTI_SDK_MAJOR[ ]*\(.*\)/\1/'`
      if test "x$major_version" != "x"; then
        if test "$major_version" -ge 8; then
            V8_OR_LATER=yes
        fi
      fi
      if test "x$V8_OR_LATER" = "x"; then
        AC_MSG_ERROR([MrSID JPEG2000 support requested, but this is incompatible with use of standalone Kakadu])
      fi
    fi

    MRSID_KAKADU_LIB=""
    if test "$HAVE_KAKADU" = "no" ; then
      if test x"$with_jp2mrsid" = x"" -o x"$with_jp2mrsid" = x"yes" ; then
        if test -r "$with_mrsid/3rd-party/$_LIBPART/libltikdu.a" ; then
          with_jp2mrsid=yes
          MRSID_KAKADU_LIB=-lltikdu
        elif test -r "$with_mrsid/3rd-party/$_LIBPART/liblt_kakadu.a" ; then
          with_jp2mrsid=yes
          MRSID_KAKADU_LIB=-llt_kakadu
        elif test -e "$with_mrsid/lib/libltidsdk.so" \
               -o -e "$with_mrsid/lib/libltidsdk.dylib"; then # v8+ .so has kdu
          with_jp2mrsid=yes
        elif test x"$with_jp2mrsid" = x"yes" ; then
          AC_MSG_ERROR([MrSID JPEG2000 support requested, but libltikdu.a not found.])
        else
          with_jp2mrsid=no
        fi
      fi
    fi

    if test "x$with_jp2mrsid" = "xyes" ; then
      MRSID_LIBS="$MRSID_LIBS $MRSID_KAKADU_LIB"
      MRSID_FLAGS="-DMRSID_J2K $MRSID_FLAGS"
      AC_MSG_RESULT([enabled])
    else
      AC_MSG_RESULT([disabled])
    fi

    MRSID_LIBS="-L$with_mrsid/$_LIBPART $MRSID_LIBS"
    MRSID_LIBS="-L$with_mrsid/3rd-party/$_LIBPART $MRSID_LIBS"

  else
    HAVE_MRSID=no
    AC_MSG_RESULT([not found.])
    AC_MSG_ERROR([  MrSID requested, but components not found.])
  fi
fi

AC_SUBST(MRSID_INCLUDE,$MRSID_INCLUDE)
AC_SUBST(MRSID_FLAGS,$MRSID_FLAGS)
AC_SUBST(MRSID_LIBS,$MRSID_LIBS)

if test "$HAVE_MRSID" != "no" ; then
  CPPFLAGS="-D_REENTRANT $CPPFLAGS"
  OPT_GDAL_FORMATS="mrsid $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Select MrSID/MG4 Lidar library or disable driver.
dnl ---------------------------------------------------------------------------

MRSID_LIDAR_FLAGS=
HAVE_MRSID_LIDAR=no

AC_ARG_WITH(mrsid_lidar,[  --with-mrsid_lidar[=ARG]      Include MrSID/MG4 LiDAR support (ARG=path to LizardTech LiDAR SDK or no)],,)

if test "x$with_mrsid_lidar" = "xno"  -o "x$with_mrsid_lidar" = "x" ; then
  HAVE_MRSID_LIDAR=no
  AC_MSG_NOTICE([MrSID/MG4 Lidar support disabled.])
else
  MRSID_LIDAR_BASE="$with_mrsid_lidar"

  if test -r "$MRSID_LIDAR_BASE/include/lidar/Version.h" ; then
    AC_MSG_RESULT([found LizardTech LiDAR SDK 1.1 or newer.])
    HAVE_MRSID_LIDAR=yes
  else
    HAVE_MRSID_LIDAR=no
    AC_MSG_RESULT([not found.])
    AC_MSG_ERROR([  MrSID/MG4 Lidar requested, but components not found.])
  fi

  if test $HAVE_MRSID_LIDAR = yes ; then
    if test -e "$MRSID_LIDAR_BASE/lib/liblti_lidar_dsdk.so" \
         -o -e "$MRSID_LIDAR_BASE/lib/liblti_lidar_dsdk.dylib"; then
      MRSID_LIDAR_LIBS="-L$MRSID_LIDAR_BASE/lib -llti_lidar_dsdk"
      MRSID_LIDAR_INCLUDE="-I$MRSID_LIDAR_BASE/include"
    else
      MRSID_LIDAR_LIBS="-L$MRSID_LIDAR_BASE/lib/Release -llti_lidar_dsdk"
      MRSID_LIDAR_INCLUDE="-I$MRSID_LIDAR_BASE/include"
    fi
  fi
fi

AC_SUBST(MRSID_LIDAR_INCLUDE,$MRSID_LIDAR_INCLUDE)
AC_SUBST(MRSID_LIDAR_LIBS,$MRSID_LIDAR_LIBS)

if test "$HAVE_MRSID_LIDAR" != "no" ; then
  OPT_GDAL_FORMATS="mrsid_lidar $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Check if LuraTech library is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(jp2lura,[  --with-j2lura[=ARG]    Include JP2Lua support (ARG=no, lura SDK install path)],,)

AC_MSG_CHECKING([for JP2Lura support])

HAVE_JP2LURA=no
JP2LURA_INCLUDE=

if test "$with_jp2lura" != "" -a "$with_jp2lura" != "no" ; then
  AC_MSG_CHECKING([for lwf_jp2.h])
  if test -f "$with_jp2lura/include/lwf_jp2.h"; then
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([JP2Lura support requested but header not found])
  fi
  JP2LURA_INCLUDE="-I$with_jp2lura/include"

  # mingw support
  if test -f "$with_jp2lura/library/lwf_jp2.lib"; then
      ln -s "$with_jp2lura/library/lwf_jp2.lib" "$with_jp2lura/library/lib_lwf_jp2.a"
  fi

  ORIG_LIBS="$LIBS"
  LIBS="-L$with_jp2lura/library"

  AC_CHECK_LIB(_lwf_jp2,JP2_Decompress_SetLicense,HAVE_JP2LURA="yes",,-lm)

  if test "$HAVE_JP2LURA" = "no" ; then
    AC_MSG_ERROR([JP2Lura support requested but library not found])
  fi

  LIBS="-L$with_jp2lura/library -l_lwf_jp2 -lm $ORIG_LIBS"
fi

AC_SUBST([JP2LURA_INCLUDE], [$JP2LURA_INCLUDE])

if test "$HAVE_JP2LURA" != "no" ; then
  OPT_GDAL_FORMATS="jp2lura $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Enable MSG format if EUMETSAT Wavelet Transform Software
dnl available in the local tree
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([msg],
	    AS_HELP_STRING([--with-msg[=ARG]],
	       [Enable MSG driver (ARG=yes or no)]),,)

HAVE_MSG=no
HAVE_EUMETSATWT=no

if test "x$with_msg" = "xyes" ; then
    AC_MSG_CHECKING([for EUMETSAT Wavelet Transformation Software])

    if test -d "frmts/msg/PublicDecompWT" -a \
        -f "frmts/msg/PublicDecompWT/COMP/Inc/CImage.h"; then
        HAVE_EUMETSATWT=yes
    else
        HAVE_EUMETSATWT=no
    fi
    AC_MSG_RESULT([$HAVE_EUMETSATWT])
fi

AC_MSG_CHECKING([for MSG])

if test "x$with_msg" = "xyes" -o "x$with_msg" = "x" \
    -a "x$HAVE_EUMETSATWT" = "xyes"; then
    OPT_GDAL_FORMATS="msg $OPT_GDAL_FORMATS"
    HAVE_MSG=yes
    AC_MSG_RESULT([enabled])
else
    HAVE_MSG=no
    AC_MSG_RESULT([not requested])
fi


dnl ---------------------------------------------------------------------------
dnl Check for GeoRaster in the local tree.
dnl ---------------------------------------------------------------------------

ORACLE_OCI_REQ_VERSION="10.0.1"
AX_LIB_ORACLE_OCI($ORACLE_OCI_REQ_VERSION)

if test "$HAVE_ORACLE_OCI" = "yes"; then
    OPT_GDAL_FORMATS="georaster $OPT_GDAL_FORMATS"
    HAVE_GEORASTER=yes
else
    HAVE_GEORASTER=no
fi


dnl ---------------------------------------------------------------------------

AC_SUBST([OPT_GDAL_FORMATS], [$OPT_GDAL_FORMATS])

dnl ---------------------------------------------------------------------------

GNM_ENABLED=yes

AC_ARG_WITH(gnm,[  --with-gnm            Build GNM into shared library],,)

if test "$with_gnm" = "no" ; then
  GNM_ENABLED=no
  AC_MSG_RESULT([disabled by user])
else
  GNM_ENABLED=yes
  AC_MSG_RESULT([enabled])
fi
AC_SUBST(GNM_ENABLED,$GNM_ENABLED)

dnl ---------------------------------------------------------------------------
dnl Select an MySQL Library to use, or disable driver.
dnl ---------------------------------------------------------------------------

MYSQL_CONFIG=no

AC_ARG_WITH(mysql,[  --with-mysql[=ARG]      Include MySQL (ARG=path to mysql_config) [[default=no]]],,)

if test "$with_mysql" = "yes" ; then
  AC_PATH_PROG(MYSQL_CONFIG, mysql_config, no)
else
    if test "x$with_mysql" != "x" ; then
      MYSQL_CONFIG=$with_mysql
    fi
fi

AC_MSG_CHECKING([for MySQL])

if test "$MYSQL_CONFIG" = "no" ; then

  HAVE_MYSQL=no
  MYSQL_LIB=
  MYSQL_INC=

  AC_MSG_RESULT([no])

else
  if test -d $MYSQL_CONFIG ; then
      AC_MSG_RESULT([no])
      AC_MSG_ERROR([--with-mysql argument is a directory.  It should be the path to the mysql_config script, often somewhere like /usr/local/bin/mysql_config.])
  fi

  if test \! -x $MYSQL_CONFIG ; then
      AC_MSG_RESULT([no])
      AC_MSG_ERROR([--with-mysql argument is a not an executable file.  It should be the path to the mysql_config script, often somewhere like /usr/local/bin/mysql_config.])
  fi

  MYSQL_VERSION="`$MYSQL_CONFIG --version`"
  MYSQL_MAJOR_VERSION=${MYSQL_VERSION%%'.'*}

  if test $MYSQL_MAJOR_VERSION -le 3 ; then
    HAVE_MYSQL=no
    AC_MSG_RESULT([no, mysql is pre-4.x])
  else
      HAVE_MYSQL=yes
      MYSQL_LIB="`$MYSQL_CONFIG --libs`"
      STRIP_SYSTEM_LIBRARY_PATHS("${MYSQL_LIB}")
      MYSQL_LIB="$STRIPPED_LIBRARY_NAME"
      MYSQL_INC="`$MYSQL_CONFIG --include`"
      AC_MSG_RESULT([yes])
  fi
fi

AC_SUBST(HAVE_MYSQL,$HAVE_MYSQL)
AC_SUBST(MYSQL_INC,$MYSQL_INC)
AC_SUBST(MYSQL_LIB,$MYSQL_LIB)

dnl ---------------------------------------------------------------------------
dnl INGRES support.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(ingres,[  --with-ingres=ARG     Include Ingres (ARG=$II_SYSTEM)],,)

AC_MSG_CHECKING([for Ingres])

if test "$with_ingres" = "no" -o "x$with_ingres" = "x" ; then
  HAVE_INGRES=no
  II_SYSTEM=
  INGRES_LIB=
  INGRES_INC=
  AC_MSG_RESULT([no])
elif test "$with_ingres" = "yes" ; then
  AC_MSG_ERROR([--with-ingres requires a path argument ($II_SYSTEM)])
else
  HAVE_INGRES=yes
  II_SYSTEM=$with_ingres
  INGRES_LIB="-L$II_SYSTEM/ingres/lib \
	-liiapi.1 -lcompat.1 -lq.1 -lframe.1"
  INGRES_INC=-I$II_SYSTEM/ingres/files
fi

AC_SUBST(HAVE_INGRES,$HAVE_INGRES)
AC_SUBST(II_SYSTEM,$II_SYSTEM)
AC_SUBST(INGRES_LIB,$INGRES_LIB)
AC_SUBST(INGRES_INC,$INGRES_INC)

dnl ---------------------------------------------------------------------------
dnl Check for Xerces C++ Parser support.
dnl ---------------------------------------------------------------------------

XERCES_REQ_VERSION="3.1.0"
AX_LIB_XERCES($XERCES_REQ_VERSION)

if test "$HAVE_XERCES" = "yes"; then
    LIBS="$XERCES_LDFLAGS $LIBS"

m4_foreach_w([frmt],XERCES_DRIVERS,[
  driver_enabled=m4_join([_],[$INTERNAL_FORMAT],frmt,[ENABLED])
  if test "x$driver_enabled" = "xyes"; then
    OGRFORMATS_ENABLED="$OGRFORMATS_ENABLED frmt"
    OGRFORMATS_ENABLED_CFLAGS="$OGRFORMATS_ENABLED_CFLAGS _OGRDEFINE(frmt)"
  fi
])

fi

AC_SUBST([HAVE_XERCES], $HAVE_XERCES)
AC_SUBST([XERCES_INCLUDE], $XERCES_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl Check for Expat configuration.
dnl ---------------------------------------------------------------------------

dnl Expat 1.95.0 released in 2000-09-28
EXPAT_REQ_VERSION="1.95.0"
AX_LIB_EXPAT($EXPAT_REQ_VERSION)

if test "$HAVE_EXPAT" = "yes"; then
    LIBS="$EXPAT_LDFLAGS $LIBS"

m4_foreach_w([frmt],EXPAT_DRIVERS,[
  driver_enabled=m4_join([_],[$INTERNAL_FORMAT],frmt,[ENABLED])
  if test "x$driver_enabled" = "xyes"; then
    OGRFORMATS_ENABLED="$OGRFORMATS_ENABLED frmt"
    OGRFORMATS_ENABLED_CFLAGS="$OGRFORMATS_ENABLED_CFLAGS _OGRDEFINE(frmt)"
  fi
])

fi

AC_SUBST([HAVE_EXPAT], $HAVE_EXPAT)
AC_SUBST([EXPAT_INCLUDE], $EXPAT_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl Check for Google libkml support.
dnl ---------------------------------------------------------------------------

LIBKML_REQ_VERSION="1.3.0"
AX_LIB_LIBKML($LIBKML_REQ_VERSION)

if test "$HAVE_LIBKML" = "yes"; then
    LIBS="$LIBKML_LDFLAGS $LIBS"
fi

AC_SUBST([HAVE_LIBKML], $HAVE_LIBKML)
AC_SUBST([LIBKML_INCLUDE], $LIBKML_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl Check for ODBC support.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(odbc,[  --with-odbc[=ARG]       Include ODBC support (ARG=no or path)],,)

if test "$with_odbc" = "no" ; then

  ODBC_SETTING=no

  echo "ODBC support disabled."

elif test "$with_odbc" = "yes" -o "$with_odbc" = "" ; then
  ODBC_SETTING=no

  echo "check for unixODBC"
  AC_CHECK_LIB(odbc,SQLConnect,[ODBC_LIBS="-lodbc -lodbcinst"])
  AC_CHECK_LIB(odbcinst,SQLInstallDriverEx,ODBC_SETTING=yes)

  if test "$ODBC_SETTING" = "no" ; then
    echo "check for windows ODBC"
    AC_CHECK_LIB(odbc32,main,[ODBC_LIBS="-lodbc32 -lodbccp32"])
    AC_CHECK_LIB(odbccp32,SQLInstallDriverEx,ODBC_SETTING=yes)
  fi

  if test "$ODBC_SETTING" = "yes" ; then
    AC_CHECK_HEADERS(sql.h,,[ODBC_SETTING=no],
[#ifdef _WIN32
# include <windows.h>
#endif
])
    if test "$ODBC_SETTING" = "no" ; then
      if test -f /usr/local/include/sql.h ; then
        ODBC_SETTING=yes
        echo "using sql.h from /usr/local/include"
      elif test -f /usr/include/sql.h ; then
        ODBC_SETTING=yes
        echo "using sql.h from /usr/include"
      else
        echo "sql.h not found"
      fi
    fi
  fi

  if test "$ODBC_SETTING" = "yes"  ; then
    LIBS="$ODBC_LIBS $LIBS"
  fi

else

  ODBC_SETTING=yes
  ODBC_LIBS=""
  AC_CHECK_LIB(odbc,SQLConnect,ODBC_LIBS="-lodbc -lodbcinst")
  AC_CHECK_LIB(odbc32,main,ODBC_LIBS="-lodbc32 -lodbccp32")
  if test "$ODBC_LIBS" = ""; then
    AC_MSG_ERROR([Cannot find ODBC libs])
  fi
  LIBS="-L$with_odbc -L$with_odbc/lib $ODBC_LIBS $LIBS"
  EXTRA_INCLUDES="-I$with_odbc -I$with_odbc/include $EXTRA_INCLUDES"

  echo "using odbc library from $with_odbc."
fi

if test "$ODBC_SETTING" = "yes"  ; then
m4_foreach_w([frmt],ODBC_DRIVERS,[
  driver_enabled=m4_join([_],[$INTERNAL_FORMAT],frmt,[ENABLED])
  if test "x$driver_enabled" = "xyes"; then
    OGRFORMATS_ENABLED="$OGRFORMATS_ENABLED frmt"
    OGRFORMATS_ENABLED_CFLAGS="$OGRFORMATS_ENABLED_CFLAGS _OGRDEFINE(frmt)"
  fi
])
fi

AC_SUBST(ODBC_SETTING,$ODBC_SETTING)

dnl ---------------------------------------------------------------------------
dnl Check for OCI support.
dnl ---------------------------------------------------------------------------

ORACLE_OCI_REQ_VERSION="8.1.7"
AX_LIB_ORACLE_OCI($ORACLE_OCI_REQ_VERSION)

if test "$HAVE_ORACLE_OCI" = "yes"; then
    OCI_INCLUDE="$ORACLE_OCI_CFLAGS"
    LIBS="$ORACLE_OCI_LDFLAGS $LIBS"
fi

AC_SUBST([HAVE_OCI], [$HAVE_ORACLE_OCI])
AC_SUBST([OCI_INCLUDE], $[OCI_INCLUDE])

dnl ---------------------------------------------------------------------------
dnl Check for DODS.
dnl ---------------------------------------------------------------------------

HAVE_DODS=no

AC_MSG_CHECKING(for DODS)
AC_ARG_WITH(dods_root,
    [  --with-dods-root[=ARG]  Include DODS support (ARG=no or absolute path)],
    ,,)

if test -z "$with_dods_root" -o "$with_dods_root" = "no"; then
    AC_MSG_RESULT(disabled)
elif test "$with_dods_root" = "yes" ; then
    AC_MSG_ERROR([--with-dods-root requires path to dods root as argument])
else
    DODS_LIB=$with_dods_root/lib
    DODS_INC="-I$with_dods_root/include"
    if test -d "$with_dods_root/include/libdap"; then
       DODS_INC="$DODS_INC -I$with_dods_root/include/libdap";
    elif test -d "$with_dods_root/include/dap"; then
       DODS_INC="$DODS_INC -I$with_dods_root/include/dap";
   fi
    DODS_BIN=$with_dods_root/bin

    dnl Check libdap version the sane way (if possible through dap-config)
    if test -x $DODS_BIN/dap-config ; then
        LIBDAP_VER=`${DODS_BIN}/dap-config --version |awk '{print \$2}'`
        LIBDAP_CFLAGS=`${DODS_BIN}/dap-config --cflags`
        dnl The dods driver needs LIBDAP_320 and LIBDAP_39 defined to include the right headers
        DODS_INC="$DODS_INC $LIBDAP_CFLAGS -DLIBDAP_310 -DLIBDAP_39"
        dnl libs from dap-config gets added further down
        AC_MSG_RESULT([libdap $LIBDAP_VER])
    else
        PKG_PROG_PKG_CONFIG([0.21])
        PKG_CHECK_MODULES(LIBDAP,[libdap], [HAVE_LIBDAP=yes], [HAVE_LIBDAP=no])

        if test "${HAVE_LIBDAP}" = "yes"; then
            dnl The dods driver needs LIBDAP_320 and LIBDAP_39 defined to include the right headers
            DODS_INC="$DODS_INC $LIBDAP_CFLAGS -DLIBDAP_310 -DLIBDAP_39"
        else
            dnl Check the version by compiling test programs

            dnl Test if we have libdap >= 3.10
            dnl From libdap 3.10, AISConnect.h has been renamed as Connect.h
            rm -f islibdappost310.*
            echo '#include "Connect.h"' > islibdappost310.cpp
            echo 'int main(int argc, char** argv) { return 0; } ' >> islibdappost310.cpp
            if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} islibdappost310.cpp -c ${DODS_INC} ${CPPFLAGS} 2>&1`" ; then
            dnl yes, libdap >= 3.10
                DODS_INC="$DODS_INC -DLIBDAP_310 -DLIBDAP_39"
                AC_MSG_RESULT([libdap >= 3.10])
            else
                AC_MSG_RESULT([libdap < 3.10])

                dnl Test if we have libdap < 3.9
                dnl Before libdap < 3.9, DAS derived from AttrTable class
                dnl I wish they had defines with version numbers instead of this test !
                rm -f islibdappre39.*
                echo '#include "DAS.h"' > islibdappre39.cpp
                echo '#include "AttrTable.h"' >> islibdappre39.cpp
                echo 'using namespace libdap;' >> islibdappre39.cpp
                echo 'int main(int argc, char** argv) { DAS oDAS; AttrTable& oAttrTable = oDAS; } ' >> islibdappre39.cpp
                if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} islibdappre39.cpp -c ${DODS_INC} 2>&1`" ; then
                dnl yes, libdap < 3.9
                    AC_MSG_RESULT([libdap < 3.9])
                else
                    DODS_INC="$DODS_INC -DLIBDAP_39"
                    AC_MSG_RESULT([libdap >= 3.9])
                fi
                rm -f islibdappre39.*

            fi
            rm -f islibdappost310.*
        fi
    fi


    dnl Add the DODS libraries to LIBS
    if test -x $DODS_BIN/opendap-config ; then
      dnl OPeNDAP 3.4 and earlier lack opendap-config, but use it if avail.
      LIBS="$LIBS `$DODS_BIN/opendap-config --libs`"
    elif test -x $DODS_BIN/dap-config ; then
      dnl OPeNDAP 3.4 and earlier lack opendap-config, but use it if avail.
      LIBS="$LIBS `$DODS_BIN/dap-config --libs`"
    elif test "${HAVE_LIBDAP}" = "yes"; then
      LIBS="$LIBS $LIBDAP_LIBS"
    else
      dnl Otherwise try to put things together in a more primitive way.
      LIBS="$LIBS -L$DODS_LIB -ldap++ -lpthread -lrx"

      dnl Add curl to LIBS; it might be local to DODS or generally installed
      if test -x $DODS_BIN/curl-config; then
          LIBS="$LIBS  `$DODS_BIN/curl-config --libs`"
      elif which curl-config > /dev/null 2>&1; then
          LIBS="$LIBS  `curl-config --libs`"
      else
          AC_MSG_ERROR([You gave a dods root, but I can't find curl!])
      fi

      if test -x $DODS_BIN/xml2-config; then
          LIBS="$LIBS `$DODS_BIN/xml2-config --libs`"
      elif which xml2-config > /dev/null 2>&1; then
          LIBS="$LIBS  `xml2-config --libs`"
      else
          AC_MSG_ERROR([You gave a dods root, but I can't find xml2!])
      fi
    fi

    OPT_GDAL_FORMATS="dods $OPT_GDAL_FORMATS"
    DODSDEF=-DFRMT_dods
    AC_MSG_RESULT(setting DODS root directory to $with_dods_root)
    HAVE_DODS=yes
fi

dnl This is used by the GNUmakefile in frmts/dods (via GDALmake.opt)
AC_SUBST(DODS_INC)

dnl ---------------------------------------------------------------------------
dnl Check for libxml2.
dnl ---------------------------------------------------------------------------

HAVE_LIBXML2=no
LIBXML2_INC=
LIBXML2_LIB=

AC_ARG_WITH(xml2,
    [  --with-xml2[=ARG]       Include libxml2 (ARG=yes/no)],,,)

if test "x$with_xml2" = "xyes" -o "x$with_xml2" = "x" ; then

  PKG_PROG_PKG_CONFIG([0.21])
  PKG_CHECK_MODULES(LIBXML2,[libxml-2.0], [HAVE_LIBXML2=yes], [HAVE_LIBXML2=no])

  if test "${HAVE_LIBXML2}" = "yes"; then
    SAVED_LIBS="${LIBS}"
    LIBS="${LIBXML2_LIBS}"
    AC_CHECK_LIB(xml2,xmlParseDoc,HAVE_LIBXML2=yes,HAVE_LIBXML2=no)
    LIBS="${SAVED_LIBS}"
  fi

  if test "${HAVE_LIBXML2}" = "yes"; then
    LIBXML2_INC="${LIBXML2_CFLAGS}"
    LIBXML2_LIB="${LIBXML2_LIBS}"
  else
    if test "x$with_xml2" = "xyes"; then
      AC_MSG_ERROR([--with-xml2 was requested, but libxml2 is not available])
    fi
  fi
elif test "x$with_xml2" != "xno"; then
  AC_MSG_ERROR([Only --with-xml2=yes/no supported])
fi

AC_SUBST(HAVE_LIBXML2,$HAVE_LIBXML2)
AC_SUBST(LIBXML2_INC, $LIBXML2_INC)
AC_SUBST(LIBXML2_LIB, $LIBXML2_LIB)

dnl ---------------------------------------------------------------------------
dnl Check for librasterlite2.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(rasterlite2,
    [  --with-rasterlite2=ARG Include RasterLite2 support (ARG=no(default), yes or path)],
    ,,)

HAVE_RASTERLITE2=no
RASTERLITE2_CFLAGS=

if test -z "$with_rasterlite2" -o "$with_rasterlite2" = "no"; then
    AC_MSG_NOTICE([rasterlite2 support disabled])
elif test "$with_rasterlite2" = "yes"; then
    AC_CHECK_HEADERS(rasterlite2/rasterlite2.h)
    if test "$ac_cv_header_rasterlite2_rasterlite2_h" = "yes"; then
        AC_CHECK_LIB(rasterlite2,rl2_load_raw_tiles_into_dbms,HAVE_RASTERLITE2=yes,HAVE_RASTERLITE2=no,)
        if test "$HAVE_RASTERLITE2" = "yes"; then
            LIBS="$LIBS -lrasterlite2 -lsqlite3"
        else
            AC_MSG_ERROR([did not find rl2_load_raw_tiles_into_dbms])
        fi
    else
        AC_MSG_ERROR([did not find rasterlite2.h])
    fi
else
    AC_MSG_CHECKING([for include/rasterlite2/rasterlite2.h])
    if test -f "$with_rasterlite2/include/rasterlite2/rasterlite2.h"; then
        AC_MSG_RESULT([found])
        AC_CHECK_LIB(rasterlite2,rl2_load_raw_tiles_into_dbms,HAVE_RASTERLITE2=yes,HAVE_RASTERLITE2=no,-L$with_rasterlite2/lib)
        if test "$HAVE_RASTERLITE2" = "yes"; then
            RASTERLITE2_CFLAGS="-I$with_rasterlite2/include"
            LIBS="$LIBS -L$with_rasterlite2/lib -lrasterlite2 -lsqlite3"
        else
            AC_MSG_ERROR([did not find rl2_load_raw_tiles_into_dbms])
        fi
    else
        AC_MSG_ERROR([did not find rasterlite2.h])
    fi
fi

AC_SUBST([HAVE_RASTERLITE2], $HAVE_RASTERLITE2)
AC_SUBST([RASTERLITE2_CFLAGS], $RASTERLITE2_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl Check for PCRE2.
dnl ---------------------------------------------------------------------------

HAVE_PCRE2=no

AC_ARG_WITH(pcre2,
    [  --with-pcre2             Include libpcre2 support (REGEXP support for SQLite)],
    ,,)

if test "$with_pcre2" = "no"; then
    AC_MSG_RESULT(disabled)
    HAVE_PCRE2=no
else
    save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -DPCRE2_CODE_UNIT_WIDTH=8"
    AC_CHECK_HEADERS([pcre2.h])
    CPPFLAGS="$save_CPPFLAGS"
    if test "$ac_cv_header_pcre2_h" = "no" ; then
        if test "$with_pcre2" = "yes"; then
            AC_MSG_ERROR([cannot find pcre2.h])
        fi
    else
        AC_CHECK_LIB(pcre2-8,pcre2_compile_8,HAVE_PCRE2=yes,HAVE_PCRE2=no,)
        if test "$HAVE_PCRE2" = "yes" ; then
            HAVE_PCRE2=yes
            LIBS="$LIBS -lpcre2-8"
        else
            if test "$with_pcre2" = "yes"; then
                AC_MSG_ERROR([cannot find libpcre2])
            fi
        fi
    fi
fi

AC_SUBST(HAVE_PCRE2)

dnl ---------------------------------------------------------------------------
dnl Check for PCRE.
dnl ---------------------------------------------------------------------------

HAVE_PCRE=no

AC_ARG_WITH(pcre,
    [  --with-pcre             Include libpcre support (REGEXP support for SQLite)],
    ,,)

if test "$with_pcre" = "no"; then
    AC_MSG_RESULT(disabled)
    HAVE_PCRE=no
elif test "$HAVE_PCRE2" = "no"; then
    AC_CHECK_HEADERS([pcre.h])
    if test "$ac_cv_header_pcre_h" = "no" ; then
        if test "$with_pcre" = "yes"; then
            AC_MSG_ERROR([cannot find pcre.h])
        fi
    else
        AC_CHECK_LIB(pcre,pcre_compile,HAVE_PCRE=yes,HAVE_PCRE=no,)
        if test "$HAVE_PCRE" = "yes" ; then
            HAVE_PCRE=yes
            LIBS="$LIBS -lpcre"
        else
            if test "$with_pcre" = "yes"; then
                AC_MSG_ERROR([cannot find libpcre])
            fi
        fi
    fi
fi

AC_SUBST(HAVE_PCRE)

dnl ---------------------------------------------------------------------------
dnl Check for Teigha.
dnl ---------------------------------------------------------------------------

TEIGHA_DIR=
TEIGHA_CPPFLAGS=

AC_MSG_CHECKING(Checking for Teigha)
AC_ARG_WITH(teigha,
    [  --with-teigha[=path] Include Teigha DWG/DGN support],
    ,,)

if test -z "$with_teigha" -o "$with_teigha" = "no"; then
    AC_MSG_RESULT(disabled)
    HAVE_TEIGHA=no
else
    AC_MSG_RESULT(enabled)

    AC_ARG_WITH(teigha-plt,
      [  --with-teigha-plt[=platform] Teigha platform],
      ,,)
    if test "x$with_teigha_plt" != "x" ; then
        TEIGHA_DIR=$with_teigha
        HAVE_TEIGHA=yes
        TEIGHA_PLT=$with_teigha_plt
        LIBS="${LIBS} ${TEIGHA_DIR}/bin/${TEIGHA_PLT}/TG_Db.tx"
        if test -f "${TEIGHA_DIR}/bin/${TEIGHA_PLT}/TD_DbEntities.tx"; then
            LIBS="${LIBS} ${TEIGHA_DIR}/bin/${TEIGHA_PLT}/TD_DbEntities.tx"
            LIBS="${LIBS} ${TEIGHA_DIR}/lib/${TEIGHA_PLT}/libTD_DrawingsExamplesCommon.a"
        fi
        LIBS="${LIBS} ${TEIGHA_DIR}/lib/${TEIGHA_PLT}/libTG_ExamplesCommon.a"
        LIBS="${LIBS} ${TEIGHA_DIR}/lib/${TEIGHA_PLT}/libTD_ExamplesCommon.a"
        LIBS="${LIBS} ${TEIGHA_DIR}/lib/${TEIGHA_PLT}/libTD_Key.a"
        LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_Db"
        if test -f "${TEIGHA_DIR}/bin/${TEIGHA_PLT}/libTD_DbCore.*"; then
            LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_DbCore"
        fi
        LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_DbRoot"
        LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_Gi"
        LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_SpatialIndex"
        LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_Root"
        LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_Ge"
        if test -f "${TEIGHA_DIR}/bin/${TEIGHA_PLT}/libTD_Zlib.*"; then
            LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_Zlib"
        fi
        LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -lTD_Alloc"
        LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -loless"
        if test -f "${TEIGHA_DIR}/bin/${TEIGHA_PLT}/liblibcrypto.*"; then
            LIBS="${LIBS} -L${TEIGHA_DIR}/bin/${TEIGHA_PLT} -llibcrypto"
        fi
        if test -f "${TEIGHA_DIR}/bin/${TEIGHA_PLT}/libTD_Db.so"; then
            TEIGHA_CPPFLAGS="-D_TOOLKIT_IN_DLL_"
        fi
    else
        AC_MSG_ERROR([--with-teigha-plt not specified])
    fi
fi

AC_SUBST(TEIGHA_DIR)
AC_SUBST(HAVE_TEIGHA)
AC_SUBST(TEIGHA_CPPFLAGS)

dnl ---------------------------------------------------------------------------
dnl Select Informix DataBlade support
dnl ---------------------------------------------------------------------------

HAVE_IDB=no

AC_ARG_WITH(idb,[  --with-idb=DIR        Include Informix DataBlade support (DIR points to Informix root)],,)

if test x"${with_idb}" = x"no" ; then
  AC_MSG_NOTICE(["IBM Informix DataBlade support disabled."])
else
  if test x"${with_idb}" = x ; then
    with_idb=$INFORMIXDIR
  fi

  if test -e "${with_idb}/incl/c++/it.h" ; then

    LIBS_DIRS="-L${with_idb}/lib/ -L${with_idb}/lib/esql"
    LIBS_ESQL="-lifsql -lifasf -lifgen -lifos -lifgls -lifglx ${with_idb}/lib/esql/checkapi.o"
    LIBS_LIBMI="-L${with_idb}/lib/dmi -lifdmi"
    LIBS_CPPIF="-L${with_idb}/lib/c++ -lifc++"

    IDB_INC="-I${with_idb}/incl/ -I${with_idb}/incl/dmi -I${with_idb}/incl/c++"
    IDB_LIB="${LIBS_CPPIF} ${LIBS_LIBMI} ${LIBS_DIRS} ${LIBS_ESQL}"

    ax_save_LIBS="${LIBS}"
    LIBS="${IDB_LIB} -ldl -lcrypt"
    AC_CHECK_LIB(ifsql,ifx_srvinfo,HAVE_IDB=yes,HAVE_IDB=no,)
    LIBS="${ax_save_LIBS}"

  fi

  if test "${HAVE_IDB}" = "yes" ; then
    AC_MSG_NOTICE([using Informix C++ client library from $with_idb.])
  else
    AC_MSG_NOTICE([IBM Informix DataBlade not supported.])
  fi

fi

AC_SUBST(HAVE_IDB,${HAVE_IDB})
AC_SUBST(IDB_INC,${IDB_INC})
AC_SUBST(IDB_LIB,${IDB_LIB})

dnl ---------------------------------------------------------------------------
dnl Check if webp library is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(webp,[  --with-webp[=ARG]    Include WEBP support (ARG=no, yes or libwebp install root path)],,)

WEBP_SETTING=no

if test "$with_webp" = "yes" -o "$with_webp" = "" ; then

  AC_CHECK_LIB(webp,WebPDecodeRGB,WEBP_SETTING=yes,WEBP_SETTING=no,)

  if test "$WEBP_SETTING" = "yes" ; then
    LIBS="-lwebp $LIBS"
  else
    echo "libwebp not found - WEBP support disabled"
  fi

elif test "$with_webp" != "no" -a "$with_webp" != ""; then

  AC_CHECK_LIB(webp,WebPDecodeRGB,WEBP_SETTING=yes,WEBP_SETTING=no,-L$with_webp/lib -lwebp)

  if test "$WEBP_SETTING" = "yes" ; then
    LIBS="-L$with_webp/lib -lwebp $LIBS"
    EXTRA_INCLUDES="-I$with_webp/include $EXTRA_INCLUDES"
  else
    echo "libwebp not found - WEBP support disabled"
  fi

fi

AC_SUBST(WEBP_SETTING,$WEBP_SETTING)

if test "$WEBP_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="webp $OPT_GDAL_FORMATS"
fi

dnl ---------------------------------------------------------------------------
dnl Check if geos library is available.
dnl ---------------------------------------------------------------------------

GEOS_INIT(3.1.0)
if test "${HAVE_GEOS}" = "yes" ; then
  AC_MSG_NOTICE([Using C API from GEOS $GEOS_VERSION])
  STRIP_SYSTEM_LIBRARY_PATHS("${GEOS_LIBS}")
  GEOS_LIBS="$STRIPPED_LIBRARY_NAME"
  LIBS="${GEOS_LIBS} ${LIBS}"
fi

dnl ---------------------------------------------------------------------------
dnl Check if SFCGAL library is available.
dnl ---------------------------------------------------------------------------

SFCGAL_INIT(1.2.2)
if test "${HAVE_SFCGAL}" = "yes" ; then
  AC_MSG_NOTICE([Using C API from SFCGAL $SFCGAL_VERSION])
  STRIP_SYSTEM_LIBRARY_PATHS("${SFCGAL_LIBS}")
  SFCGAL_LIBS="$STRIPPED_LIBRARY_NAME"
  LIBS="${SFCGAL_LIBS} ${LIBS}"
fi

dnl ---------------------------------------------------------------------------
dnl Check if QHull library is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(qhull,[  --with-qhull[=ARG]     Include QHull support (ARG=no, yes, internal)],,)

QHULL_SETTING=no

if test "$with_qhull" = "no" ; then

  AC_MSG_NOTICE([QHull support disabled.])

elif test "$with_qhull" = "yes" -o "$with_qhull" = "" ; then

  AC_ARG_WITH(qhull_pkgname,[  --with-qhull-pkgname[=ARG]     pkg-config name of the reentrant qhull library, typically qhull_r (default) or qhullstatic_r],,)

  if test "x$with_qhull_pkgname" = "x" ; then
    with_qhull_pkgname=qhull_r
  fi

  PKG_PROG_PKG_CONFIG([0.21])
  PKG_CHECK_MODULES([QHULL],[$with_qhull_pkgname], [HAVE_QHULL=yes], [HAVE_QHULL=no])

  dnl First try with pkg-config
  if test "$HAVE_QHULL" = "yes" ; then
    QHULL_SETTING=external
    CFLAGS="$CFLAGS $QHULL_CFLAGS"
    LIBS="$LIBS $QHULL_LIBS"
  else
    AC_CHECK_HEADERS([libqhull_r/libqhull_r.h])
    if test "$ac_cv_header_libqhull_r_libqhull_r_h" = "yes"; then
      AC_CHECK_LIB(qhull_r,qh_new_qhull,QHULL_SETTING=yes,QHULL_SETTING=no,)
      if test "$QHULL_SETTING" = "yes"; then
        QHULL_SETTING=external
        LIBS="-lqhull_r $LIBS"
      fi
    fi

    if test "$QHULL_SETTING" = "no" ; then
      if test "$with_qhull" = "yes"; then
        AC_MSG_ERROR([--with-qhull requested, but library not found!])
      else
        QHULL_SETTING=internal
      fi
    fi
  fi
else
  QHULL_SETTING=internal
fi

AC_SUBST([QHULL_SETTING],$QHULL_SETTING)

dnl ---------------------------------------------------------------------------
dnl Check if opencl library is available.
dnl ---------------------------------------------------------------------------

OPENCL_SETTING=no
OPENCL_FLAGS=
OPENCL_LIB=

AC_ARG_WITH(opencl,
    [  --with-opencl[=ARG]       Include OpenCL (GPU) support],,,)

AC_MSG_CHECKING([for OpenCL support])

if test "$with_opencl" = "yes" ; then

  AC_ARG_WITH(opencl-include,
      [  --with-opencl-include=ARG OpenCL Include directory (with a CL subdirectory)],,,)

  OPENCL_SETTING=yes

  if test "x$with_opencl_include" = "x" ; then
    OPENCL_FLAGS=-DHAVE_OPENCL
    if test -z "`uname | grep Darwin`" ; then
        AC_CHECK_HEADERS([CL/opencl.h])
        if test "$ac_cv_header_CL_opencl_h" = "no" ; then
            AC_MSG_ERROR([cannot find CL/opencl.h])
        fi
    fi
  else
    OPENCL_FLAGS="-I$with_opencl_include -DHAVE_OPENCL"
  fi

  AC_ARG_WITH(opencl-lib,
      [  --with-opencl-lib=ARG   OpenCL Link Flags (i.e. -L/xxx -lOpenCL)],,,)

  if test "x$with_opencl_lib" = "x" ; then
    if test ! -z "`uname | grep Darwin`" ; then
      OPENCL_LIB="-framework OpenCL"
    else
      AC_CHECK_LIB(OpenCL,clGetPlatformIDs,OPENCL_LIB=-lOpenCL,OPENCL_LIB=missing)
      if test "$OPENCL_LIB" = "missing"; then
        AC_MSG_ERROR([--with-opencl requested, but libraries not found!])
      fi
    fi
  else
    OPENCL_LIB="$with_opencl_lib"
  fi

fi

AC_MSG_RESULT([$OPENCL_SETTING])

AC_SUBST(OPENCL_FLAGS,  $OPENCL_FLAGS)
AC_SUBST(OPENCL_LIB,    $OPENCL_LIB)


dnl ---------------------------------------------------------------------------
dnl Check if FreeXL library is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(freexl,[  --with-freexl[=ARG]    Include freexl support (ARG=no, yes (default) or libfreexl install path)],,)

AC_MSG_CHECKING([for FreeXL support])

HAVE_FREEXL=no
FREEXL_INCLUDE=

if test "$with_freexl" = "" -o "$with_freexl" = "yes" ; then
  AC_CHECK_HEADERS(freexl.h)
  AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-lfreexl",FREEXL_LIBS=missing)

  if test "$FREEXL_LIBS" = "missing"; then
    unset ac_cv_lib_freexl_freexl_open
    AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-lfreexl -lm",FREEXL_LIBS=missing,-lm)
  fi

  if test "$FREEXL_LIBS" = "missing"; then
    unset ac_cv_lib_freexl_freexl_open
    AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-lfreexl -liconv",FREEXL_LIBS=missing,-liconv)
  fi

  if test "$FREEXL_LIBS" = "missing"; then
    unset ac_cv_lib_freexl_freexl_open
    AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-lfreexl -liconv -lm",FREEXL_LIBS=missing,-liconv -lm)
  fi

  if test "$FREEXL_LIBS" = "missing"; then
    unset ac_cv_lib_freexl_freexl_open
    AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-lfreexl -liconv -lcharset",FREEXL_LIBS=missing,-liconv -lcharset)
  fi

  if test "$FREEXL_LIBS" != "missing" -a "$ac_cv_header_freexl_h" = "yes" ; then

    # Check that freexl is recent enough
    rm -f testrlist.*
    echo '#include <freexl.h>' > testfreexl.c
    echo 'int main(int argc, char** argv) { FreeXL_CellValue s; freexl_get_cell_value (0,0,0,&s); return 0; } ' >> testfreexl.c
    if test -z "`${CC} ${CFLAGS} testfreexl.c -c 2>&1`" ; then
        HAVE_FREEXL=yes
        LIBS="$FREEXL_LIBS $LIBS"
    else
        HAVE_FREEXL=no
        AC_MSG_RESULT([freexl too old. Needs freexl >= 1.0])
    fi
    rm -f testfreexl.*

  else
    HAVE_FREEXL=no
    echo "libfreexl not found - FreeXL support disabled"
  fi

elif test "$with_freexl" != "no"; then

  AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-L$with_freexl/lib -lfreexl",FREEXL_LIBS=missing,-L$with_freexl/lib)

  if test "$FREEXL_LIBS" = "missing"; then
    unset ac_cv_lib_freexl_freexl_open
    AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-L$with_freexl/lib -lfreexl -lm",FREEXL_LIBS=missing,-L$with_freexl/lib -lm)
  fi

  if test "$FREEXL_LIBS" = "missing"; then
    unset ac_cv_lib_freexl_freexl_open
    AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-L$with_freexl/lib -lfreexl -liconv",FREEXL_LIBS=missing,-L$with_freexl/lib -liconv)
  fi

  if test "$FREEXL_LIBS" = "missing"; then
    unset ac_cv_lib_freexl_freexl_open
    AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-L$with_freexl/lib -lfreexl -liconv -lm",FREEXL_LIBS=missing,-L$with_freexl/lib -liconv -lm)
  fi

  if test "$FREEXL_LIBS" = "missing"; then
    unset ac_cv_lib_freexl_freexl_open
    AC_CHECK_LIB(freexl,freexl_open,FREEXL_LIBS="-L$with_freexl/lib -lfreexl -liconv -lcharset",FREEXL_LIBS=missing,-L$with_freexl/lib -liconv -lcharset)
  fi

  if test "$FREEXL_LIBS" != "missing" -a -f "$with_freexl/include/freexl.h" ; then

    # Check that freexl is recent enough
    rm -f testrlist.*
    echo '#include <freexl.h>' > testfreexl.c
    echo 'int main(int argc, char** argv) { FreeXL_CellValue s; freexl_get_cell_value (0,0,0,&s); return 0; } ' >> testfreexl.c
    if test -z "`${CC} ${CFLAGS} -I$with_freexl/include testfreexl.c -c 2>&1`" ; then
        HAVE_FREEXL=yes
        LIBS="$FREEXL_LIBS $LIBS"
        FREEXL_INCLUDE="-I$with_freexl/include"
    else
        HAVE_FREEXL=no
        AC_MSG_RESULT([freexl too old. Needs freexl >= 1.0])
    fi
    rm -f testfreexl.*

  else
    HAVE_FREEXL=no
    echo "libfreexl not found - FreeXL support disabled"
  fi

fi

AC_SUBST(HAVE_FREEXL,  $HAVE_FREEXL)
AC_SUBST(FREEXL_INCLUDE,  $FREEXL_INCLUDE)


dnl ---------------------------------------------------------------------------
dnl Check if libjson-c is available.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(libjson-c,[  --with-libjson-c[=ARG]       Include libjson-c support (ARG=internal or libjson-c directory)],,)

if test "$with_libjson_c" = "external" -o "$with_libjson_c" = "" -o "$with_libjson_c" = "yes" ; then
  AC_CHECK_LIB(json-c,json_object_set_serializer,LIBJSONC_SETTING=external,LIBJSONC_SETTING=internal,)
elif test "$with_libjson_c" = "internal" ; then
  LIBJSONC_SETTING=internal
elif test "$with_libjson_c" != "no"; then
  LIBS="-L$with_libjson_c/lib $LIBS"
  AC_CHECK_LIB(json-c,json_object_set_serializer,LIBJSONC_SETTING=external,LIBJSONC_SETTING=internal,-L$with_libjson_c/lib)
else
  AC_MSG_ERROR([libjson-c (internal or external) is required])
fi

if test "$LIBJSONC_SETTING" = "external" ; then
  AC_MSG_RESULT([using pre-installed libjson-c])
  LIBS="-ljson-c $LIBS"
  if test "$with_libjson_c" != "" -a "$with_libjson_c" != "yes" -a "$with_libjson_c" != "external" ; then
    JSON_INCLUDE="-I$with_libjson_c/include/json-c"
  elif test -f "/usr/include/json-c/json.h"; then
    JSON_INCLUDE="-I/usr/include/json-c"
  elif test -f "/usr/local/include/json-c/json.h"; then
    JSON_INCLUDE="-I/usr/local/include/json-c"
  else
    AC_MSG_ERROR([could not find json-c/json.h])
  fi
else
    JSON_INCLUDE="-I\$(GDAL_ROOT)/ogr/ogrsf_frmts/geojson/libjson"
    AC_MSG_RESULT([using internal libjson-c code])
fi

AC_SUBST(LIBJSONC_SETTING,$LIBJSONC_SETTING)
AC_SUBST(JSON_INCLUDE,$JSON_INCLUDE)


dnl ---------------------------------------------------------------------------
dnl Check if we must enable PAM
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([whether to enable PAM])

AC_ARG_WITH(pam,[  --without-pam         Disable PAM (.aux.xml) support],,)

export PAM_SETTING=

if test "x$with_pam" = "xno" ; then
  AC_MSG_RESULT([no])
else
  AC_MSG_RESULT([yes])
  PAM_SETTING=-DPAM_ENABLED
fi

AC_SUBST(PAM_SETTING, $PAM_SETTING)

dnl ---------------------------------------------------------------------------
dnl Check if the PDF driver should be built as a plugin
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([whether to build PDF driver as a plugin])

AC_ARG_ENABLE(pdf_plugin,
              AS_HELP_STRING([--enable-pdf-plugin],
                             [enable PDF driver as a plugin (included in libgdal by default)]),,)

PDF_PLUGIN=no
if test "x$enable_pdf_plugin" = "xyes" ; then

  if test "$with_libtool" = "yes"; then
    AC_MSG_ERROR([--enable-pdf-plugin not supported together --with-libtool])
  fi

  PDF_PLUGIN=yes

  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

AC_SUBST(PDF_PLUGIN, $PDF_PLUGIN)
if test "$PDF_PLUGIN" != "yes" ; then
  if test "x$INTERNAL_FORMAT_pdf_ENABLED" = "xyes"; then
    if test "$LIBZ_SETTING" != "no" ; then
      GDALFORMATS_ENABLED="$GDALFORMATS_ENABLED pdf"
    fi
  fi
fi


dnl ---------------------------------------------------------------------------
dnl Check if libpoppler is available
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(poppler,[  --with-poppler[=ARG]    Include poppler(for PDF) support (ARG=no(default), yes or poppler install path)],,)

HAVE_POPPLER=no
POPPLER_MAJOR_VERSION=
POPPLER_MINOR_VERSION=

AC_MSG_CHECKING([for poppler])

if test "$with_poppler" != "no" -a "$with_poppler" != ""; then

    PKG_PROG_PKG_CONFIG([0.21])
    PKG_CHECK_MODULES([POPPLER], [poppler >= 0.23.0],
            [POPPLER_VERSION=`$PKG_CONFIG --modversion poppler`], [POPPLER_VERSION=])
    if test "$POPPLER_VERSION" != ""; then
        HAVE_POPPLER=yes
        POPPLER_MAJOR_VERSION=`echo $POPPLER_VERSION | cut -d'.' -f1`
        POPPLER_MINOR_VERSION=`echo $POPPLER_VERSION | cut -d'.' -f2 | sed -e 's/^0//'`
        POPPLER_CFLAGS=`echo $POPPLER_CFLAGS $POPPLER_CFLAGS | sed "s/include\/poppler/include/"`
    fi
else
    AC_MSG_RESULT([disabled])
fi

if test "$HAVE_POPPLER" = "yes" -a "$PDF_PLUGIN" = "no"; then
    LIBS="${POPPLER_LIBS} ${LIBS}"
fi

AC_SUBST(HAVE_POPPLER, $HAVE_POPPLER)
AC_SUBST(POPPLER_MAJOR_VERSION, $POPPLER_MAJOR_VERSION)
AC_SUBST(POPPLER_MINOR_VERSION, $POPPLER_MINOR_VERSION)
AC_SUBST(POPPLER_CFLAGS, $POPPLER_CFLAGS)
AC_SUBST(POPPLER_PLUGIN_LIB, $POPPLER_LIBS)

dnl ---------------------------------------------------------------------------
dnl Check if libpodofo is available
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(podofo,[  --with-podofo[=ARG]    Include podofo(for PDF) support (ARG=no(default), yes or podofo install path)],,)

HAVE_PODOFO=no

AC_MSG_CHECKING([for podofo])

if test "$with_podofo" != "no" -a "$with_podofo" != ""; then

    AC_ARG_WITH(podofo-lib,
      [  --with-podofo-lib=ARG   podofo Link Flags (i.e. -L/xxx -lpodofo ...). Mainly for static libpodofo],,,)

    if test "$with_podofo" = "yes" ; then
        TEST_PODOFO_INC="-I/usr/include/podofo"
        PODOFO_LIB="-lpodofo"
    else
        TEST_PODOFO_INC="-I$with_podofo/include -I$with_podofo/include/podofo"
        PODOFO_LIB="-L$with_podofo/lib -lpodofo"
    fi

    if test "x$with_podofo_lib" = "x" ; then
        rm -f testpodofo.*
        echo '#include <podofo.h>' > testpodofo.cpp
        echo 'int main(int argc, char** argv) { PoDoFo::PdfError::EnableDebug( 0 ); return 0; } ' >> testpodofo.cpp
        ${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS} testpodofo.cpp -o testpodofo ${TEST_PODOFO_INC} ${PODOFO_LIB} 2>/dev/null
        RETVAL=$?
        if test $RETVAL -eq 0; then
            HAVE_PODOFO=yes
            if test "$PDF_PLUGIN" = "no"; then
                LIBS="${PODOFO_LIB} ${LIBS}"
            fi
            PODOFO_INC=$TEST_PODOFO_INC
            PODOFO_PLUGIN_LIB="${PODOFO_LIB}"
            AC_MSG_RESULT([yes])
        fi
        rm -f testpodofo.*
        rm -f testpodofo
    fi

    if test "$HAVE_PODOFO" = "no"; then
        if test "x$with_podofo_lib" != "x" ; then
            PODOFO_LIB="$with_podofo_lib"
        else
            # This may be a static libpodofo.a, so add dependent libraries.
            PODOFO_LIB="$PODOFO_LIB -lfreetype -lfontconfig -lpthread"
        fi

        AC_ARG_WITH(podofo-extra-lib-for-test,
          [  --with-podofo-extra-lib-for-test=ARG   Additional libraries to pass the detection test, but not used for libgdal linking (i.e. -ljpeg ...). Mainly for static libpodofo],,,)

        if test "x$with_podofo_extra_lib_for_test" = "x" ; then
            TEST_PODOFO_LIB="$PODOFO_LIB"
        else
            TEST_PODOFO_LIB="$PODOFO_LIB $with_podofo_extra_lib_for_test"
        fi

        rm -f testpodofo.*
        echo '#include <podofo.h>' > testpodofo.cpp
        echo 'int main(int argc, char** argv) { PoDoFo::PdfError::EnableDebug( 0 ); return 0; } ' >> testpodofo.cpp
        ${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS} testpodofo.cpp -o testpodofo ${TEST_PODOFO_INC} ${TEST_PODOFO_LIB} 2>/dev/null
        RETVAL=$?
        if test $RETVAL -eq 0; then
            HAVE_PODOFO=yes
            if test "$PDF_PLUGIN" = "no"; then
                LIBS="${PODOFO_LIB} ${LIBS}"
            fi
            PODOFO_INC=$TEST_PODOFO_INC
            PODOFO_PLUGIN_LIB="${PODOFO_LIB}"
            AC_MSG_RESULT([yes])
        else
            AC_MSG_RESULT([no])
        fi
        rm -f testpodofo.*
        rm -f testpodofo
    fi
else
    AC_MSG_RESULT([disabled])
fi

AC_SUBST(HAVE_PODOFO, $HAVE_PODOFO)
AC_SUBST(PODOFO_INC, $PODOFO_INC)
AC_SUBST(PODOFO_PLUGIN_LIB, $PODOFO_PLUGIN_LIB)

dnl ---------------------------------------------------------------------------
dnl Check if libpdfium is available
dnl
dnl Support for open-source PDFium library
dnl
dnl Copyright (C) 2015 Klokan Technologies GmbH (http://www.klokantech.com/)
dnl Author: Martin Mikita <martin.mikita@klokantech.com>, xmikit00 @ FIT VUT Brno
dnl Copyright 2019 Even Rouault
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(pdfium,[  --with-pdfium[=ARG]    Include pdfium (for PDF) support (ARG=no(default), yes or pdfium install path)],,)

HAVE_PDFIUM=no

AC_MSG_CHECKING([for pdfium])

if test "$with_pdfium" != "no" -a "$with_pdfium" != ""; then

    if test "$with_pdfium" = "yes" ; then
        PDFIUM_INC="-I/usr/include/pdfium"
        PDFIUM_LIB="-lpdfium"
    else
        PDFIUM_INC="-I$with_pdfium/include/pdfium"
        PDFIUM_LIB="-L$with_pdfium/lib -lpdfium"
    fi

    PDFIUM_LIB="$PDFIUM_LIB -lopenjp2 -ljpeg -lpng -lz -llcms2 -lpthread -lm -lstdc++"

    if test ! -z "`uname | grep Darwin`" ; then
        PDFIUM_LIB="-stdlib=libstdc++ $PDFIUM_LIB"
    fi

    rm -f testpdfium.*
    echo '#include "public/fpdfview.h"' > testpdfium.cpp
    echo '#include "core/fpdfapi/page/cpdf_page.h"' >> testpdfium.cpp
    echo '#include "constants/transparency.h"' >> testpdfium.cpp # specific to pdfium_build_gdal_3_2
    echo 'int main(int argc, char** argv) { FPDF_InitLibrary(); FPDF_DestroyLibrary(); return 0; } ' >> testpdfium.cpp
    TEST_CXX_FLAGS=""
    if test ! -z "`uname | grep Darwin`" ; then
        TEST_CXX_FLAGS="$TEST_CXX_FLAGS -stdlib=libstdc++"
    fi
    if ${CXX} ${CXXFLAGS} ${CPPFLAGS} $TEST_CXX_FLAGS -std=c++14 testpdfium.cpp -o testpdfium ${PDFIUM_INC} ${PDFIUM_LIB} >/dev/null 2>/dev/null; then
        HAVE_PDFIUM=yes
        CXXFLAGS="$TEST_CXX_FLAGS $CXXFLAGS"
        if test "$PDF_PLUGIN" = "no"; then
            LIBS="${PDFIUM_LIB} ${LIBS}"
        fi
        PDFIUM_PLUGIN_LIB="${PDFIUM_LIB}"
        AC_MSG_RESULT([yes])
    fi
    rm -f testpdfium.*
    rm -f testpdfium

    if test "$HAVE_PDFIUM" = "no" -a "$with_pdfium" != ""; then
        AC_MSG_ERROR([pdfium requested but not found. Must come from https://github.com/rouault/pdfium_build_gdal_3_4])
    fi
else
    AC_MSG_RESULT([disabled])
fi

AC_SUBST(HAVE_PDFIUM, $HAVE_PDFIUM)
AC_SUBST(PDFIUM_INC, $PDFIUM_INC)
AC_SUBST(PDFIUM_PLUGIN_LIB, $PDFIUM_PLUGIN_LIB)

dnl ---------------------------------------------------------------------------
dnl Get/override version.
dnl ---------------------------------------------------------------------------


AC_ARG_WITH(gdal-ver,[  --with-gdal-ver=ARG   Override GDAL version ],,)
GDAL_VERSION_MAJOR=`grep "#*define.GDAL_VERSION_MAJOR.*" gcore/gdal_version.h.in | awk '{print $4}'`
GDAL_VERSION_MINOR=`grep "#*define.GDAL_VERSION_MINOR.*" gcore/gdal_version.h.in | awk '{print $4}'`
GDAL_VERSION_REV=`grep "#*define.GDAL_VERSION_REV.*" gcore/gdal_version.h.in | awk '{print $4}'`

AC_MSG_RESULT([checking GDAL version information from gdal_version.h.in: $GDAL_VERSION_MAJOR.$GDAL_VERSION_MINOR.$GDAL_VERSION_REV])

AC_SUBST(GDAL_VERSION_MAJOR,    "$GDAL_VERSION_MAJOR")
AC_SUBST(GDAL_VERSION_MINOR,    "$GDAL_VERSION_MINOR")
AC_SUBST(GDAL_VERSION_REV,    "$GDAL_VERSION_REV")

if test "x$with_gdal_ver" != "x"; then

  GDAL_VER=$with_gdal_ver
  AC_MSG_RESULT([  overridden GDAL version: $GDAL_VER])
else

  GDAL_VER=$GDAL_VERSION_MAJOR.$GDAL_VERSION_MINOR.$GDAL_VERSION_REV

fi

AC_SUBST(GDAL_VER, $GDAL_VER)
echo $GDAL_VER > VERSION

dnl ---------------------------------------------------------------------------
dnl MacOS X Framework Build

if test ! -z "`uname | grep Darwin`" ; then

  CXXFLAGS="$ARCHFLAGS $CXXFLAGS"
  CFLAGS="$ARCHFLAGS $CFLAGS"
  LIBS="$ARCHFLAGS $LIBS"

  MACOSX_FRAMEWORK=no
  AC_ARG_WITH(macosx-framework,[  --with-macosx-framework         Build and install GDAL as a Mac OS X Framework],,)
  AC_MSG_CHECKING([for Mac OS X Framework build])
  if test "$with_macosx_framework" = yes ; then
    MACOSX_FRAMEWORK=yes
    AC_MSG_RESULT([enabled])
    AC_DEFINE_UNQUOTED(MACOSX_FRAMEWORK,1, [Define for Mac OSX Framework build])
    prefix="/Library/Frameworks/GDAL.framework/Versions/${GDAL_VERSION_MAJOR}.${GDAL_VERSION_MINOR}"
    exec_prefix='${prefix}'
    includedir='${prefix}/Headers'
    datadir='${prefix}/Resources/gdal'
    libdir='${exec_prefix}'
    bindir='${exec_prefix}/Programs'
    docdir='${prefix}/Resources/doc'
    mandir='${prefix}/Resources/man'
  else
    AC_MSG_RESULT([disabled])
  fi
  AC_SUBST(MACOSX_FRAMEWORK, $MACOSX_FRAMEWORK)
fi

AC_SUBST(EXTRA_INCLUDES,$EXTRA_INCLUDES)

dnl ---------------------------------------------------------------------------
dnl Check whether we need to add specific suffix for executables (.EXE when
dnl building for Windows). This test should work for the case of
dnl cross-compilation too.
dnl ---------------------------------------------------------------------------
case "${host_os}" in
    cygwin* | mingw* | pw32*)
      EXE_EXT=.exe
      SO_EXT=dll
      ;;
    *)
      EXE_EXT=
      ;;
esac

AC_SUBST(EXE_EXT)
AC_SUBST(SO_EXT)

dnl ---------------------------------------------------------------------------
dnl Which "true" program should we use?
dnl ---------------------------------------------------------------------------
BINTRUE=/bin/true

if test -f /bin/true.exe ; then
  BINTRUE=/bin/true.exe
fi
if test -f /usr/bin/true ; then
  BINTRUE=/usr/bin/true
fi

export BINTRUE

AC_SUBST(BINTRUE,$BINTRUE)

dnl ---------------------------------------------------------------------------
dnl Capture GDAL_PREFIX for the cpl_config.h include file.
dnl ---------------------------------------------------------------------------

if test "$prefix" = "NONE" ; then
  GDAL_PREFIX=/usr/local
else
  GDAL_PREFIX=${prefix}
fi

export GDAL_PREFIX
AC_DEFINE_UNQUOTED(GDAL_PREFIX,["]$GDAL_PREFIX["], [--prefix directory for GDAL install])

dnl ---------------------------------------------------------------------------
dnl Perl bindings.
dnl ---------------------------------------------------------------------------

BINDINGS=

AC_ARG_WITH(perl,[  --with-perl           Enable perl bindings],,)

AC_MSG_CHECKING([for perl bindings])
if test "$with_perl" = "yes" ; then
  BINDINGS="perl $BINDINGS"
  AC_MSG_RESULT([enabled])
else
  AC_MSG_RESULT([disabled])
fi

dnl ---------------------------------------------------------------------------
dnl NG Python bindings.
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(python,[  --with-python=ARG   Enable python bindings (ARG=yes, no, or path to python binary)],,)

AC_MSG_CHECKING([for python bindings])
if test "$with_python" = "yes" ; then
  dnl
  dnl Check for Python executable in PATH
  dnl
  AC_CHECK_PROGS([PYTHON], [python python3 python2])
  if test "x$PYTHON" = "x"; then
    AC_MSG_ERROR([--with-python requested, but Python not found!])
  fi
elif test "x$with_python" != "x" -a "$with_python" != "no" ; then
  PYTHON="$with_python"
  if test "`$PYTHON -c 'print(1)'`" != "1"; then
    AC_MSG_ERROR([--with-python requested, but Python not found!])
  fi
  AC_MSG_RESULT([enabled])
else
  PYTHON=""
  AC_MSG_RESULT([disabled])
fi

if test "x$PYTHON" != "x"; then
  BINDINGS="python $BINDINGS"

  AC_MSG_CHECKING([for python setuptools])
  SETUPTEST='try:
  import setuptools
  print(1)
except ImportError:
  pass'
  PY_HAVE_SETUPTOOLS=`$PYTHON -c "$SETUPTEST"`
  if test "$PY_HAVE_SETUPTOOLS" = "1"; then
    AC_MSG_RESULT([found])
  else
    AC_MSG_RESULT([not found])
  fi
fi

AC_SUBST([PY_HAVE_SETUPTOOLS])
AC_SUBST([BINDINGS])

dnl ---------------------------------------------------------------------------
dnl Java support
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(java,[  --with-java       Include Java support (ARG=yes, no or JDK home path)  [[default=no]]],,)

AC_MSG_CHECKING([whether we should include Java support])

JAVA_HOME=""
JAVA_INC=""
mac_java_fw=""

if test "x$with_java" = "xyes"; then

dnl Ubuntu
    JAVA_6_TEST_DIR=
    JAVA_7_TEST_DIR=
    JAVA_8_TEST_DIR=
    JAVA_MAC_TEST_DIR=
    case "$host_cpu" in
        i*86*)
            JAVA_6_TEST_DIR=/usr/lib/jvm/java-6-openjdk-i386
            JAVA_7_TEST_DIR=/usr/lib/jvm/java-7-openjdk-i386
            JAVA_8_TEST_DIR=/usr/lib/jvm/java-8-openjdk-i386
            ;;
        x86_64*)
            JAVA_6_TEST_DIR=/usr/lib/jvm/java-6-openjdk-amd64
            JAVA_7_TEST_DIR=/usr/lib/jvm/java-7-openjdk-amd64
            JAVA_8_TEST_DIR=/usr/lib/jvm/java-8-openjdk-amd64
            ;;
        *)
            ;;
    esac
    case "${host_os}" in
      darwin*)
        JAVA_MAC_TEST_DIR=$(/usr/libexec/java_home)
        ;;
    esac

    if test "x$JAVA_8_TEST_DIR" != "x" -a -d "$JAVA_8_TEST_DIR/include"; then
        with_java="$JAVA_8_TEST_DIR"

    elif test "x$JAVA_7_TEST_DIR" != "x" -a -d "$JAVA_7_TEST_DIR/include"; then
        with_java="$JAVA_7_TEST_DIR"

    elif test "x$JAVA_6_TEST_DIR" != "x" -a -d "$JAVA_6_TEST_DIR/include"; then
        with_java="$JAVA_6_TEST_DIR"

    elif test -d /usr/lib/jvm/java-6-openjdk; then
        with_java="/usr/lib/jvm/java-6-openjdk"

dnl RHEL6
    elif test -d /usr/lib/jvm/java-openjdk; then
        with_java="/usr/lib/jvm/java-openjdk"


dnl OpenSolaris
    elif test -d /usr/java; then
        with_java="/usr/java"

dnl Mac OS X
    elif test "x$JAVA_MAC_TEST_DIR" != "x"; then
        if test -d "$JAVA_MAC_TEST_DIR/bundle"; then
            # Mac default 1.6.0_x framework
            # test for ./bundle first, since older installs also have ./include
            with_java="$JAVA_MAC_TEST_DIR"
            mac_java_fw="JavaVM"
        elif test -d "$JAVA_MAC_TEST_DIR/include"; then
            # Oracle Java install
            with_java="$JAVA_MAC_TEST_DIR"
        fi
    else
        AC_MSG_ERROR("cannot find JDK root directory.")
    fi

    if test \! -d "$with_java/include" && test \! -d "$with_java/bundle"; then
        AC_MSG_ERROR("$with_java is not a valid JDK.")
    fi
fi

if test "x$JAVA_INC" != "x"; then
    AC_MSG_RESULT([yes])
elif test "x$with_java" != "x" -a "x$with_java" != "xno"; then

    if test -d "$with_java/bundle" && test -d "/System/Library/Frameworks/JavaVM.framework/Headers"; then
        # Mac default 1.6.0_x framework
        # test for ./bundle first, since older installs also have ./include
        JAVA_HOME="$with_java"
        JAVA_INC="-I/System/Library/Frameworks/JavaVM.framework/Headers"
        mac_java_fw="JavaVM"
        AC_MSG_RESULT([yes])
    elif test -d "$with_java/include"; then
        if test -d "$with_java/include/linux"; then
            JAVA_HOME="$with_java"
            JAVA_INC="-I$JAVA_HOME/include -I$JAVA_HOME/include/linux"
            AC_MSG_RESULT([yes])
        elif test -d "$with_java/include/solaris"; then
            JAVA_HOME="$with_java"
            JAVA_INC="-I$JAVA_HOME/include -I$JAVA_HOME/include/solaris"
            AC_MSG_RESULT([yes])
        elif test -d "$with_java/include/freebsd"; then
            JAVA_HOME="$with_java"
            JAVA_INC="-I$JAVA_HOME/include -I$JAVA_HOME/include/freebsd"
            AC_MSG_RESULT([yes])
        elif test -d "$with_java/include/darwin"; then
            # Oracle Java install, use instead of Mac default 1.6.0_x
            JAVA_HOME="$with_java"
            JAVA_INC="-I$JAVA_HOME/include -I$JAVA_HOME/include/darwin"
            AC_MSG_RESULT([yes])
        else
            AC_MSG_ERROR("Cannot find $with_java/include/linux or solaris or freebsd or darwin directory.")
        fi
    else
        AC_MSG_ERROR("Cannot find $with_java/include or <mac_java_framework>/Headers directory.")
    fi

else
    AC_MSG_RESULT([no])
fi

AC_SUBST(JAVA_HOME,$JAVA_HOME)
AC_SUBST(JAVA_INC,$JAVA_INC)

dnl ---------------------------------------------------------------------------
dnl HDFS support
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(hdfs,[  --with-hdfs[=ARG]        Include HDFS support (ARG=Path or no)],,)

HDFS_ENABLED=no

if test -z "$with_hdfs" -o "$with_hdfs" = "no" ; then
     AC_MSG_RESULT([HDFS support not requested.])
else
     if test "$JAVA_HOME"; then
          JNI_LIB="-L$(find $JAVA_HOME | grep libjvm | head -1 | sed "s/[[^/]]*$//") -ljvm"
     else
          AC_MSG_ERROR([HDFS support requires java support (--with-java). $JAVA_HOME])
     fi
     if echo $JNI_LIB | grep $JAVA_HOME; then
          AC_MSG_CHECKING([for HDFS in $with_hdfs])
          AC_CHECK_LIB(hdfs,hdfsConnect,HAVE_HDFS_LIB=yes,HAVE_HDFS_LIB=no,$JNI_LIB -L$with_hdfs/lib/native)
          AC_CHECK_FILE($with_hdfs/include/hdfs.h,HAVE_HDFS_HEADER=yes,HAVE_HDFS_HEADER=no)
          if test "$HAVE_HDFS_LIB" = "yes" -a "$HAVE_HDFS_HEADER" = "yes"; then
               AC_MSG_RESULT([HDFS support enabled.])
               HDFS_ENABLED=yes
               HDFS_LIB="-L$with_hdfs/lib/native -lhdfs"
               HDFS_INC="-I$with_hdfs/include"
          else
               AC_MSG_ERROR([HDFS support not enabled.])
          fi
     else
          AC_MSG_ERROR([Unable to find libjvm.])
     fi
fi

AC_SUBST(HDFS_ENABLED,$HDFS_ENABLED)
AC_SUBST(HDFS_LIB,$HDFS_LIB)
AC_SUBST(HDFS_INC,$HDFS_INC)
AC_SUBST(JNI_LIB,$JNI_LIB)

dnl ---------------------------------------------------------------------------
dnl TileDB support
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(tiledb,[  --with-tiledb[=ARG]    Include TileDB support (ARG=no, yes or tiledb install root path)],,)

TILEDB_SETTING=no
TILEDB_CFLAGS=

if test "$with_tiledb" = "yes" -o "$with_tiledb" = "" ; then

  AC_CHECK_LIB(tiledb,tiledb_coords,TILEDB_SETTING=yes,TILEDB_SETTING=no,)

  if test "$TILEDB_SETTING" = "yes" ; then
    LIBS="-ltiledb $LIBS"
  else
    echo "libtiledb not found - TileDB support disabled"
  fi

elif test "$with_tiledb" != "no" -a "$with_tiledb" != ""; then

  AC_CHECK_LIB(tiledb,tiledb_coords,TILEDB_SETTING=yes,TILEDB_SETTING=no,-L$with_tiledb/lib -ltiledb)

  if test "$TILEDB_SETTING" = "yes" ; then
    LIBS="-L$with_tiledb/lib -ltiledb $LIBS"
    TILEDB_CFLAGS="-I$with_tiledb/include"
  else
    echo "libtiledb not found - TileDB support disabled"
  fi
fi

if test "$TILEDB_SETTING" != "no" ; then

  AC_LANG_PUSH([C++])
  SAVED_CXXFLAGS=$CXXFLAGS
  CXXFLAGS="$CXXFLAGS $TILEDB_CFLAGS"
  AC_MSG_CHECKING([if tiledb/tiledb can be included without additional flags])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include "tiledb/tiledb"]])],
    [TILEDB_CAN_BE_INCLUDED="yes"]
    [AC_MSG_RESULT([yes])],
    [TILEDB_CAN_BE_INCLUDED="no"]
    [AC_MSG_RESULT([no])])
  CXXFLAGS=$SAVED_CXXFLAGS
  AC_LANG_POP([C++])

  if test "$TILEDB_CAN_BE_INCLUDED" = "no"; then
    AC_LANG_PUSH([C++])
    SAVED_CXXFLAGS=$CXXFLAGS
    CXXFLAGS="$CXXFLAGS $TILEDB_CFLAGS -std=c++17"
    AC_MSG_CHECKING([if tiledb/tiledb can be included with -std=c++17])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
      [[#include "tiledb/tiledb"]])],
      [TILEDB_CAN_BE_INCLUDED="yes"]
      [AC_MSG_RESULT([yes])],
      [TILEDB_CAN_BE_INCLUDED="no"]
      [AC_MSG_RESULT([no])])
    CXXFLAGS=$SAVED_CXXFLAGS
    AC_LANG_POP([C++])

    if test "$TILEDB_CAN_BE_INCLUDED" = "yes"; then
      TILEDB_CFLAGS="$TILEDB_CFLAGS -std=c++17"
    fi
  fi

  OPT_GDAL_FORMATS="tiledb $OPT_GDAL_FORMATS"
fi

AC_SUBST(TILEDB_SETTING,$TILEDB_SETTING)
AC_SUBST(TILEDB_CFLAGS,$TILEDB_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl MDB driver
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(mdb,[  --with-mdb       Include MDB driver],,)

AC_MSG_CHECKING([whether we should include MDB support])

MDB_ENABLED=no
JVM_LIB=""

if test "$with_mdb" = "yes" ; then
    if test "x$JAVA_INC" = "x"; then
        AC_MSG_ERROR("--with-java must be specified.")
    fi

    AC_ARG_WITH(jvm-lib,          [  --with-jvm-lib=[ARG]        ARG is dlopen or points to Java libjvm path],,)

    AC_ARG_WITH(jvm-lib-add-rpath,[  --with-jvm-lib-add-rpath    Add the libjvm path to the RPATH (no by default)],,)

    if test "x$with_jvm_lib" = "xdlopen"; then
        AC_DEFINE_UNQUOTED(JVM_LIB_DLOPEN, 1, [Define to 1 if libjvm.so should be dlopen'd])
        JVM_LIB="-ldl"
    elif test "x$with_jvm_lib" != "x"; then
        if test -d "$with_jvm_lib"; then
            saved_LDFLAGS="$LDFLAGS"
            LDFLAGS="$LDFLAGS -L$with_jvm_lib"
            if test "x$mac_java_fw" != "x"; then
                AC_CHECK_FW_FUNC($mac_java_fw,JNI_CreateJavaVM,HAS_LIB_JVM=yes,HAS_LIB_JVM=no,)
            else
                AC_CHECK_LIB(jvm,JNI_CreateJavaVM,HAS_LIB_JVM=yes,HAS_LIB_JVM=no,)
            fi
            LDFLAGS="$saved_LDFLAGS"
            if test "$HAS_LIB_JVM" = "yes"; then
                JVM_LIB="-L$with_jvm_lib -ljvm"
                if test "x$mac_java_fw" != "x"; then
                    JVM_LIB="-framework $mac_java_fw"
                fi
                if test "x$with_jvm_lib_add_rpath" = "xyes"; then
                    case "${host_os}" in
                      darwin*)
                          if test "x$mac_java_fw" = "x"; then
                              JVM_LIB="-Wl,-rpath,$with_jvm_lib -L$with_jvm_lib -lazy-ljvm"
                          fi
                          ;;
                      *)
                          JVM_LIB="-Wl,-rpath=$with_jvm_lib $JVM_LIB"
                          ;;
                    esac
                fi
            else
                AC_MSG_ERROR("--with-jvm-lib must point to a directory with a libjvm.")
            fi
        else
            AC_MSG_ERROR("--with-jvm-lib must point to a directory.")
        fi
    elif test "x$JAVA_HOME" != "x"; then
        TEST_DIR="$JAVA_HOME/jre/lib/amd64/server"
        if test -d "$JAVA_HOME/jre/lib/server"; then
            # Oracle Java install on Mac
            TEST_DIR="$JAVA_HOME/jre/lib/server"
        fi
        if test "x$mac_java_fw" != "x"; then
            # Mac default 1.6.0_x framework, TEST_DIR not used
            TEST_DIR="$with_java"
        fi
        if test "x$JVM_LIB" = "x" -a -d "$TEST_DIR"; then
            unset ac_cv_lib_jvm_JNI_CreateJavaVM
            saved_LDFLAGS="$LDFLAGS"
            LDFLAGS="$LDFLAGS -L$TEST_DIR"
            if test "x$mac_java_fw" != "x"; then
                AC_CHECK_FW_FUNC($mac_java_fw,JNI_CreateJavaVM,HAS_LIB_JVM=yes,HAS_LIB_JVM=no,)
            else
                AC_CHECK_LIB(jvm,JNI_CreateJavaVM,HAS_LIB_JVM=yes,HAS_LIB_JVM=no,)
            fi
            LDFLAGS="$saved_LDFLAGS"
            if test "$HAS_LIB_JVM" = "yes"; then
                JVM_LIB="-L$TEST_DIR -ljvm"
                if test "x$mac_java_fw" != "x"; then
                    JVM_LIB="-framework $mac_java_fw"
                fi
                if test "x$with_jvm_lib_add_rpath" = "xyes"; then
                    case "${host_os}" in
                      darwin*)
                          if test "x$mac_java_fw" = "x"; then
                              JVM_LIB="-Wl,-rpath,$TEST_DIR -L$TEST_DIR -lazy-ljvm"
                          fi
                          ;;
                      *)
                          JVM_LIB="-Wl,-rpath=$TEST_DIR $JVM_LIB"
                          ;;
                    esac
                fi
            fi
        fi

        TEST_DIR="$JAVA_HOME/jre/lib/i386/server"
        if test "x$JVM_LIB" = "x" -a -d "$TEST_DIR"; then
            unset ac_cv_lib_jvm_JNI_CreateJavaVM
            saved_LDFLAGS="$LDFLAGS"
            LDFLAGS="$LDFLAGS -L$TEST_DIR"
            AC_CHECK_LIB(jvm,JNI_CreateJavaVM,HAS_LIB_JVM=yes,HAS_LIB_JVM=no,)
            LDFLAGS="$saved_LDFLAGS"
            if test "$HAS_LIB_JVM" = "yes"; then
                JVM_LIB="-L$TEST_DIR -ljvm"
                if test "x$with_jvm_lib_add_rpath" = "xyes"; then
                    JVM_LIB="-Wl,-rpath=$TEST_DIR $JVM_LIB"
                fi
            fi
        fi

        if test "x$JVM_LIB" = "x"; then
            AC_MSG_ERROR("--with-jvm-lib must be specified.")
        fi
    else
        AC_MSG_ERROR("--with-jvm-lib must be specified.")
    fi

    MDB_ENABLED=yes
    AC_MSG_RESULT([yes])

else
    AC_MSG_RESULT([no])
fi

AC_SUBST(MDB_ENABLED,$MDB_ENABLED)
AC_SUBST(JVM_LIB,$JVM_LIB)


dnl ---------------------------------------------------------------------------
dnl Select Rasdaman or disable driver.
dnl ---------------------------------------------------------------------------

RASDAMAN_ENABLED=no

AS_MESSAGE([checking whether we should include rasdaman support...])
AC_ARG_WITH(rasdaman,
[  --with-rasdaman[=DIR]        Include rasdaman support (DIR is rasdaman's install dir).],,)

if test "$with_rasdaman" = "yes" ; then

  AC_CHECK_LIB(raslib,main,RASDAMAN_ENABLED=yes,,,)

  if test -n "$RASDAMAN_ENABLED" ; then
      RASDAMAN_LIB="-lrasodmg -lclientcomm -lcompression -lnetwork -lraslib"
      OPT_GDAL_FORMATS="rasdaman $OPT_GDAL_FORMATS"
      AC_MSG_RESULT([        using rasdaman from system libs.])
  else
      AC_MSG_WARN([        rasdaman not found in system libs... use --with-rasdaman=DIR.])
  fi

elif test -n "$with_rasdaman" -a "$with_rasdaman" != "no" ; then

  RASDAMAN_DIR=$with_rasdaman

  if test -f $RASDAMAN_DIR/include/rasdaman.hh -a -d$RASDAMAN_DIR/lib ; then
      RASDAMAN_INC=-I$RASDAMAN_DIR/include
      RASDAMAN_LIB="-L$RASDAMAN_DIR/lib -lrasodmg -lclientcomm -lcompression -lnetwork -lraslib"
  else
      AC_MSG_ERROR("Could not find rasdaman.hh or libraslib.a in $RASDAMAN_DIR.")
  fi

  RASDAMAN_ENABLED=yes
  OPT_GDAL_FORMATS="rasdaman $OPT_GDAL_FORMATS"
  AC_MSG_RESULT([        using rasdaman from $RASDAMAN_DIR.])

else

  AC_MSG_RESULT([        rasdaman support not requested.])
fi

AC_SUBST(RASDAMAN_ENABLED,$RASDAMAN_ENABLED)
AC_SUBST(RASDAMAN_INC,    $RASDAMAN_INC)
AC_SUBST(RASDAMAN_LIB,    $RASDAMAN_LIB)


dnl ---------------------------------------------------------------------------
dnl brunsli support
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(brunsli,[  --with-brunsli    Include brunsli support via libbrunsli library],,)

BRUNSLI_ENABLED=no

if test "$with_brunsli" != "" -a "$with_brunsli" != "no" ; then
  BRUNSLI_ENABLED="yes"
  AC_SUBST(BRUNSLI_ENABLED, $BRUNSLI_ENABLED)
  AC_SUBST(BRUNSLI_INCLUDE, -I$prefix/include)
  AC_SUBST(BRUNSLI_LIB, "-lbrunslienc-c -lbrunslidec-c")
fi

dnl ---------------------------------------------------------------------------
dnl RDB support
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(rdb,[  --with-rdb[=ARG]    Include RDB support (ARG=no, yes or RDB SDK root path)],,)

RDB_SETTING=no

if test "$with_rdb" = "yes" -o "$with_rdb" = "" ; then

  AC_CHECK_LIB(rdb,rdb_library_name,RDB_SETTING=yes,RDB_SETTING=no,)

  if test "$RDB_SETTING" = "yes" ; then
    LIBS="-lrdb $LIBS"
  else
    echo "librdb not found - RDB support disabled"
  fi

elif test "$with_rdb" != "no" -a "$with_rdb" != ""; then

  AC_CHECK_LIB(rdb,rdb_library_name,RDB_SETTING=yes,RDB_SETTING=no,-L$with_rdb/library -lrdb)

  if test "$RDB_SETTING" = "yes" ; then
    LIBS="-L$with_rdb/library -lrdb $LIBS"
    EXTRA_INCLUDES="-I$with_rdb/interface/cpp/ -I$with_rdb/interface/c/ $EXTRA_INCLUDES"
  else
    echo "librdb not found - RDB support disabled"
  fi

fi

AC_SUBST(RDB_SETTING,$RDB_SETTING)

if test "$RDB_SETTING" != "no" ; then
  OPT_GDAL_FORMATS="rdb $OPT_GDAL_FORMATS"
fi


dnl ---------------------------------------------------------------------------
dnl Armadillo support
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(armadillo,[  --with-armadillo=ARG       Include Armadillo support for faster TPS transform computation (ARG=yes/no/path to armadillo install root) [[default=no]]],,)

AC_MSG_CHECKING([whether we should include Armadillo support])

HAVE_ARMADILLO=no

if test "$with_armadillo" = "yes" ; then
    rm -f testarmadillo.*
    rm -f testarmadillo
    echo '#include <armadillo>' > testarmadillo.cpp
    echo 'int main(int argc, char** argv) { arma::mat matInput(2,2); const arma::mat& matInv = arma::inv(matInput); return 0; } ' >> testarmadillo.cpp
    if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} testarmadillo.cpp -o testarmadillo -larmadillo 2>&1`" ; then
        HAVE_ARMADILLO=yes
        LIBS="-larmadillo ${LIBS}"
        AC_MSG_RESULT([yes])
    else
        dnl Try adding -llapack (#4923)
        if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} testarmadillo.cpp -o testarmadillo -larmadillo -llapack 2>&1`" ; then
            HAVE_ARMADILLO=yes
            LIBS="-larmadillo -llapack ${LIBS}"
            AC_MSG_RESULT([yes])
        else
            # clang 3.4 needs linking against libstdc++ (ubuntu 14.04)
            if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} testarmadillo.cpp -o testarmadillo -larmadillo -lstdc++ 2>&1`" ; then
                HAVE_ARMADILLO=yes
                LIBS="-larmadillo -lstdc++ ${LIBS}"
                AC_MSG_RESULT([yes])
            else
                AC_MSG_RESULT([no])
            fi
        fi
    fi
    rm -f testarmadillo.*
    rm -f testarmadillo

elif test -n "$with_armadillo" -a "$with_armadillo" != "no" ; then

    rm -f testarmadillo.*
    rm -f testarmadillo
    echo '#include <armadillo>' > testarmadillo.cpp
    echo 'int main(int argc, char** argv) { arma::mat matInput(2,2); const arma::mat& matInv = arma::inv(matInput); return 0; } ' >> testarmadillo.cpp
    if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -I$with_armadillo/include testarmadillo.cpp -o testarmadillo -L$with_armadillo/lib -larmadillo 2>&1`" ; then
        HAVE_ARMADILLO=yes
        LIBS="-L$with_armadillo/lib -larmadillo ${LIBS}"
        EXTRA_INCLUDES="-I$with_armadillo/include $EXTRA_INCLUDES"
        AC_MSG_RESULT([yes])
    else
        dnl Try adding -llapack (#4923)
        if test -z "`${CXX} ${CXXFLAGS} ${CPPFLAGS} -I$with_armadillo/include testarmadillo.cpp -o testarmadillo -L$with_armadillo/lib -larmadillo -llapack 2>&1`" ; then
            HAVE_ARMADILLO=yes
            LIBS="-L$with_armadillo/lib -larmadillo -llapack ${LIBS}"
            EXTRA_INCLUDES="-I$with_armadillo/include $EXTRA_INCLUDES"
            AC_MSG_RESULT([yes])
        else
            AC_MSG_RESULT([no])
        fi
    fi
    rm -f testarmadillo.*
    rm -f testarmadillo

else
    AC_MSG_RESULT([no])
fi

AC_SUBST(HAVE_ARMADILLO,$HAVE_ARMADILLO)

dnl ---------------------------------------------------------------------------
dnl libcryptopp
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(cryptopp,[  --with-cryptopp[=ARG]       Include cryptopp support (ARG=yes, no or path)],,)

HAVE_CRYPTOPP=no
USE_ONLY_CRYPTODLL_ALG=yes

AC_MSG_CHECKING([for cryptopp])

if test "$with_cryptopp" = "no" ; then

  AC_MSG_RESULT([disabled])

elif test "$with_cryptopp" = "yes" -o "$with_cryptopp" = "" ; then

    rm -f testcryptopp.*
    echo '#include <cryptopp/aes.h>' > testcryptopp.cpp
    # Catch issue with clang++ (https://groups.google.com/forum/#!topic/cryptopp-users/DfWHy3bT0KI)
    echo '#include <cryptopp/osrng.h>' >> testcryptopp.cpp
    echo 'int main(int argc, char** argv) { CryptoPP::AES::Encryption oEnc; return 0; } ' >> testcryptopp.cpp
    ${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS} testcryptopp.cpp -o testcryptopp -lcryptopp 2>/dev/null
    RETVAL=$?
    if test $RETVAL -eq 0; then
        HAVE_CRYPTOPP=yes
        LIBS="-lcryptopp ${LIBS}"
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
        if test "$with_cryptopp" = "yes"; then
            AC_MSG_ERROR([--with-cryptopp was requested, but not available])
        fi
    fi
    rm -f testcryptopp.*
    rm -f testcryptopp

    if test "$HAVE_CRYPTOPP" = "yes"; then

        AC_MSG_CHECKING([if cryptopp has all cipher methods builtin])
        rm -f testcryptopp.*
        echo '#include <cryptopp/blowfish.h>' > testcryptopp.cpp
        echo 'int main(int argc, char** argv) { CryptoPP::Blowfish::Encryption oEnc; return 0; } ' >> testcryptopp.cpp
        ${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS} testcryptopp.cpp -o testcryptopp -lcryptopp 2>/dev/null
        RETVAL=$?
        if test $RETVAL -eq 0; then
            USE_ONLY_CRYPTODLL_ALG=no
            AC_MSG_RESULT([yes])
        else
            USE_ONLY_CRYPTODLL_ALG=yes
            AC_MSG_RESULT([no])
        fi
        rm -f testcryptopp.*
        rm -f testcryptopp
    fi

else

    rm -f testcryptopp.*
    echo '#include <cryptopp/aes.h>' > testcryptopp.cpp
    # Catch issue with clang++ (https://groups.google.com/forum/#!topic/cryptopp-users/DfWHy3bT0KI)
    echo '#include <cryptopp/osrng.h>' >> testcryptopp.cpp
    echo 'int main(int argc, char** argv) { CryptoPP::AES::Encryption oEnc; return 0; } ' >> testcryptopp.cpp
    ${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS} testcryptopp.cpp -I$with_cryptopp/include -o testcryptopp -L$with_cryptopp/lib -lcryptopp 2>/dev/null
    RETVAL=$?
    if test $RETVAL -eq 0; then
        HAVE_CRYPTOPP=yes
        LIBS="-L$with_cryptopp/lib -lcryptopp ${LIBS}"
        EXTRA_INCLUDES="-I$with_cryptopp/include $EXTRA_INCLUDES"
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
    fi
    rm -f testcryptopp.*
    rm -f testcryptopp

    if test "$HAVE_CRYPTOPP" = "yes"; then

        AC_MSG_CHECKING([if cryptopp has all cipher methods builtin])
        rm -f testcryptopp.*
        echo '#include <cryptopp/blowfish.h>' > testcryptopp.cpp
        echo 'int main(int argc, char** argv) { CryptoPP::Blowfish::Encryption oEnc; return 0; } ' >> testcryptopp.cpp
        ${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS} testcryptopp.cpp -I$with_cryptopp/include -o testcryptopp -L$with_cryptopp/lib -lcryptopp 2>/dev/null
        RETVAL=$?
        if test $RETVAL -eq 0; then
            USE_ONLY_CRYPTODLL_ALG=no
            AC_MSG_RESULT([yes])
        else
            USE_ONLY_CRYPTODLL_ALG=yes
            AC_MSG_RESULT([no])
        fi
        rm -f testcryptopp.*
        rm -f testcryptopp
    fi

fi

AC_SUBST(HAVE_CRYPTOPP,$HAVE_CRYPTOPP)
AC_SUBST(USE_ONLY_CRYPTODLL_ALG,$USE_ONLY_CRYPTODLL_ALG)

dnl ---------------------------------------------------------------------------
dnl crypto
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(crypto,[  --with-crypto[=ARG]       Include crypto (from openssl) support (ARG=yes, no or path)],,)

HAVE_OPENSSL_CRYPTO=no

AC_MSG_CHECKING([for crypto])

if test "$with_crypto" = "no" ; then

  AC_MSG_RESULT([disabled])

elif test "$with_crypto" = "yes" -o \( "$with_crypto" = "" -a "$HAVE_CRYPTOPP" = "no" \) ; then

    AC_CHECK_LIB(crypto,EVP_sha256,HAVE_OPENSSL_CRYPTO=yes)
    if test "$HAVE_OPENSSL_CRYPTO" = "yes"; then
        AC_CHECK_HEADERS([openssl/bio.h])
        if test "$ac_cv_header_openssl_bio_h" = "no"; then
            HAVE_OPENSSL_CRYPTO=no
        else
            LIBS="-lcrypto $LIBS"
        fi
    fi
fi

AC_SUBST(HAVE_OPENSSL_CRYPTO,$HAVE_OPENSSL_CRYPTO)

dnl ---------------------------------------------------------------------------
dnl Whether to include LERC in the build
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(lerc,
    [  --with-lerc[=ARG]         Include libLerc support(ARG=yes/no/internal/path). Used by internal libtiff and MRF.],,,)

HAVE_LERC=no
AC_MSG_CHECKING([for LERC])
if test "$with_lerc" = "internal"; then
    if test "$ac_cv_c_bigendian" = "yes"; then
        AC_MSG_RESULT([disabled on big-endian])
    else
        HAVE_LERC=internal
        AC_MSG_RESULT([enabled, internal])
    fi
elif test "$with_lerc" = "no"; then
    HAVE_LERC=no
    AC_MSG_RESULT([disabled by user])
elif test "$with_lerc" = "yes" -o "x$with_lerc" = "x" ; then
  lerc_lib_name="LercLib"
  AC_CHECK_LIB("$lerc_lib_name", lerc_decode, [lerc_lib=yes], [lerc_lib=no],)
  if test "$lerc_lib" = "no" ; then
    lerc_lib_name="Lerc"
    AC_CHECK_LIB("$lerc_lib_name", lerc_decode, [lerc_lib=yes], [lerc_lib=no],)
  fi
  if test "$lerc_lib" = "no" -a "$with_lerc" = "yes"; then
    AC_MSG_ERROR([lerc library not found])
  fi

  AC_CHECK_HEADER(Lerc_c_api.h, [lerc_c_api_h=yes], [lerc_c_api_h=no])
  if test "$lerc_c_api_h" = "no" -a "$with_lerc" = "yes"; then
    AC_MSG_ERROR([Liblerc headers not found])
  fi

  if test "$lerc_lib" = "yes" -a "$lerc_c_api_h" = "yes"; then
    HAVE_LERC=external
    LIBS="$LIBS -l$lerc_lib_name"
  else
    if test "$ac_cv_c_bigendian" = "yes"; then
        AC_MSG_RESULT([disabled on big-endian])
    else
        HAVE_LERC=internal
        AC_MSG_RESULT([enabled, internal])
    fi
  fi
else

  ORIG_LIBS="$LIBS"
  LIBS="-L$with_lerc/lib $LIBS"
  lerc_lib_name="LercLib"
  AC_CHECK_LIB("$lerc_lib_name", lerc_decode, [lerc_lib=yes], [lerc_lib=no],)
  if test "$lerc_lib" = "no" ; then
    lerc_lib_name="Lerc"
    AC_CHECK_LIB("$lerc_lib_name", lerc_decode, [lerc_lib=yes], [lerc_lib=no],)
  fi
  if test "$lerc_lib" = "no"; then
    AC_MSG_ERROR([lerc library not found])
  fi

  LIBS="-L$with_lerc/lib -l$lerc_lib_name $ORIG_LIBS"

  if ! test -f "$with_lerc/include/Lerc_c_api.h"; then
    AC_MSG_ERROR([Liblerc headers not found])
  fi

  HAVE_LERC=external
  LERC_INCLUDE="-I$with_lerc/include"
fi

AC_SUBST(HAVE_LERC,$HAVE_LERC)
AC_SUBST(LERC_INCLUDE,$LERC_INCLUDE)

dnl ---------------------------------------------------------------------------
dnl Whether to include NULL driver in the build
dnl ---------------------------------------------------------------------------

AC_ARG_WITH([null],
	    AS_HELP_STRING([--with-null],
	       [Enable NULL driver (only useful for development purposes]),,)

AC_MSG_CHECKING([for NULL])
if test "$with_null" = "yes" ; then
    OPT_GDAL_FORMATS="null $OPT_GDAL_FORMATS"
    AC_MSG_RESULT([enabled])
else
    AC_MSG_RESULT([disabled])
fi

dnl ---------------------------------------------------------------------------
dnl Detect OpenEXR
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(exr,[  --with-exr            Include EXR support via OpenEXR library],,)

HAVE_EXR=no
EXR_CFLAGS=""

if test "$with_exr" = "no" ; then

  AC_MSG_NOTICE([EXR support disabled.])

else

  PKG_PROG_PKG_CONFIG([0.21])
  PKG_CHECK_MODULES([EXR], [OpenEXR >= 2.2], [HAVE_EXR=yes], [HAVE_EXR=no])

  if test -n "$EXR_CFLAGS"; then

    # Test that the package found is for the right architecture
    saved_LIBS="$LIBS"
    LIBS="$EXR_LIBS"
    AC_CHECK_LIB(IlmImf,ImfTiledInputReadTile, [HAVE_EXR=yes], [HAVE_EXR=no])
    if test "$HAVE_EXR" = "no"; then
      dnl Try with OpenEXR >= 3 name
      AC_CHECK_LIB(OpenEXR,ImfTiledInputReadTile, [HAVE_EXR=yes], [HAVE_EXR=no])
    fi
    LIBS="$saved_LIBS"

    if test "$HAVE_EXR" = "yes"; then
        OPT_GDAL_FORMATS="exr $OPT_GDAL_FORMATS"
        LIBS="$EXR_LIBS $LIBS"
    else
        EXR_CFLAGS=""
    fi
  fi
fi

AC_SUBST(EXR_CFLAGS,$EXR_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl Detect libheif
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(heif,[  --with-heif            Include HEIF support via libheif library],,)

HAVE_HEIF=no
HEIF_CFLAGS=""

if test "$with_heif" = "no" ; then

  AC_MSG_NOTICE([HEIF support disabled.])

else

  PKG_PROG_PKG_CONFIG([0.21])
  PKG_CHECK_MODULES([HEIF], [libheif >= 1.1], [], [HAVE_HEIF=no])

  if test -n "$HEIF_LIBS"; then

    # Test that the package found is for the right architecture
    saved_LIBS="$LIBS"
    LIBS="$HEIF_LIBS"
    AC_CHECK_LIB(heif,heif_context_get_list_of_top_level_image_IDs, [HAVE_HEIF=yes], [HAVE_HEIF=no])
    LIBS="$saved_LIBS"

    if test "$HAVE_HEIF" = "yes"; then
        OPT_GDAL_FORMATS="heif $OPT_GDAL_FORMATS"
        LIBS="$HEIF_LIBS $LIBS"
    else
        HEIF_CFLAGS=""
    fi
  fi
fi

AC_SUBST(HEIF_CFLAGS,$HEIF_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl Detect libjxl
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(jxl,[  --with-jxl            Include JPEG-XL support via libjxl library, requires an internal libtiff.],,)

HAVE_JXL=no

if test "$TIFF_SETTING" != "internal" -o  "$with_jxl" = "no" ; then

  AC_MSG_NOTICE([JXL support disabled.])

else

  PKG_PROG_PKG_CONFIG([0.21])
  PKG_CHECK_MODULES([JXL], [libjxl], [], [HAVE_JXL=no])

  if test -n "$JXL_LIBS" ; then

    # Test that the package found is for the right architecture
    saved_LIBS="$LIBS"
    LIBS="$JXL_LIBS"
    AC_CHECK_LIB(jxl,JxlEncoderOptionsCreate, [HAVE_JXL=yes], [HAVE_JXL=no])
    LIBS="$saved_LIBS"

    if test "$HAVE_JXL" = "yes"; then
        LIBS="$JXL_LIBS $LIBS"

        AC_CHECK_LIB(jxl,JxlEncoderFrameSettingsSetOption, [HAVE_JxlEncoderFrameSettingsSetOption=yes], [HAVE_JxlEncoderFrameSettingsSetOption=no])
        if test "$HAVE_JxlEncoderFrameSettingsSetOption" = "yes"; then
            JXL_CFLAGS="$JXL_CFLAGS -DHAVE_JxlEncoderFrameSettingsSetOption"
        fi
    fi
  fi
fi

AC_SUBST(HAVE_JXL,$HAVE_JXL)
AC_SUBST(JXL_CFLAGS,$JXL_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl Test if sprintf can be overloaded with _XOPEN_SOURCE being defined or other,
dnl includes being included before, which mingw64 4.8 doesn't like (#6390)
dnl ---------------------------------------------------------------------------

AC_MSG_CHECKING([if sprintf can be overloaded for GDAL compilation])
echo '#define _XOPEN_SOURCE' > testdeprecated.cpp
echo '#include <vector>' >> testdeprecated.cpp
echo '#include <stdio.h>' >> testdeprecated.cpp
echo 'extern "C"' >> testdeprecated.cpp
echo '{' >> testdeprecated.cpp
echo 'int sprintf(char *str, const char* fmt, ...);' >> testdeprecated.cpp
echo '}' >> testdeprecated.cpp
${CXX} ${CXXFLAGS} ${CPPFLAGS} testdeprecated.cpp -c 2>/dev/null
RETVAL=$?
if test $RETVAL -eq 0; then
    AC_MSG_RESULT([yes])
else
    CPPFLAGS="$CPPFLAGS -DDONT_DEPRECATE_SPRINTF"
    AC_MSG_RESULT([no])
fi
rm -f testdeprecated.*
rm -f testdeprecated

dnl Use -municode for mingw
case "${host_os}" in
    mingw32*)
        AC_MSG_CHECKING([if mingw supports wmain])
        rm -f testwmain*
        echo '#include <wchar.h>' > testwmain.cpp
        echo '#include <stdlib.h>' >> testwmain.cpp
        echo 'extern "C" int wmain( int argc, wchar_t ** argv_w );' >> testwmain.cpp
        echo 'extern "C" int wmain( int , wchar_t **  ) { return 0; } ' >> testwmain.cpp
        ${CXX} ${CXXFLAGS} ${CPPFLAGS} ${LDFLAGS} testwmain.cpp -o testwmain.exe -municode 2>/dev/null
        RETVAL=$?
        rm -f testwmain*
        if test $RETVAL -eq 0; then
            AC_MSG_RESULT([yes])
            CXXFLAGS="$CXXFLAGS -DSUPPORTS_WMAIN"
            LDFLAGS="$LDFLAGS -municode"
        else
            AC_MSG_RESULT([no])
        fi
        ;;
    *)
        ;;
esac

AC_CONFIG_FILES([GDALmake.opt])
AC_OUTPUT

dnl ---------------------------------------------------------------------------
dnl Display configuration status
dnl ---------------------------------------------------------------------------

LOC_MSG()
LOC_MSG([GDAL is now configured for ${host}])
LOC_MSG()
LOC_MSG([  Installation directory:    ${prefix}])
LOC_MSG([  C compiler:                ${CC} ${CFLAGS}])
LOC_MSG([  C++ compiler:              ${CXX} ${CXXFLAGS}])
LOC_MSG([  C++14 support:             ${CXX14_SUPPORT}])
LOC_MSG()
LOC_MSG([  LIBTOOL support:           ${with_libtool}])
LOC_MSG()
LOC_MSG([  Armadillo support:         ${HAVE_ARMADILLO}])
LOC_MSG([  Blosc support:             ${HAVE_BLOSC}])
LOC_MSG([  CFITSIO support:           ${FITS_SETTING}])
LOC_MSG([  crypto/openssl support:    ${HAVE_OPENSSL_CRYPTO}])
LOC_MSG([  cryptopp support:          ${HAVE_CRYPTOPP}])
LOC_MSG([  cURL support (wms/wcs/...):${CURL_SETTING}])
LOC_MSG([  DDS support:               ${DDS_SETTING}])
LOC_MSG([  DODS support:              ${HAVE_DODS}])
LOC_MSG([  ECW support:               ${ECW_SETTING}])
LOC_MSG([  Expat support:             ${HAVE_EXPAT}])
LOC_MSG([  EXR support:               ${HAVE_EXR}])
LOC_MSG([  FGDB support:              ${FGDB_ENABLED}])
LOC_MSG([  FreeXL support:            ${HAVE_FREEXL}])
LOC_MSG([  GEORASTER support:         ${HAVE_GEORASTER}])
LOC_MSG([  GEOS support:              ${HAVE_GEOS}])
LOC_MSG([  Google libkml support:     ${HAVE_LIBKML}])
LOC_MSG([  GRASS support:             ${GRASS_SETTING}])
LOC_MSG([  GTA support:               ${GTA_SETTING}])
LOC_MSG([  HDF4 support:              ${HAVE_HDF4}])
LOC_MSG([  HDF5 support:              ${HAVE_HDF5}])
LOC_MSG([  JXL support:               ${HAVE_JXL}])
LOC_MSG([  HDFS support:              ${HDFS_ENABLED}])
LOC_MSG([  HEIF support:              ${HAVE_HEIF}])
LOC_MSG([  INFORMIX DataBlade support:${HAVE_IDB}])
LOC_MSG([  Ingres support:            ${HAVE_INGRES}])
LOC_MSG([  JP2Lura support:           ${HAVE_JP2LURA}])
LOC_MSG([  JPEG 12 bit:               ${JPEG12_ENABLED}])
LOC_MSG([  JPEG-in-TIFF 12 bit:       ${TIFF_JPEG12_ENABLED}])
if test "x$HAVE_JASPER_UUID" != "x" ; then
  LOC_MSG([  JPEG JasPer support:       ${HAVE_JASPER} (GeoJP2=${HAVE_JASPER_UUID})])
else
  LOC_MSG([  JPEG JasPer support:       ${HAVE_JASPER}])
fi
LOC_MSG([  JPEG-Lossless/CharLS:      ${HAVE_CHARLS}])
LOC_MSG([  Kakadu support:            ${HAVE_KAKADU}])
LOC_MSG([  Kea support:               ${HAVE_KEA}])
LOC_MSG([  LERC support:              ${HAVE_LERC}])
LOC_MSG([  libbrunsli support:        ${BRUNSLI_ENABLED}])
LOC_MSG([  libdeflate support:        ${LIBDEFLATE_SETTING}])
LOC_MSG([  LIBGEOTIFF support:        ${GEOTIFF_SETTING}])
LOC_MSG([  LIBGIF support:            ${GIF_SETTING}])
LOC_MSG([  LIBJPEG support:           ${JPEG_SETTING}])
LOC_MSG([  LIBLZMA support:           ${LIBLZMA_SETTING}])
LOC_MSG([  LIBPNG support:            ${PNG_SETTING}])
LOC_MSG([  LIBTIFF support:           ${TIFF_SETTING} (BigTIFF=${HAVE_BIGTIFF})])
LOC_MSG([  libxml2 support:           ${HAVE_LIBXML2}])
LOC_MSG([  LIBZ support:              ${LIBZ_SETTING}])
LOC_MSG([  LZ4 support:               ${HAVE_LZ4}])
LOC_MSG([  MDB support:               ${MDB_ENABLED}])
LOC_MSG([  MongoCXX v3 support:       ${MONGOCXXV3_ENABLED}])
LOC_MSG([  MongoDB support:           ${MONGODB_ENABLED}])
LOC_MSG([  MrSID/MG4 Lidar support:   ${HAVE_MRSID_LIDAR}])
LOC_MSG([  MrSID support:             ${HAVE_MRSID}])
LOC_MSG([  MSG support:               ${HAVE_MSG}])
LOC_MSG([  MySQL support:             ${HAVE_MYSQL}])
if test "$NETCDF_SETTING" = "yes" ; then
  LOC_MSG([  NetCDF has netcdf_mem.h:   ${NETCDF_MEM}])
fi
LOC_MSG([  NetCDF support:            ${NETCDF_SETTING}])
LOC_MSG([  OCI support:               ${HAVE_OCI}])
LOC_MSG([  ODBC support:              ${ODBC_SETTING}])
LOC_MSG([  OGDI support:              ${HAVE_OGDI}])
LOC_MSG([  OpenCL support:            ${OPENCL_SETTING}])
LOC_MSG([  OpenJPEG support:          ${HAVE_OPENJPEG}])
LOC_MSG([  PCIDSK support:            ${PCIDSK_SETTING}])
LOC_MSG([  PCRaster support:          ${PCRASTER_SETTING}])
LOC_MSG([  PCRE support:              ${HAVE_PCRE}])
LOC_MSG([  PCRE2 support:             ${HAVE_PCRE2}])
LOC_MSG([  PDFium support:            ${HAVE_PDFIUM}])
LOC_MSG([  Podofo support:            ${HAVE_PODOFO}])
LOC_MSG([  Poppler support:           ${HAVE_POPPLER}])
LOC_MSG([  PostgreSQL support:        ${HAVE_PG}])
LOC_MSG([  QHull support:             ${QHULL_SETTING}])
LOC_MSG([  Rasdaman support:          ${RASDAMAN_ENABLED}])
LOC_MSG([  RasterLite2 support:       ${HAVE_RASTERLITE2}])
LOC_MSG([  RDB support:               ${RDB_SETTING}])
LOC_MSG([  SFCGAL support:            ${HAVE_SFCGAL}])
LOC_MSG([  SOSI support:              ${SOSI_ENABLED}])
LOC_MSG([  SpatiaLite support:        ${HAVE_SPATIALITE}])
if test "x$SPATIALITE_SONAME" != "x"; then
  LOC_MSG([  SpatiaLite shared obj name:${SPATIALITE_SONAME}])
fi
LOC_MSG([  SQLite support:            ${HAVE_SQLITE}])
LOC_MSG([  Teigha (DWG and DGNv8):    ${HAVE_TEIGHA}])
LOC_MSG([  TileDB support:            ${TILEDB_SETTING}])
LOC_MSG([  userfaultfd support:       ${ENABLE_UFFD}])
LOC_MSG([  WebP support:              ${WEBP_SETTING}])
LOC_MSG([  Xerces-C support:          ${HAVE_XERCES}])
LOC_MSG([  ZSTD support:              ${ZSTD_SETTING}])

LOC_MSG()
if test ! -z "`uname | grep Darwin`" ; then
  LOC_MSG([  Mac OS X Framework :       ${MACOSX_FRAMEWORK}])
fi
LOC_MSG()
if test -z "$BINDINGS"; then
    WHAT_BINDINGS="no"
else
    WHAT_BINDINGS="$BINDINGS"
fi
LOC_MSG([  misc. gdal formats:       ${GDALFORMATS_ENABLED}])
LOC_MSG([  misc. ogr formats:        ${OGRFORMATS_ENABLED}])
LOC_MSG()
LOC_MSG([  disabled gdal formats:    ${GDALFORMATS_DISABLED}])
LOC_MSG([  disabled ogr formats:     ${OGRFORMATS_DISABLED}])
LOC_MSG()
LOC_MSG([  SWIG Bindings:             ${WHAT_BINDINGS}])
LOC_MSG()
LOC_MSG([  PROJ >= 6:                 yes])
LOC_MSG([  enable GNM building:       ${GNM_ENABLED}])
LOC_MSG([  enable pthread support:    ${PTHREAD_ENABLED}])
LOC_MSG([  enable POSIX iconv support:${am_cv_func_iconv}])
LOC_MSG([  hide internal symbols:     ${HAVE_HIDE_INTERNAL_SYMBOLS}])
LOC_MSG()

if test "$HAVE_PODOFO" = "yes" -a "$HAVE_POPPLER" = "yes"  -a "$HAVE_PDFIUM" = "yes"; then
    AC_MSG_WARN([--with-podofo, --with-poppler and --with-pdfium available. This is unusual setup, but will work. Pdfium will be used by default...])
elif test "$HAVE_PODOFO" = "yes" -a "$HAVE_POPPLER" = "yes" ; then
    AC_MSG_WARN([--with-podofo and --with-poppler are both available. This is unusual setup, but will work. Poppler will be used by default...])
elif test "$HAVE_POPPLER" = "yes" -a "$HAVE_PDFIUM" = "yes" ; then
    AC_MSG_WARN([--with-poppler and --with-pdfium are both available. This is unusual setup, but will work. Pdfium will be used by default...])
elif test "$HAVE_PODOFO" = "yes" -a "$HAVE_PDFIUM" = "yes" ; then
    AC_MSG_WARN([--with-podofo and --with-pdfium are both available. This is unusual setup, but will work. Pdfium will be used by default...])
fi

if test "$HAVE_LIBXML2" = "yes" -a "$FGDB_ENABLED" = "yes"; then
    AC_MSG_WARN([--with-libxml2 and --with-fgdb are both available. There might be some incompatibility between system libxml2 and the embedded copy within libFileGDBAPI])
fi

if test "$HAVE_CRYPTOPP" = "yes" -a "$HAVE_OPENSSL_CRYPTO" = "yes"; then
    AC_MSG_WARN([--with-crypto and --with-cryptopp available. This is unusual setup, but will work.])
fi

if test "$HAVE_TEIGHA" = "yes" -a "$ECW_54" = "yes"; then
    AC_MSG_WARN([Both Teigha SDK and ECW SDK >= 5.4 found. This was found to cause crashes in ECW driver.])
fi

AC_MSG_NOTICE([])
AC_MSG_NOTICE([])
AC_MSG_WARN([Starting with GDAL 3.5, the autoconf build system is deprecated in favor of the CMake build system. The autoconf build system will be completely removed in GDAL 3.6])
