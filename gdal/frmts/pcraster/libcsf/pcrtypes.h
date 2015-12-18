#ifndef INCLUDED_PCRTYPES
# define INCLUDED_PCRTYPES

#ifndef INCLUDED_CSFTYPES
#include "csftypes.h"
#define INCLUDED_CSFTYPES
#endif

#ifndef INCLUDED_STRING
#include <string>
#define INCLUDED_STRING
#endif

/* NOTE that this file is included in the PCRaster gdal driver
 *      it accounts for more compiler issues than we deal with
 *      normally in PCRaster, such as the VC6 compiler.
 *      64 bit integer typedefs are only defined for the PCRaster
 *      source not for the Gdal driver.
 *      The Gdal PCRaster driver defines USE_IN_GDAL, PCRaster sources
 *      do not.
 */

// memset
// use string.h not cstring
// VC6 does not have memset in std
// better use Ansi-C string.h to be safe
#ifndef INCLUDED_C_STRING
#include <string.h>
#define INCLUDED_C_STRING
#endif

#ifndef USE_IN_GDAL
 // exclude Gdal driver, only for PCRaster sources

# ifdef _MSC_VER
  typedef          __int64    PCR_INT8;
  typedef unsigned __int64    PCR_UINT8;
# else
  // assume gcc FTTB
  typedef          long long  PCR_INT8;
  typedef unsigned long long  PCR_UINT8;
# endif
/* from gcc manual:
  ISO C99 supports data types for integers that are at least 64 bits wide,
  and as an extension GCC supports them in C89 mode and in C++. Simply
  write long long int for a signed integer, or unsigned long long int for
  an unsigned integer. To make an integer constant of type long long int,
  add the suffix `LL' to the integer. To make an integer constant of type
  unsigned long long int, add the suffix `ULL' to the integer.

  WARNING: macros below are not typesafe, use  pcr::isMV and pcr::setMV instead
 */
#define MV_INT8   ((CSF_IN_GLOBAL_NS PCR_INT8)0x8000000000000000LL)
#define MV_UINT8  ((CSF_IN_GLOBAL_NS PCR_UINT8)0xFFFFFFFFFFFFFFFFULL)
#define IS_MV_INT8(x) ((*((const CSF_IN_GLOBAL_NS PCR_INT8 *)x)) == MV_INT8)
#define IS_MV_UINT8(x) ((*((const CSF_IN_GLOBAL_NS PCR_UINT8 *)x)) == MV_UINT8)
#define SET_MV_INT8(x) ( (*((PCR_INT8 *)(x))) = MV_INT8)
#define SET_MV_UINT8(x) ( (*((PCR_UINT8 *)(x))) = MV_UINT8)

#endif

namespace pcr {
/*!
  \brief     Tests if the value v is a missing value.
  \param     v the value to be tested.
  \return    True if value \a v is a missing value.

    The generic isMV(const T& v) is not implemented, only the specializations.

  TODO: Check translation of todo to English.
  \todo      Put all things with a certain type, SMF, set Mv isType in a so-called.
             Struct trait see cast drama as isMV fog INT2 in BandMapTest::Open2.
             See numeric_limit discussion in Josuttis.
*/
  template<typename T> static bool isMV(const T& v);
/*!
  \brief     Tests if the value pointed to by v is a missing value.
  \param     v Pointer to the value to be tested.
  \return    True if the value pointed to by v is a missing value.
*/
  template<typename T> static bool isMV(T* v) {
    return isMV(*v);
  }

# define PCR_DEF_ISMV(type)  \
  template<>                  \
   inline bool isMV(const type& v) \
   { return v == MV_##type; }
   PCR_DEF_ISMV(UINT1)
   PCR_DEF_ISMV(UINT2)
   PCR_DEF_ISMV(UINT4)
   PCR_DEF_ISMV(INT1)
   PCR_DEF_ISMV(INT2)
   PCR_DEF_ISMV(INT4)
#  ifndef USE_IN_GDAL
  template<>
   inline bool isMV(const PCR_UINT8& v)
   { return v == MV_UINT8; }
  template<>
   inline bool isMV(const PCR_INT8& v)
   { return v == MV_INT8; }
#  endif
#  undef PCR_DEF_ISMV
  template<> inline bool isMV(const REAL4& v)
  { return IS_MV_REAL4(&v); }
  template<> inline bool isMV(const REAL8& v)
  { return IS_MV_REAL8(&v); }

template<> inline bool isMV(std::string const& string)
{
  return string.empty();
}

 /*!
    \brief     Sets the value v to a missing value.
    \param     v value to be set.
    the generic setMV(T& v) is not implemented, only the specializations
  */
  template<typename T> void setMV(T& v);
 /*!
    \brief     Sets the value pointed to by v to a missing value.
    \param     v Pointer to the value to be set.
  */
  template<typename T> static void setMV(T *v) {
    setMV(*v);
  }

# define PCR_DEF_SETMV(type)  \
  template<>                  \
   inline void setMV(type& v) \
   { v = MV_##type; }
   PCR_DEF_SETMV(UINT1)
   PCR_DEF_SETMV(UINT2)
   PCR_DEF_SETMV(UINT4)
   PCR_DEF_SETMV(INT1)
   PCR_DEF_SETMV(INT2)
   PCR_DEF_SETMV(INT4)
#  ifndef USE_IN_GDAL
    template<>
     inline void setMV(PCR_UINT8& v)
     { v = MV_UINT8; }
    template<>
     inline void setMV(PCR_INT8& v)
     { v = MV_INT8; }
#  endif
#  undef PCR_DEF_SETMV

  template<>
   inline void setMV(REAL4& v)
  {
#   ifndef __i386__
     SET_MV_REAL4((&v));
#   else
     // this fixes an optimization problem (release mode), if is v is a single
     // element variable in function scope (stack-based)
     // constraint the setting to memory (m)
     // for correct alignment
     asm ("movl $-1, %0" : "=m" (v));
#   endif
  }
  template<>
   inline void setMV(REAL8& v)
  {
#   ifndef __i386__
     SET_MV_REAL8((&v));
#   else
    memset(&v,MV_UINT1,sizeof(REAL8));
    // constraint the setting to memory (m)
    // this fixes the same optimization problem, as for REAL4
    // see com_mvoptest.cc, does not work: !
    // int *v2= (int *)&v;
    // asm ("movl $-1, %[dest]" : [dest] "=m" (v2[0]));
    // asm ("movl $-1, %[dest]" : [dest] "=m" (v2[1]));
#   endif
  }

template<>
inline void setMV(std::string& string)
{
  // VC6 does not have clear
  // string.clear();
  string="";
}

/*! \brief set array \a v of size \a n to all MV's
 *  the generic setMV(T& v) is implemented, the specializations
 *  are optimizations
 * \todo
 *   check if stdlib has a 'wordsized' memset
 *   or optimize for I86, for gcc look into include/asm/string
 */
template<typename T>
static void setMV(T *v, size_t n)
{
  for(size_t i=0; i<n; i++)
      pcr::setMV(v[i]);
}

 namespace detail {
   template<typename T>
    static void setMVMemSet(T *v, size_t n) {
      memset(v,MV_UINT1,n*sizeof(T));
    }
 }

# define PCR_DEF_SETMV_MEMSET(type)    \
  template<>                           \
   inline void setMV(type* v,size_t n) \
   { detail::setMVMemSet(v,n); }
  PCR_DEF_SETMV_MEMSET(UINT1)
  PCR_DEF_SETMV_MEMSET(UINT2)
  PCR_DEF_SETMV_MEMSET(UINT4)
# ifndef USE_IN_GDAL
  PCR_DEF_SETMV_MEMSET(PCR_UINT8)
# endif
  PCR_DEF_SETMV_MEMSET(REAL4)
  PCR_DEF_SETMV_MEMSET(REAL8)
# undef PCR_DEF_SETMV_MEMSET
  template<>
    inline void setMV(INT1 *v, size_t n) {
      memset(v,MV_INT1,n);
    }

//! replace a value equal to \a nonStdMV with the standard MV
/*!
 * \todo
 *   the isMV test is only needed for floats, to protect NAN evaluation
 *   what once happened on bcc/win32. Should reevaluate that.
 */
template<typename T>
  struct AlterToStdMV {
    T d_nonStdMV;
    AlterToStdMV(T nonStdMV):
      d_nonStdMV(nonStdMV) {};

    void operator()(T& v) {
      if (!isMV(v) && v == d_nonStdMV)
        setMV(v);
    }
  };

//! return the value or the standard missing value if value equal to \a nonStdMV
/*!
 * \todo
 *   the isMV test is only needed for floats, to protect NAN evaluation
 *   what once happened on bcc/win32. Should reevaluate that.
 */
template<typename T>
  struct ToStdMV {
    T d_nonStdMV;
    T d_mv;
    ToStdMV(T nonStdMV):
      d_nonStdMV(nonStdMV) {
        setMV(d_mv);
      }

    T operator()(T const& v) {
      if (!isMV(v) && v == d_nonStdMV) {
        return d_mv;
      }
      return v;
    }
  };

//! replace the standard MV with a value  equal to \a otherMV
/*!
 * \todo
 *   the isMV test is only needed for floats, to protect NAN evaluation
 *   what once happened on bcc/win32. Should reevaluate that.
 */
template<typename T>
  struct AlterFromStdMV {
    T d_otherMV;
    AlterFromStdMV(T otherMV):
      d_otherMV(otherMV) {};

    void operator()(T& v) {
      if (isMV(v))
        v = d_otherMV;
    }
  };

//! return the value or \a otherMV if value equal to standard MV
/*!
 * \todo
 *   the isMV test is only needed for floats, to protect NAN evaluation
 *   what once happened on bcc/win32. Should reevaluate that.
 */
template<typename T>
  struct FromStdMV {
    T d_otherMV;
    FromStdMV(T otherMV):
      d_otherMV(otherMV) {};

    T operator()(const T& v) {
      if (isMV(v))
        return d_otherMV;
      return v;
    }
  };

}


#endif
