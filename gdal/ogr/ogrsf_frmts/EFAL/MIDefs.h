/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/
#ifndef MIDEFS_H
#define MIDEFS_H
#if defined(_MSC_VER)
#pragma once
#endif

#include <wchar.h>

// We will always turn on DEVELOPMENT when _DEBUG is defined
#ifdef _DEBUG
#if !defined(DEVELOPMENT)
#define DEVELOPMENT
#endif
#endif

// From https://stackoverflow.com/questions/2989810/which-cross-platform-preprocessor-defines-win32-or-win32-or-win32
#define ELLIS_OS_NT 1
#define ELLIS_OS_UNIX 2
#define WINWS  1
#define MTFWS  2
#define XOLWS  3

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
    /* UNIX-style OS. ------------------------------------------- */
#define ELLIS_OS ELLIS_OS_UNIX
#define ELLIS_OS_IS_WINOS 0
#define ELLIS_OS_IS_DOSBASED  0
#define ELLIS_OS_ISUNIX 1
#define XVTWS  -1

#define __int64 long long

typedef unsigned int DWORD;
typedef unsigned int MIUINT32;
typedef unsigned int* ULONG_PTR;
typedef MIUINT32 *PULONG;
typedef ULONG_PTR DWORD_PTR;
typedef unsigned short WORD;
typedef unsigned char BYTE;

typedef struct _GUID {
    DWORD Data1;
    WORD  Data2;
    WORD  Data3;
    BYTE  Data4[8];
} GUID;

inline bool operator ==(GUID a, GUID b) {
    return (a.Data1 == b.Data1) && (a.Data2 == b.Data2) && (a.Data3 == b.Data3)
        && (a.Data4[0] == b.Data4[0])
        && (a.Data4[1] == b.Data4[1])
        && (a.Data4[2] == b.Data4[2])
        && (a.Data4[3] == b.Data4[3])
        && (a.Data4[4] == b.Data4[4])
        && (a.Data4[5] == b.Data4[5])
        && (a.Data4[6] == b.Data4[6])
        && (a.Data4[7] == b.Data4[7])
        ;
}
#include <limits.h>
#define TRUE 1
#define FALSE 0
#define _MAX_PATH PATH_MAX

#include <unistd.h>
#if defined(_POSIX_VERSION)
/* POSIX compliant */
#endif

#if defined(__linux__)
    /* Linux  */
#elif defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/param.h>
#if defined(BSD)
    /* BSD (DragonFly BSD, FreeBSD, OpenBSD, NetBSD). ----------- */
#pragma message ("Hello BSD (DragonFly BSD, FreeBSD, OpenBSD, NetBSD). ")
#endif
#endif

#elif defined(__CYGWIN__) && !defined(_WIN32)
    /* Cygwin POSIX under Microsoft Windows. */
#pragma message ("Hello Cygwin POSIX under Microsoft Windows")

#elif defined(_WIN32) || defined(WIN32) || defined(WIN64)
#define XVTWS  WINWS
#define ELLIS_OS ELLIS_OS_NT
#define ELLIS_OS_IS_WINOS 1
#define ELLIS_OS_IS_DOSBASED  0
#define ELLIS_OS_ISUNIX 0

#else
#pragma message ("Hello unknown operating system!!! Your build will fail.")
This string will fail the build.

#endif

#if ELLIS_OS_IS_WINOS
#include <crtdbg.h>
#elif ELLIS_OS_ISUNIX
#include <cstring>
#endif

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DBG_NEW
#endif
#endif  // _DEBUG


/*
* Note on Windows constants such as LONG_MAX have fixed definitions (LONG_MAX is based on a 32 bit interger max).
* However, on Linux systems, these vary. For example, LONG_MAX may be a 32 bit or 64 bit max depending on the architecture.
* In places in our code where it matters, use these specific constants instead.
* The following constants appear to be the same across systems and do not need an architecture neutral definition:
*    #define SHRT_MIN    (-32768)        // minimum (signed) short value
*    #define SHRT_MAX      32767         // maximum (signed) short value
*    #define USHRT_MAX     0xffff        // maximum unsigned short value
*    #define UINT_MAX      0xffffffff    // maximum unsigned int value
*    #define LLONG_MAX     9223372036854775807i64       // maximum signed long long int value
*    #define LLONG_MIN   (-9223372036854775807i64 - 1)  // minimum signed long long int value
*    #define ULLONG_MAX    0xffffffffffffffffui64       // maximum unsigned long long int value
*
*
*  #define ULONG_MAX     0xffffffffUL  // maximum unsigned long value
*  #define MB_LEN_MAX    5             // max. # bytes in multibyte char
*/
#if ELLIS_OS_IS_WINOS
#include <basetsd.h>
typedef short     MI_INT16, *pMI_INT16;
typedef long      MI_INT32, *pMI_INT32;
typedef long long MI_INT64, *pMI_INT64;
typedef unsigned short     MI_UINT16, *pMI_UINT16;
typedef unsigned long      MI_UINT32, *pMI_UINT32;
typedef unsigned long long MI_UINT64, *pMI_UINT64;

#include <limits.h>
#define INT64MAX  _I64_MAX   //(9223372036854775807LL) The string representation of this value is positive 10675199.02:48:05.4775807.
#define INT64MIN  _I64_MIN   //(-9223372036854775808LL) The string representation of this value is negative 10675199.02:48:05.4775808.
#define INT32MAX  _I32_MAX   // 2147483647
#define INT32MIN  _I32_MIN   // (-2147483647 - 1)
#define UINT32MAX ULONG_MAX  // 0xffffffff
#define UINT64MAX ULLONG_MAX // 0xffffffffffffffff

typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long ULONG;
typedef long LONG;
#elif ELLIS_OS_ISUNIX
typedef short     MI_INT16, *pMI_INT16;
typedef int       MI_INT32, *pMI_INT32;
typedef long long MI_INT64, *pMI_INT64;
typedef unsigned short     MI_UINT16, *pMI_UINT16;
typedef unsigned int       MI_UINT32, *pMI_UINT32;
typedef unsigned long long MI_UINT64, *pMI_UINT64;

#include <climits>
#define INT64MAX  LLONG_MAX  //(9223372036854775807LL) The string representation of this value is positive 10675199.02:48:05.4775807.
#define INT64MIN  LLONG_MIN  //(-9223372036854775808LL) The string representation of this value is negative 10675199.02:48:05.4775808.
#define INT32MAX  INT_MAX    // 2147483647
#define INT32MIN  INT_MIN    // (-2147483647 - 1)
#define UINT32MAX UINT_MAX   // 0xffffffff
#define UINT64MAX ULLONG_MAX // 0xffffffffffffffff

typedef int BOOL;
typedef MI_UINT32 UINT;
typedef MI_UINT32 ULONG;
typedef MI_INT32 LONG;
#elif (ELLIS_OS == ELLIS_OS_OSF1)
/******************************************************************************
* MILONG32 is used for file IO for the OSF port. It is defined here because
* dec memory is always 64 bits long and mapinfo's file storge is 32 bits long.
******************************************************************************/
typedef unsigned int  *pMI_UINT32;
#endif

//typedef bool *pBOOLEAN;
typedef double DOUBLE8, *pDOUBLE8;
typedef void *pMEM, *pPTR;
typedef void *pVOID;
typedef char *pCHAR;
typedef const char *pCONSTCHAR;
typedef unsigned char UCHAR, *pUCHAR;
typedef short *pSHORT;
typedef unsigned short USHORT, *pUSHORT;
typedef MI_INT32 *pINT;
typedef MI_UINT32 *pUINT;
typedef float *pFLOAT;
typedef double *pDOUBLE;

#if ELLIS_OS_ISUNIX

#define PtrToLong( p )  ((long)(long *) (p) )
#define PtrToUlong( p ) ((unsigned long)(unsigned long *) (p) )
#define PtrToInt( p )  ( (int) (long) (int *) (p) )

#endif

namespace Ellis
{
    /************************************************************************
    * Define the list of known C compilers. If multiple versions
    * of a compiler must be distinguished we adopt the convention
    * that the constant with no numberic extension shall be defined
    * to be equal to the latest version.  This will allow the program to use
    * the non-extension version in most cases.  In cases where a program
    * is running with an earlier version of the compiler, then
    * it will need to check explicitly.
    ************************************************************************/

#define ELLIS_CC_UNKNOWN 0    /* unknown compiler */
#define ELLIS_CC_UNIXWARE 1   /* UnixWare cc compiler */
#define ELLIS_CC_ALPHA 2      /* DEC Alpha compiler */
#define ELLIS_CC_MSVC110 3    /* MSVC 5.0 */

#define ELLIS_CC  ELLIS_CC_MSVC110

    template<class T>
    inline void E_CLEAR_VAR(T& t) { memset(&t, 0, sizeof(T)); }

    template<class T>
    inline void E_SWAP(T *a, T* b, T* temp)
    {
        *temp = *a; *a = *b; *b = *temp;
    }
    template<class T>
    inline void E_SWAP(T& a, T& b)
    {
        T temp = a; a = b; b = temp;
    }

    //TODO:  move these into namespace in diff file only included where needed
    const double PI = 3.14159265358979323846;
    inline double DEG_TO_RAD(double a) { return a * PI / 180.0; }
    inline double RAD_TO_DEG(double a) { return a * 180.0 / PI; }

#define ABS(x)            ((x) >= 0 ? (x) : -(x))
#define LABS(x)           ((x) >= 0 ? (x) : -(x))
#define FABS(x)           ((x) >= 0.0 ? (x) : -(x))

#define CLIP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))
    template<class T>
    inline T Clip(const T& x, const T& min, const T& max)
    {
        return x < min ? min : (x > max ? max : x);
    }

    const char CR = '\r';
    const char LF = '\n';
    const char DBL_QUOTE = '"';
    const char BACKSLASH = '\\';
    const char ESC = 27;

#if !defined(NOREF)
#define NOREF(a)  (a);
#endif


    // we will not allow C++ streams in Ellis code under CE We only use it for debug anyway
#if !defined(UNDER_CE) && defined(DEVELOPMENT)
#define STREAMS_ALLOWED_IN_ELLIS
#endif

#ifdef UNDER_CE
#define MI_EXCEPTIONS_NOT_SUPPORTED
#endif

#ifdef MI_EXCEPTIONS_NOT_SUPPORTED
#define try
#define catch(xxx) if (0)
#if defined  _DEBUG
#if !defined MICOMPONENT_STATIC
#if defined __UTILITYDLL__
    __declspec(dllexport)
#else
    __declspec(dllimport)
#endif // __UTILITYDLL__
#endif // MICOMPONENT_STATIC
        bool MIAssertionFailed(wchar_t *pchFile, int iLine, wchar_t* pchMessage, ...);
#define THROW_DEFINE MIAssertionFailed(_T(__FILE__), __LINE__, L"Exception thrown");
#define throw THROW_DEFINE
#else
#define THROW_DEFINE
#define throw
#endif

#include <MIException.h>
#define MI_CHECK_EXCEPTION(type) { if (MIException<type>::GetError()) goto catch_##type; }
#define MI_CHECK_EXCEPTION_WITH_LABEL(label, type) { if (MIException<type>::GetError()) goto catch_##label; }
#define MI_CHECK_EXCEPTION_IMMEDIATE(type) { if (MIException<type>::GetError()) return; }
#define MI_CHECK_EXCEPTION_IMMEDIATE_RC(type, rc) { if (MIException<type>::GetError()) return(rc); }
#define MI_THROW(type, value) { MIException<type>::SetError(value); goto catch_##type; }
#define MI_THROW_WITH_LABEL(label, type, value) { MIException<type>::SetError(value); goto catch_##label; }
#define MI_THROW_IMMEDIATE(type, value) { MIException<type>::SetError(value); return; }
#define MI_THROW_IMMEDIATE_RC(type, value, rc) { MIException<type>::SetError(value); return(rc); }
#define MI_RETHROW(type) { MIException<type>::SetError(); return; }
#define MI_RETHROW_RC(type, rc) { MIException<type>::SetError(); return(rc); }
#define MI_BEGIN_CATCH(type, value) goto end_catch_##type; catch_##type: { type& value = MIException<type>::GetValue(); value;
#define MI_BEGIN_CATCH_WITH_LABEL(label, type, value) goto end_catch_##label; catch_##label: { type& value = MIException<type>::GetValue(); value;
#define MI_END_CATCH(type) } end_catch_##type: ;
#else
#define MI_CHECK_EXCEPTION(type)
#define MI_CHECK_EXCEPTION_WITH_LABEL(label, type)
#define MI_CHECK_EXCEPTION_IMMEDIATE(type)
#define MI_CHECK_EXCEPTION_IMMEDIATE_RC(type, rc)
#define MI_THROW(type, value) throw value
#define MI_THROW_WITH_LABEL(type, errortype, value) throw value
#define MI_THROW_IMMEDIATE(type, value) throw value
#define MI_THROW_IMMEDIATE_RC(type, value, rc) throw value
#define MI_RETHROW(type) throw
#define MI_RETHROW_RC(type, rc) throw
#define MI_BEGIN_CATCH(type, value) catch(type& value) { value;
#define MI_BEGIN_CATCH_WITH_LABEL(label, errortype, value) catch(errortype& value) { value;
#define MI_END_CATCH(type) }
#endif

#ifdef UNDER_CE
#define MI_CPPRTTI_NOT_SUPPORTED
#endif

    }
#ifdef USING_ELLIS_NAMESPACE
    using namespace Ellis;
#else
    namespace Ellis
    {
    }
#endif
#endif // MIDEFS_H

    // End-of-file
