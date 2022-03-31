
/* We define this here in general so that a VC++ build will publically
   declare STDCALL interfaces even if an application is built against it
   using MinGW */

#ifndef CPL_DISABLE_STDCALL
#  define CPL_STDCALL __stdcall
#endif

/* Define if you have the vprintf function.  */
#cmakedefine HAVE_VPRINTF 1
#cmakedefine HAVE_VSNPRINTF 1
#cmakedefine HAVE_SNPRINTF 1
#if defined(_MSC_VER) && (_MSC_VER < 1500)
#  define vsnprintf _vsnprintf
#endif
#if defined(_MSC_VER) && (_MSC_VER < 1900)
#  define snprintf _snprintf
#endif

#cmakedefine HAVE_GETCWD 1
/* gmt_notunix.h from GMT project also redefines getcwd. See #3138 */
#ifndef getcwd
#define getcwd _getcwd
#endif

/* Define if you have the ANSI C header files.  */
#ifndef STDC_HEADERS
#cmakedefine STDC_HEADERS 1
#endif

/* Define to 1 if you have the <assert.h> header file. */
#cmakedefine HAVE_ASSERT_H 1

/* Define to 1 if you have the <fcntl.h> header file.  */
#cmakedefine HAVE_FCNTL_H 1

/* Define if you have the <unistd.h> header file.  */
#cmakedefine HAVE_UNISTD_H 1

/* Define if you have the <stdint.h> header file.  */
#cmakedefine HAVE_STDINT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#cmakedefine HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <locale.h> header file. */
#cmakedefine HAVE_LOCALE_H 1

#cmakedefine HAVE_FLOAT_H 1

#cmakedefine HAVE_ERRNO_H 1

#cmakedefine HAVE_SEARCH_H 1

/* Define to 1 if you have the <direct.h> header file. */
#cmakedefine HAVE_DIRECT_H 1

/* Define to 1 if you have the `localtime_r' function. */
#cmakedefine HAVE_LOCALTIME_R 1

#cmakedefine HAVE_DLFCN_H 1
#cmakedefine HAVE_DBMALLOC_H 1
#cmakedefine HAVE_LIBDBMALLOC 1
#cmakedefine WORDS_BIGENDIAN 1

/* The size of a `int', as computed by sizeof. */
#cmakedefine SIZEOF_INT @SIZEOF_INT@

/* The size of a `long', as computed by sizeof. */
#cmakedefine SIZEOF_LONG @SIZEOF_LONG@

/* The size of a `unsigned long', as computed by sizeof. */
#cmakedefine SIZEOF_UNSIGNED_LONG @SIZEOF_UNSIGNED_LONG@

/* The size of `void*', as computed by sizeof. */
#cmakedefine SIZEOF_VOIDP @SIZEOF_VOIDP@

/* Set the native cpu bit order */
#cmakedefine HOST_FILLORDER @HOST_FILLORDER@

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST @ICONV_CONST@

/* For .cpp files, define as const if the declaration of iconv() needs const. */
#define ICONV_CPP_CONST @ICONV_CPP_CONST@

/* Define as 0 or 1 according to the floating point format suported by the
   machine */
#cmakedefine HAVE_IEEEFP 1

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#  ifndef inline
#    define inline __inline
#  endif
#endif

#define lfind _lfind

#if defined(_MSC_VER) && (_MSC_VER < 1310)
#  define VSI_STAT64 _stat
#  define VSI_STAT64_T _stat
#else
#  define VSI_STAT64 _stat64
#  define VSI_STAT64_T __stat64
#endif

/* VC6 doesn't known intptr_t */
#if defined(_MSC_VER) && (_MSC_VER <= 1200)
    typedef int intptr_t;
#endif

#pragma warning(disable: 4786)

/* Define to 1, if your compiler supports long long data type */
#define HAVE_LONG_LONG 1

/* #define CPL_DISABLE_DLL */
