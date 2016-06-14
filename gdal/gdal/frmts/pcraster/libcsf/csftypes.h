#ifndef INCLUDED_CSFTYPES
# define INCLUDED_CSFTYPES

#ifdef USE_IN_GDAL
#include "cpl_port.h"
#endif


#ifdef __cplusplus
 extern "C" {
 #define CSF_IN_GLOBAL_NS ::
#else
 #define CSF_IN_GLOBAL_NS
#endif

#ifdef CSF_V1
# error include file CSF version 2 used while CSF_V1 is defined
#endif


/* MACHINE AND OPERATING SYSTEM DEPENDENT SECTION:
 * (LOOK AND CONFIGURE YOUR MACHINE/OS)
 *  - Retype all types for file i/o
 *  - Define endian mode
 *  - Compliance mode (POSIX/XOPEN)
 */

#ifdef THINK_C
# error read notes in csftypes.h
  /* Some history:
   *   THINK C 3.0:
   *     - does not know the keyword signed
   *     - uses a 12-byte for double if compiled with
   *       the in-line 68881 option
   */
#endif

 /* the only 64 bit cpu we tested on:
  * -ifdef __alpha
  * don't use long
  * -define CSF_4BYTE_INT_SIZE_SPECIFIER long
  */
#define CSF_4BYTE_INT_SIZE_SPECIFIER

/* some old C++ compiler complains
 * about signed
 */
#ifdef SIGNED_NOT_IMPL
# define CSF_SIGNED_SPECIFIER
#else
# define CSF_SIGNED_SPECIFIER signed
#endif

/* the last-character figure is the
 * size in bytes of the data type
 */

typedef CSF_SIGNED_SPECIFIER char INT1;
typedef unsigned             char UINT1;

#ifdef USE_IN_GDAL
typedef GInt16   INT2;
typedef GInt32   INT4;
typedef GUInt16  UINT2;
typedef GUInt32  UINT4;
#else
typedef CSF_SIGNED_SPECIFIER short                        int  INT2;
typedef CSF_SIGNED_SPECIFIER CSF_4BYTE_INT_SIZE_SPECIFIER int  INT4;
typedef unsigned             short                        int  UINT2;
typedef unsigned             CSF_4BYTE_INT_SIZE_SPECIFIER int  UINT4;
#endif

#ifdef __GNUC__
    /* Modern versions of GCC use knowledge about strict aliasing to implement
     * certain optimizations. We have some code that fails to comply to the
     * strict aliasing rules (see uses of UINT_ALIASING in this header).
     * These are simple cases that should not be optimized. Using
     * UINT_ALIASING will prevent gcc from optimizing these expressions.
     * http://dbp-consulting.com/tutorials/StrictAliasing.html
     * http://ohse.de/uwe/articles/gcc-attributes.html#type-may_alias
     */
    typedef UINT4 __attribute__((__may_alias__)) UINT4_ALIASING;
#else
    typedef UINT4 UINT4_ALIASING;
#endif

#undef CSF_4BYTE_INT_SIZE_SPECIFIER
#undef CSF_SIGNED_SPECIFIER

typedef float               REAL4; /* IEEE-754 32-bit */
typedef double              REAL8; /* IEEE-754 64-bit */


/* endian mode
 * DEFINE WITH -D or find here
 */
#ifndef CPU_BIG_ENDIAN
# ifndef CPU_LITTLE_ENDIAN

#ifdef USE_IN_GDAL
  /* GDAL CPL STYLE */
# ifdef CPL_LSB
#  define CPU_LITTLE_ENDIAN
# endif
# ifdef CPL_MSB
#  define CPU_BIG_ENDIAN
# endif

#else
 /* probe a few: */

#ifdef _AIX
/* IBM AIX defines this on RS/6000 */
# define CPU_BIG_ENDIAN
#endif

#ifdef sparc
/* both cc and gcc defines this in SunOS */
# define CPU_BIG_ENDIAN
#endif

/*
 * #ifdef mips
 * worked once on the SGI machines
 * but mips has both endian architectures
 * # define CPU_BIG_ENDIAN
 * #endif
 */

#ifdef __alpha
/* DEC alpha defines this
 * tested on OSF1 planet V4.0 1229 alpha
 *   in combo with egcs/gcc 2.95.2
 */
#  define CPU_LITTLE_ENDIAN
#endif

#ifdef __i386__
/* linux/gcc defines this on intel 80x86 platform */
#  define CPU_LITTLE_ENDIAN
#endif

#ifdef __x86_64__
/* linux/gcc defines this on intel 80x86_64 platform */
#  define CPU_LITTLE_ENDIAN
#endif
#ifdef _M_X64
/* Mscc defines this on intel 80x86_64 platform */
#  define CPU_LITTLE_ENDIAN
#endif

#ifdef _M_IX86
/* Borland C defines this */
/* Win32/MSC defines this on intel 80x86 platform */
#  define CPU_LITTLE_ENDIAN
#endif


#ifdef __hppa
/* cc and gcc defines this on HP PA risc platform */
#  define CPU_BIG_ENDIAN
#endif

/* endif probing */
# endif

/* endif no ENDIAN defined */
# endif
#endif

/* POSIX or XOPEN compliance
 *  The only need for the POSIX or even XOPEN superset
 *  over ANSI-C are
 *  M_PI    HP-UX complained about that and it seems in XOPEN
 * solution:
 *  M_PI is defined in situ in source files if it is not defined
 *    (most times in math.h)
 * we probe a few systems to find if we may define it
 */

#ifdef __hpux
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE
# endif
#endif

/* END OF MACHINE AND OPERATING SYSTEM DEPENDENT SECTION
 * NO NEED TO EDIT ANYTHING BELOW THIS POINT
 */



/* PROJECTION
 */
/* version 1 types, no longer used
 */
#define PT_XY  0  /* XY-field (= PT_YINCT2B)
                            */
#define PT_UTM 1  /* Universal Transverse Mercator (= PT_YDECT2B)
                            */
#define PT_LATLON 2  /* Latitude / Longitude (= PT_YDECT2B)
                            */
#define PT_CART        3  /* Carthesian (= PT_YDECT2B)
                            */
#define PT_RDM 4  /* Rijks Driehoek Metingen stelsel (= PT_YDECT2B)
                            */
/* the only difference we make is whether Y increases from
 * top to bottom or decreases from top to bottom
 */

typedef enum CSF_PT {

	/* * these two can be returned by or passed to a csf2 function */
	PT_YINCT2B=0,    /* Y increase from top to bottom, wrong do not use */
	PT_YDECT2B=1,    /* Y decrease from top to bottom, correct */

	/* * this one CANNOT be returned by NOR passed to a csf2 function */
	PT_UNDEFINED=100 /* just some value different from the rest */

} CSF_PT;

/* DATATYPE
 * A.K.A. VALUESCALE
 */

/* historical errors don't use them:
 * #define VS_NOTCLASSIFIED 2
 * #define VS_CONTINUES 	 2
 *
 * NOTE new VS_* types must be different from CR_* values
 *      VS_BOOLEAN       0xE0 => 224
 *      VS_NOMINAL       0xE2 => 226
 *      VS_ORDINAL       0xF2 => 242
 *      VS_SCALAR        0xEB => 235
 *      VS_DIRECTION     0xFB => 251
 *      VS_LDD           0xF0 => 240
 *      VS_VECTOR        0xEC => 236
 *      VS_VECTOR        0xEC vector, not used yet
 */

typedef enum CSF_VS {

	/* * version 1 datatypes,
	 * these can be returned by BUT NOT passed to a csf2 function
	 */
        VS_NOTDETERMINED=0, /* version 1  */
        VS_CLASSIFIED   =1, /* version 1  */
        VS_CONTINUOUS 	=2, /* version 1  */

        /* * version 2 datatypes
	 * these two can be returned by or passed to a csf2 function
	 */
        VS_BOOLEAN      =0xE0,/* boolean, always UINT1, values: 0,1 or MV_UINT1 */
        VS_NOMINAL      =0xE2,/* nominal, UINT1 or INT4 */
        VS_ORDINAL      =0xF2,/* ordinal, UINT1 or INT4 */
        VS_SCALAR       =0xEB,/* scalar, REAL4 or (maybe) REAL8 */
        VS_DIRECTION    =0xFB,/* directional REAL4 or (maybe) REAL8, -1 means no direction */
        VS_LDD          =0xF0,/* local drain direction, always UINT1, values: 1-9 or MV_UINT1 */

	/* * this one CANNOT be returned by NOR passed to a csf2 function */
	VS_UNDEFINED    =100 /* just some value different from the rest */

} CSF_VS;


/* CELL REPRESENTATION
   CR_UINT1      0x00 =>   0
   CR_INT4       0x26 =>  38
   CR_REAL4      0x5A =>  90
   CR_INT1	 0x04 =>   4
   CR_INT2	 0x15 =>  21
   CR_UINT2	 0x11 =>  17
   CR_UINT4	 0x22 =>  34
   CR_REAL8	 0xDB => 219
   for vector types
   CR_VECT4	0x??
   CR_VECT8	0x??
 */

typedef enum CSF_CR {

       /* * preferred version 2 cell representations
        */

	CR_UINT1      =0x00, /*  boolean, ldd and small nominal and small ordinal */
	CR_INT4       =0x26, /*  large nominal and large ordinal */
	CR_REAL4      =0x5A, /*  single scalar and single directional */

       /* * other version 2 cell representations
        */

	CR_REAL8      =0xDB, /* double scalar or directional, and also the only type that
	                      * can hold all
	                      * cell representation without loss of precision
	                      */

       /* * version 1 cell representations
	* these can be returned by BUT NOT passed to a csf2 function
        */

	CR_INT1	      =0x04, /* . */
	CR_INT2	      =0x15, /* . */
	CR_UINT2      =0x11, /* . */
	CR_UINT4      =0x22, /* . */

	/* * this one CANNOT be returned by NOR passed to a csf2 function */

        CR_UNDEFINED  =100 /* just some value different from the rest */
} CSF_CR;



/* how to get the cellsize from these type identifiers */
#define CSF_SIZE_MASK 		((size_t)0x03)
#define CSF_SIGN_MASK		((size_t)0x04)
#define CSF_FLOAT_MASK		((size_t)0x08)
#define CSF_FLOAT_SIGN_MASK	((size_t)0x0C)
#define CSF_SIZE_MV_MASK	((size_t)0x30)
#define CSF_SKIP_MASK		((size_t)0xC0)
/* low nibble is uniq for every CR_VALUE: */
#define CSF_UNIQ_MASK	((size_t)0x0F)
#define CSF_POS_SIZE_MV_MASK 	((size_t)4)
#define CSF_POS_SKIP_MASK       ((size_t)6)
#define CSF_UNIQ_CR_MASK(type)  ((size_t)((type) & CSF_UNIQ_MASK))
#define LOG_CELLSIZE(type)  	((size_t)((type) & CSF_SIZE_MASK))
#define CELLSIZE(type)  	((size_t)(1 << LOG_CELLSIZE(type)))
#define CSFSIZEOF(nr, type)	((size_t)(((size_t)nr) << LOG_CELLSIZE(type)))

#include <float.h> /* FLT_MIN, DBL_MAX, DBL_MAX, FLT_MAX */

#define MV_INT1   ((CSF_IN_GLOBAL_NS INT1)-128)
#define MV_INT2   ((CSF_IN_GLOBAL_NS INT2)-32768)
/* cl C4146 has it right
  #define MV_INT4   ((CSF_IN_GLOBAL_NS INT4)-2147483648)
   is dangerous
 */
#define MV_INT4   ((CSF_IN_GLOBAL_NS INT4)0x80000000L)

#define MV_UINT1  ((CSF_IN_GLOBAL_NS UINT1)0xFF)
#define MV_UINT2  ((CSF_IN_GLOBAL_NS UINT2)0xFFFF)
#define MV_UINT4  ((CSF_IN_GLOBAL_NS UINT4)0xFFFFFFFFL)

#define INT2_MIN  ((INT2)(MV_INT2+1))
#define INT2_MAX  ((INT2)0x7FFF)

#define UINT1_MIN ((UINT1)0)
#define UINT1_MAX ((UINT1)(MV_UINT1-1))

#define INT4_MIN   ((INT4)(MV_INT4+1))
#define INT4_MAX   ((INT4)0x7FFFFFFFL)

#define REAL4_MIN  ((REAL4)(FLT_MIN))
#define REAL4_MAX  ((REAL4)(FLT_MAX))

#define REAL8_MIN  ((REAL8)(DBL_MIN))
#define REAL8_MAX  ((REAL8)(DBL_MAX))


/* x is a pointer to a value */
#define IS_MV_UINT1(x)	((*((const CSF_IN_GLOBAL_NS UINT1 *)x)) == MV_UINT1)
#define IS_MV_UINT2(x)	((*((const CSF_IN_GLOBAL_NS UINT2 *)x)) == MV_UINT2)
#define IS_MV_UINT4(x)	((*((const CSF_IN_GLOBAL_NS UINT4 *)x)) == MV_UINT4)
#define IS_MV_INT1(x)	((*((const CSF_IN_GLOBAL_NS INT1 *)x)) == MV_INT1)
#define IS_MV_INT2(x)	((*((const CSF_IN_GLOBAL_NS INT2 *)x)) == MV_INT2)
#define IS_MV_INT4(x)	((*((const CSF_IN_GLOBAL_NS INT4 *)x)) == MV_INT4)

/* MV_REAL4 and MV_REAL8 are bitpatterns with all 1's that
 * are NAN's
 * MV_REAL4 has the same bitpattern as a MV_UINT4
 * MV_REAL8 has the same bitpattern as two MV_UINT4's
 *          only the first 32 bits already identify a NAN,
 *          so that's what we test
 */
#ifdef CPU_LITTLE_ENDIAN
# ifdef CPU_BIG_ENDIAN
#  error CPU_BIG_ENDIAN and CPU_LITTLE_ENDIAN are both defined
# endif
# ifdef INTEL16
#  define IS_MV_REAL4(x) (((const UINT2 *)(x))[1] == MV_UINT2)
#  define IS_MV_REAL8(x) (((const UINT2 *)(x))[3] == MV_UINT2)
# else
#  define IS_MV_REAL4(x) ((*((const CSF_IN_GLOBAL_NS UINT4_ALIASING *)(x))) == MV_UINT4)
#  define IS_MV_REAL8(x) (((const CSF_IN_GLOBAL_NS UINT4_ALIASING *)(x))[1] == MV_UINT4)
# endif
#else
# ifdef CPU_BIG_ENDIAN
#  ifdef CPU_LITTLE_ENDIAN
#   error CPU_BIG_ENDIAN and CPU_LITTLE_ENDIAN are both defined
#  endif
#  define IS_MV_REAL4(x) (((const UINT4 *)(x))[0] == MV_UINT4)
#  define IS_MV_REAL8(x) (((const UINT4 *)(x))[0] == MV_UINT4)
# else
#  error BYTE ORDER NOT SPECIFIED (CPU_LITTLE_ENDIAN or CPU_BIG_ENDIAN)
# endif
#endif


/* some special values
 */
#define LDD_PIT          5
#define DIR_NODIRECTION -1

/* some special macro's
 * x is a pointer
 */
#define SET_MV_UINT1(x)	( (*((UINT1 *)(x))) = MV_UINT1)
#define SET_MV_UINT2(x)	( (*((UINT2 *)(x))) = MV_UINT2)
#define SET_MV_UINT4(x)	( (*((UINT4 *)(x))) = MV_UINT4)
#define SET_MV_INT1(x)	( (*(( INT1 *)(x))) = MV_INT1)
#define SET_MV_INT2(x)	( (*(( INT2 *)(x))) = MV_INT2)
#define SET_MV_INT4(x)	( (*(( INT4 *)(x))) = MV_INT4)
#define	SET_MV_REAL4(x)	((*(CSF_IN_GLOBAL_NS UINT4_ALIASING *)(x)) = MV_UINT4)
#define	SET_MV_REAL8(x)	SET_MV_REAL4((x)),SET_MV_REAL4((((CSF_IN_GLOBAL_NS UINT4 *)(x))+1))

/* copy of floats  by typecasting to
 * an integer since MV_REAL? is a NAN
 */
#define	COPY_REAL4(dest,src) ( (*(UINT4_ALIASING *)(dest)) = (*(const UINT4_ALIASING *)(src)) )
#define	COPY_REAL8(dest,src) COPY_REAL4((dest),(src)),\
		COPY_REAL4( (((UINT4 *)(dest))+1),(((const UINT4 *)(src))+1) )

#ifdef __cplusplus
 }
#endif

#endif
