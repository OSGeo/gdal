/*
Copyright (C) 1996 Hughes and Applied Research Corporation

Permission to use, modify, and distribute this software and its documentation 
for any purpose without fee is hereby granted, provided that the above 
copyright notice appear in all copies and that both that copyright notice and 
this permission notice appear in supporting documentation.
*/


/* cfortran.h */ /* 2.8 */            /* anonymous ftp: zebra.desy.de */
/* Burkhard Burow, burow@vxdesy.cern.ch, University of Toronto, 1993. */


#ifndef __CFORTRAN_LOADED
#define __CFORTRAN_LOADED

/* 
   THIS FILE IS PROPERTY OF BURKHARD BUROW. IF YOU ARE USING THIS FILE YOU
   SHOULD ALSO HAVE ACCESS TO CFORTRAN.DOC WHICH PROVIDES TERMS FOR USING,
   MODIFYING, COPYING AND DISTRIBUTING THE CFORTRAN.H PACKAGE.
*/

/* Before using cfortran.h on CRAY, RS/6000, Apollo >=6.8, gcc -ansi,
   or any other ANSI C compiler, you must once do:
prompt> mv cfortran.h cf_temp.h && sed 's/\/\*\*\//\#\#/g' cf_temp.h >cfortran.h
   i.e. we change the ' / * * / ' kludge to # #. */

/* First prepare for the C compiler. */

#if (defined(vax)&&defined(unix)) || (defined(__vax__)&&defined(__unix__))
#define VAXUltrix
#endif

#include <stdio.h>     /* NULL [in all machines stdio.h]                      */
#include <string.h>    /* strlen, memset, memcpy, memchr.                     */
#if !( defined(VAXUltrix) || defined(sun) || (defined(apollo)&&!defined(__STDCPP__)) )
#include <stdlib.h>    /* malloc,free                                         */
#else
#include <malloc.h>    
#ifdef apollo
#define __CF__APOLLO67 /* __STDCPP__ is in Apollo 6.8 (i.e. ANSI) and onwards */
#endif
#endif

#if (!defined(__GNUC__) && (defined(sun)||defined(VAXUltrix)||defined(lynx)))
#define __CF__KnR     /* Sun, LynxOS and VAX Ultrix cc only supports K&R.     */
                      /* Manually define __CF__KnR for HP if desired/required.*/
#endif                /*       i.e. We will generate Kernighan and Ritchie C. */
/* Note that you may define __CF__KnR before #include cfortran.h, in order to
generate K&R C instead of the default ANSI C. The differences are mainly in the
function prototypes and declarations. All machines, except the Apollo, work
with either style. The Apollo's argument promotion rules require ANSI or use of
the obsolete std_$call which we have not implemented here. Hence on the Apollo,
only C calling FORTRAN subroutines will work using K&R style.*/


/* Remainder of cfortran.h depends on the Fortran compiler. */

/* VAX/VMS does not let us \-split these long lines. */ 
#if !(defined(NAGf90Fortran)||defined(f2cFortran)||defined(hpuxFortran)||defined(apolloFortran)||defined(sunFortran)||defined(IBMR2Fortran)||defined(CRAYFortran)||defined(mipsFortran)||defined(DECFortran)||defined(vmsFortran))
/* If no Fortran compiler is given, we choose one for the machines we know.   */
#if defined(lynx) || defined(VAXUltrix)
#define f2cFortran    /* Lynx:      Only support f2c at the moment.
                         VAXUltrix: f77 behaves like f2c.
                           Support f2c or f77 with gcc, vcc with f2c. 
                           f77 with vcc works, missing link magic for f77 I/O.*/
#endif
#if defined(__hpux)       /* 921107: Use __hpux instead of __hp9000s300 */
#define       hpuxFortran /*         Should also allow hp9000s7/800 use.*/
#endif
#if       defined(apollo)
#define           apolloFortran  /* __CF__APOLLO67 defines some behavior. */
#endif
#if          defined(sun)
#define              sunFortran
#endif
#if       defined(_IBMR2)
#define            IBMR2Fortran
#endif
#if        defined(_CRAY)
#define             CRAYFortran  /* _CRAY2         defines some behavior. */
#endif
#if         defined(mips) || defined(__mips)
#define             mipsFortran
#endif
#if          defined(vms) || defined(__vms)
#define              vmsFortran
#endif
#if      defined(__alpha) && defined(__unix__)
#define              DECFortran
#endif
#endif /* ...Fortran */



#if defined(VAXC) && !defined(__VAXC)
#define OLD_VAXC
#pragma nostandard                       /* Prevent %CC-I-PARAMNOTUSED.       */
#endif

/* Throughout cfortran.h we use: UN = Uppercase Name.  LN = Lowercase Name.  */

#if defined(f2cFortran) || defined(NAGf90Fortran) || defined(DECFortran) || defined(mipsFortran) || defined(apolloFortran) || defined(sunFortran) || defined(extname)
#if defined(f2cFortran)
#define CFC_(UN,LN)            LN##_  /* Lowercase FORTRAN symbols.        */
#else
#define CFC_(UN,LN)            LN##_   /* Lowercase FORTRAN symbols.        */
#endif   /* f2cFortran */
#define orig_fcallsc           CFC_
#else 
#ifdef CRAYFortran
#define CFC_(UN,LN)            UN        /* Uppercase FORTRAN symbols.        */
#define orig_fcallsc(UN,LN)    CFC_(UN,LN)  /* CRAY insists on arg.'s here.   */
#else  /* For following machines one may wish to change the fcallsc default.  */
#define CF_SAME_NAMESPACE
#ifdef vmsFortran
#define CFC_(UN,LN)            LN        /* Either case FORTRAN symbols.      */
     /* BUT we usually use UN for C macro to FORTRAN routines, so use LN here,*/
     /* because VAX/VMS doesn't do recursive macros.                          */
#define orig_fcallsc(UN,LN)    UN      
#else      /* HP-UX without +ppu or IBMR2 without -qextname. NOT reccomended. */
#define CFC_(UN,LN)            LN        /* Lowercase FORTRAN symbols.        */
#define orig_fcallsc           CFC_
#endif /*  vmsFortran */
#endif /* CRAYFortran */
#endif /* ....Fortran */

#define fcallsc                      orig_fcallsc
#define preface_fcallsc(P,p,UN,LN)   CFC_(P##UN,p##LN)
#define  append_fcallsc(P,p,UN,LN)   CFC_(UN##P,LN##p)

#define C_FUNCTION                   fcallsc      
#define FORTRAN_FUNCTION             CFC_
#define COMMON_BLOCK                 CFC_

#if defined(NAGf90Fortran) || defined(f2cFortran) || defined(mipsFortran)
#define LOGICAL_STRICT      /* These have .eqv./.neqv. == .eq./.ne.   */
#endif

#ifdef CRAYFortran
#if _CRAY
#include <fortran.h>
#else
#include "fortran.h"  /* i.e. if crosscompiling assume user has file. */
#endif
#define DOUBLE_PRECISION long double
#define PPFLOATVVVVVVV (float *)   /* Used for C calls FORTRAN.               */
/* CRAY's double==float but CRAY says pointers to doubles and floats are diff.*/
#define VOIDP0  (void *)  /* When FORTRAN calls C, we don't know if C routine 
                            arg.'s have been declared float *, or double *.   */
#else
#define DOUBLE_PRECISION double
#define PPFLOATVVVVVVV
#define VOIDP0
#endif

#ifdef vmsFortran
#if    defined(vms) || defined(__vms)
#include <descrip.h>
#else
#include "descrip.h"  /* i.e. if crosscompiling assume user has file. */
#endif
#endif

#ifdef sunFortran
#if    sun
#include <math.h>     /* Sun's FLOATFUNCTIONTYPE, ASSIGNFLOAT, RETURNFLOAT.  */
#else
#include "math.h"     /* i.e. if crosscompiling assume user has file. */
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef apolloFortran
#define COMMON_BLOCK_DEF(DEFINITION, NAME) extern DEFINITION NAME
#define CF_NULL_PROTO
#else                                         /* HP doesn't understand #elif. */
/* Without ANSI prototyping, Apollo promotes float functions to double.    */
/* Note that VAX/VMS, IBM, Mips choke on 'type function(...);' prototypes. */
#define CF_NULL_PROTO ...
#ifndef __CF__APOLLO67
#define COMMON_BLOCK_DEF(DEFINITION, NAME) \
 DEFINITION NAME __attribute((__section(NAME)))
#else
#define COMMON_BLOCK_DEF(DEFINITION, NAME) \
 DEFINITION NAME #attribute[section(NAME)]
#endif
#endif

#ifdef mipsFortran
#define CF_DECLARE_GETARG         int f77argc; char **f77argv
#define CF_SET_GETARG(ARGC,ARGV)  f77argc = ARGC; f77argv = ARGV
#else
#define CF_DECLARE_GETARG
#define CF_SET_GETARG(ARGC,ARGV)
#endif

#ifdef OLD_VAXC                          /* Allow %CC-I-PARAMNOTUSED.         */
#pragma standard                         
#endif

#define ACOMMA ,
#define ACOLON ;

/*-------------------------------------------------------------------------*/

/*               UTILITIES USED WITHIN CFORTRAN.H                          */

#define PGSMIN(A,B) (A<B?A:B)
#define firstindexlength( A) (sizeof(A)     /sizeof(A[0]))
#define secondindexlength(A) (sizeof((A)[0])/sizeof((A)[0][0]))
#ifndef FALSE
#define FALSE (1==0)
#endif

/* Behavior of FORTRAN LOGICAL. All machines' LOGICAL is same size as C's int.
Conversion is automatic except for arrays which require F2CLOGICALV/C2FLOGICALV.
f2c, MIPS f77 [DECstation, SGI], VAX Ultrix f77, CRAY-2, HP-UX f77:  as in C.
VAX/VMS FORTRAN, VAX Ultrix fort, IBM RS/6000 xlf: LS Bit = 0/1 = TRUE/FALSE.
Apollo, non CRAY-2                               : neg.   = TRUE, else FALSE. 
[Apollo accepts -1 as TRUE for function values, but NOT all other neg. values.]
[DECFortran for Ultrix RISC is also called f77 but is the same as VAX/VMS.]   
[MIPS f77 treats .eqv./.neqv. as .eq./.ne. and hence requires LOGICAL_STRICT.]*/

#define C2FLOGICALV(A,I) \
 do {int __i; for(__i=0;__i<I;__i++) A[__i]=C2FLOGICAL(A[__i]); } while (FALSE)
#define F2CLOGICALV(A,I) \
 do {int __i; for(__i=0;__i<I;__i++) A[__i]=F2CLOGICAL(A[__i]); } while (FALSE)

#if defined(apolloFortran) || (defined(CRAYFortran) && !defined(_CRAY2))
#ifndef apolloFortran
#define C2FLOGICAL(L) ((L)?(L)|(1<<sizeof(int)*8-1):(L)&~(1<<sizeof(int)*8-1))
#else
#define C2FLOGICAL(L) ((L)?-1:(L)&~(1<<sizeof(int)*8-1)) /* Apollo Exception  */
#endif
#define F2CLOGICAL(L) ((L)<0?(L):0) 
#else
#if defined(IBMR2Fortran) || defined(vmsFortran) || defined(DECFortran)
#define C2FLOGICAL(L) ((L)?(L)|1:(L)&~(int)1)
#define F2CLOGICAL(L) ((L)&1?(L):0)
#else /* all other machines evaluate LOGICALs as C does. */
#define C2FLOGICAL(L) (L)
#define F2CLOGICAL(L) (L)
#ifndef LOGICAL_STRICT
#undef  C2FLOGICALV
#undef  F2CLOGICALV
#define C2FLOGICALV(A,I)
#define F2CLOGICALV(A,I)
#endif  /* LOGICAL_STRICT */
#endif
#endif

#ifdef LOGICAL_STRICT
/* Force C2FLOGICAL to generate only the values for either .TRUE. or .FALSE.
   This is only needed if you want to do:
     logical lvariable
     if (lvariable .eq.  .true.) then       ! (1)
   instead of
     if (lvariable .eqv. .true.) then       ! (2)
   - (1) may not even be FORTRAN/77 and that Apollo's f77 and IBM's xlf
   refuse to compile (1), so you are probably well advised to stay away from 
   (1) and from LOGICAL_STRICT.
   - You pay a (slight) performance penalty for using LOGICAL_STRICT. */
#undef C2FLOGICAL
#if defined(apolloFortran) || (defined(CRAYFortran) && !defined(_CRAY2)) || defined(vmsFortran) || defined(DECFortran)
#define C2FLOGICAL(L) ((L)?-1:0) /* These machines use -1/0 for .true./.false.*/
#else
#define C2FLOGICAL(L) ((L)? 1:0) /* All others     use +1/0 for .true./.false.*/
#endif
#endif /* LOGICAL_STRICT */

/* Convert a vector of C strings into FORTRAN strings. */
#ifndef __CF__KnR
static char *c2fstrv(char* cstr, char *fstr, int elem_len, int sizeofcstr)
#else
static char *c2fstrv(      cstr,       fstr,     elem_len,     sizeofcstr)
                     char* cstr; char *fstr; int elem_len; int sizeofcstr;
#endif
{ int i,j;
/* elem_len includes \0 for C strings. Fortran strings don't have term. \0.
   Useful size of string must be the same in both languages. */
for (i=0; i<sizeofcstr/elem_len; i++) {
  for (j=1; j<elem_len && *cstr; j++) *fstr++ = *cstr++;
  cstr += 1+elem_len-j;
  for (; j<elem_len; j++) *fstr++ = ' ';
} return fstr-sizeofcstr+sizeofcstr/elem_len; }

/* Convert a vector of FORTRAN strings into C strings. */
#ifndef __CF__KnR
static char *f2cstrv(char *fstr, char* cstr, int elem_len, int sizeofcstr)
#else
static char *f2cstrv(      fstr,       cstr,     elem_len,     sizeofcstr)
                     char *fstr; char* cstr; int elem_len; int sizeofcstr; 
#endif
{ int i,j;
/* elem_len includes \0 for C strings. Fortran strings don't have term. \0.
   Useful size of string must be the same in both languages. */
cstr += sizeofcstr;
fstr += sizeofcstr - sizeofcstr/elem_len;
for (i=0; i<sizeofcstr/elem_len; i++) {
  *--cstr = '\0';
  for (j=1; j<elem_len; j++) *--cstr = *--fstr;
} return cstr; }

/* kill the trailing char t's in string s. */
#ifndef __CF__KnR
static char *kill_trailing(char *s, char t)
#else
static char *kill_trailing(      s,      t) char *s; char t;
#endif
{char *e; 
e = s + strlen(s);
if (e>s) {                           /* Need this to handle NULL string.*/
  while (e>s && *--e==t);            /* Don't follow t's past beginning. */
  e[*e==t?0:1] = '\0';               /* Handle s[0]=t correctly.       */
} return s; }

/* kill_trailingn(s,t,e) will kill the trailing t's in string s. e normally 
points to the terminating '\0' of s, but may actually point to anywhere in s.
s's new '\0' will be placed at e or earlier in order to remove any trailing t's.
If e<s string s is left unchanged. */ 
#ifndef __CF__KnR
static char *kill_trailingn(char *s, char t, char *e)
#else
static char *kill_trailingn(      s,      t,       e) char *s; char t; char *e;
#endif
{ 
if (e==s) *e = '\0';                 /* Kill the string makes sense here.*/
else if (e>s) {                      /* Watch out for neg. length string.*/
  while (e>s && *--e==t);            /* Don't follow t's past beginning. */
  e[*e==t?0:1] = '\0';               /* Handle s[0]=t correctly.       */
} return s; }

/* Note the following assumes that any element which has t's to be chopped off,
does indeed fill the entire element. */
#ifndef __CF__KnR
static char *vkill_trailing(char* cstr, int elem_len, int sizeofcstr, char t)
#else
static char *vkill_trailing(      cstr,     elem_len,     sizeofcstr,      t)
                            char* cstr; int elem_len; int sizeofcstr; char t;
#endif
{ int i;
for (i=0; i<sizeofcstr/elem_len; i++) /* elem_len includes \0 for C strings. */
  kill_trailingn(cstr+elem_len*i,t,cstr+elem_len*(i+1)-1);
return cstr; }

#ifdef vmsFortran
typedef struct dsc$descriptor_s fstring;
#define DSC$DESCRIPTOR_A(DIMCT)  		                               \
struct {                                                                       \
  unsigned short dsc$w_length;	        unsigned char	 dsc$b_dtype;	       \
  unsigned char	 dsc$b_class;	                 char	*dsc$a_pointer;	       \
           char	 dsc$b_scale;	        unsigned char	 dsc$b_digits;         \
  struct {                                                                     \
    unsigned		       : 3;	  unsigned dsc$v_fl_binscale : 1;      \
    unsigned dsc$v_fl_redim    : 1;       unsigned dsc$v_fl_column   : 1;      \
    unsigned dsc$v_fl_coeff    : 1;       unsigned dsc$v_fl_bounds   : 1;      \
  } dsc$b_aflags;	                                                       \
  unsigned char	 dsc$b_dimct;	        unsigned long	 dsc$l_arsize;	       \
           char	*dsc$a_a0;	                 long	 dsc$l_m [DIMCT];      \
  struct {                                                                     \
    long dsc$l_l;                         long dsc$l_u;                        \
  } dsc$bounds [DIMCT];                                                        \
}
typedef DSC$DESCRIPTOR_A(1) fstringvector;
/*typedef DSC$DESCRIPTOR_A(2) fstringarrarr;
  typedef DSC$DESCRIPTOR_A(3) fstringarrarrarr;*/
#define initfstr(F,C,ELEMNO,ELEMLEN)                                           \
( (F).dsc$l_arsize=  ( (F).dsc$w_length                        =(ELEMLEN) )    \
                    *( (F).dsc$l_m[0]=(F).dsc$bounds[0].dsc$l_u=(ELEMNO)  ),   \
  (F).dsc$a_a0    =  ( (F).dsc$a_pointer=(C) ) - (F).dsc$w_length          ,(F))

#else
#define _NUM_ELEMS      -1
#define _NUM_ELEM_ARG   -2
#define NUM_ELEMS(A)    A,_NUM_ELEMS
#define NUM_ELEM_ARG(B) *A##B,_NUM_ELEM_ARG
#define TERM_CHARS(A,B) A,B
#ifndef __CF__KnR
static int num_elem(char *strv, unsigned elem_len, int term_char, int num_term)
#else
static int num_elem(      strv,          elem_len,     term_char,     num_term)
                    char *strv; unsigned elem_len; int term_char; int num_term;
#endif
/* elem_len is the number of characters in each element of strv, the FORTRAN
vector of strings. The last element of the vector must begin with at least
num_term term_char characters, so that this routine can determine how 
many elements are in the vector. */
{
unsigned num,i;
if (num_term == _NUM_ELEMS || num_term == _NUM_ELEM_ARG) 
  return term_char;
if (num_term <=0) num_term = elem_len;
for (num=0; ; num++) {
  for (i=0; (int) i<num_term && *strv==(char) term_char; i++,strv++);
  if ((int) i==num_term) break;
  else strv += elem_len-i;
}
return num;
}
#endif
/*-------------------------------------------------------------------------*/

/*           UTILITIES FOR C TO USE STRINGS IN FORTRAN COMMON BLOCKS       */

/* C string TO Fortran Common Block STRing. */
/* DIM is the number of DIMensions of the array in terms of strings, not
   characters. e.g. char a[12] has DIM = 0, char a[12][4] has DIM = 1, etc. */
#define C2FCBSTR(CSTR,FSTR,DIM)                                                \
 c2fstrv((char *)CSTR, (char *)FSTR, sizeof(FSTR)/cfelementsof(FSTR,DIM)+1,    \
         sizeof(FSTR)+cfelementsof(FSTR,DIM))

/* Fortran Common Block string TO C STRing. */
#define FCB2CSTR(FSTR,CSTR,DIM)                                                \
 vkill_trailing(f2cstrv((char *)FSTR, (char *)CSTR,                            \
                        sizeof(FSTR)/cfelementsof(FSTR,DIM)+1,                 \
                        sizeof(FSTR)+cfelementsof(FSTR,DIM)),                  \
                sizeof(FSTR)/cfelementsof(FSTR,DIM)+1,                         \
                sizeof(FSTR)+cfelementsof(FSTR,DIM), ' ')

#define cfDEREFERENCE0
#define cfDEREFERENCE1 *
#define cfDEREFERENCE2 **
#define cfDEREFERENCE3 ***
#define cfDEREFERENCE4 ****
#define cfDEREFERENCE5 *****
#define cfelementsof(A,D) (sizeof(A)/sizeof(cfDEREFERENCE##D(A)))

/*-------------------------------------------------------------------------*/

/*               UTILITIES FOR C TO CALL FORTRAN SUBROUTINES               */

/* Define lookup tables for how to handle the various types of variables.  */

#ifdef OLD_VAXC                                /* Prevent %CC-I-PARAMNOTUSED. */
#pragma nostandard
#endif

static int __cfztringv[30];       /* => 30 == MAX # of arg.'s C can pass to a */
#define ZTRINGV_NUM(I) I          /*          FORTRAN function.               */
#define ZTRINGV_ARGF(I) __cfztringv[I]
#define ZTRINGV_ARGS(I) B##I

#define VPPBYTE         VPPINT
#define VPPDOUBLE       VPPINT
#define VPPFLOAT        VPPINT
#define VPPINT(    A,B) int  B = (int)A;   /* For ZSTRINGV_ARGS */
#define VPPLOGICAL(A,B) int *B;         /* Returning LOGICAL in FUNn and SUBn.*/
#define VPPLONG         VPPINT
#define VPPSHORT        VPPINT

#define VCF(TN,I)       _INT(3,V,TN,A##I,B##I)
#define VVCF(TN,AI,BI)  _INT(3,V,TN,AI,BI)
#define VINT(       T,A,B) typeP##T##VVVVVVV B = A;
#define VINTV(      T,A,B)
#define VINTVV(     T,A,B)
#define VINTVVV(    T,A,B)
#define VINTVVVV(   T,A,B)
#define VINTVVVVV(  T,A,B)
#define VINTVVVVVV( T,A,B)
#define VINTVVVVVVV(T,A,B)
#define VPINT(      T,A,B) VP##T(A,B)
#define VPVOID(     T,A,B)
#ifdef apolloFortran
#define VROUTINE(   T,A,B) void (*B)() = (void (*)())A;
#else
#define VROUTINE(   T,A,B)
#endif
#define VSIMPLE(    T,A,B)
#ifdef vmsFortran
#define VSTRING(    T,A,B) static struct {fstring f; unsigned clen;} B =       \
                                       {{0,DSC$K_DTYPE_T,DSC$K_CLASS_S,NULL},0};
#define VPSTRING(   T,A,B) static fstring B={0,DSC$K_DTYPE_T,DSC$K_CLASS_S,NULL};
#define VSTRINGV(   T,A,B) static fstringvector B =                            \
  {sizeof(A),DSC$K_DTYPE_T,DSC$K_CLASS_A,NULL,0,0,{0,0,1,1,1},1,0,NULL,0,{1,0}};
#define VPSTRINGV(  T,A,B) static fstringvector B =                            \
          {0,DSC$K_DTYPE_T,DSC$K_CLASS_A,NULL,0,0,{0,0,1,1,1},1,0,NULL,0,{1,0}};
#else
#define VSTRING(    T,A,B) struct {unsigned short clen, flen;} B;
#define VSTRINGV(   T,A,B) struct {char *s, *fs; unsigned flen;} B;
#define VPSTRING(   T,A,B) int     B;
#define VPSTRINGV(  T,A,B) struct {char *fs; unsigned short sizeofA, flen;} B;
#endif
#define VZTRINGV         VSTRINGV
#define VPZTRINGV        VPSTRINGV

/* Note that the actions of the A table were performed inside the AA table.
   VAX Ultrix vcc, and HP-UX cc, didn't evaluate arguments to functions left to
   right, so we had to split the original table into the current robust two. */
#define ACF(NAME,TN,AI,I)  STR_##TN(4,A,NAME,I,AI,B##I)
#define ALOGICAL( M,I,A,B) B=C2FLOGICAL(B);
#define APLOGICAL(M,I,A,B) A=C2FLOGICAL(A);
#define ASTRING(  M,I,A,B) CSTRING(A,B,sizeof(A))
#define APSTRING( M,I,A,B) CPSTRING(A,B,sizeof(A))
#ifdef vmsFortran
#define AATRINGV( M,I,A,B, sA,filA,silA)                                       \
 initfstr(B,malloc((sA)-(filA)),(filA),(silA)-1),                              \
          c2fstrv(A[0],B.dsc$a_pointer,(silA),(sA));
#define APATRINGV(M,I,A,B, sA,filA,silA)                                       \
 initfstr(B,A[0],(filA),(silA)-1),c2fstrv(A[0],A[0],(silA),(sA));
#else
#define AATRINGV( M,I,A,B, sA,filA,silA)                                       \
 (B.s=malloc((sA)-(filA)),B.fs=c2fstrv(A[0],B.s,(B.flen=(silA)-1)+1,(sA)));
#define APATRINGV(M,I,A,B, sA,filA,silA)                                       \
 B.fs=c2fstrv(A[0],A[0],(B.flen=(silA)-1)+1,B.sizeofA=(sA));
#endif
#define ASTRINGV( M,I,A,B)                                                     \
  AATRINGV( M,I,A,B,sizeof(A),firstindexlength(A),secondindexlength(A)) 
#define APSTRINGV(M,I,A,B)                                                     \
 APATRINGV( M,I,A,B,sizeof(A),firstindexlength(A),secondindexlength(A)) 
#define AZTRINGV( M,I,A,B) AATRINGV( M,I,A,B,                                  \
                    (M##_ELEMS_##I)*(( M##_ELEMLEN_##I)+1),            \
                              (M##_ELEMS_##I),(M##_ELEMLEN_##I)+1) 
#define APZTRINGV(M,I,A,B) APATRINGV( M,I,A,B,                                 \
                    (M##_ELEMS_##I)*(( M##_ELEMLEN_##I)+1),            \
                              (M##_ELEMS_##I),(M##_ELEMLEN_##I)+1) 

#define AAPPBYTE(   A,B) &A
#define AAPPDOUBLE( A,B) &A
#define AAPPFLOAT(  A,B) PPFLOATVVVVVVV &A
#define AAPPINT(    A,B) &A
#define AAPPLOGICAL(A,B) B= &A         /* B used to keep a common W table. */
#define AAPPLONG(   A,B) &A
#define AAPPSHORT(  A,B) &A

#define AACF(TN,AI,I,C) _SEP_(TN,C,COMMA) _INT(3,AA,TN,AI,B##I)
#define AAINT(       T,A,B) &B
#define AAINTV(      T,A,B) PP##T##VVVVVV A
#define AAINTVV(     T,A,B) PP##T##VVVVV  A[0]
#define AAINTVVV(    T,A,B) PP##T##VVVV   A[0][0]
#define AAINTVVVV(   T,A,B) PP##T##VVV    A[0][0][0]
#define AAINTVVVVV(  T,A,B) PP##T##VV     A[0][0][0][0]
#define AAINTVVVVVV( T,A,B) PP##T##V      A[0][0][0][0][0]
#define AAINTVVVVVVV(T,A,B) PP##T           A[0][0][0][0][0][0]
#define AAPINT(      T,A,B) AAP##T(A,B)
#define AAPVOID(     T,A,B) (void *) A
#ifdef apolloFortran
#define AAROUTINE(   T,A,B) &B
#else
#define AAROUTINE(   T,A,B)  (void(*)())A
#endif
#define AASTRING(    T,A,B) CCSTRING(T,A,B)
#define AAPSTRING(   T,A,B) CCPSTRING(T,A,B)
#ifdef vmsFortran
#define AASTRINGV(   T,A,B) &B
#else
#ifdef CRAYFortran
#define AASTRINGV(   T,A,B) _cptofcd(B.fs,B.flen)
#else
#define AASTRINGV(   T,A,B) B.fs
#endif
#endif
#define AAPSTRINGV      AASTRINGV
#define AAZTRINGV       AASTRINGV
#define AAPZTRINGV      AASTRINGV

#if defined(vmsFortran) || defined(CRAYFortran)
#define JCF(TN,I)
#else
#define JCF(TN,I)    STR_##TN(1,J,B##I, 0,0,0)
#define JLOGICAL( B)
#define JPLOGICAL(B)
#define JSTRING(  B) ,B.flen
#define JPSTRING( B) ,B
#define JSTRINGV     JSTRING
#define JPSTRINGV    JSTRING
#define JZTRINGV     JSTRING
#define JPZTRINGV    JSTRING
#endif

#define WCF(TN,AN,I)   STR_##TN(2,W,AN,B##I, 0,0)
#define WLOGICAL( A,B)
#define WPLOGICAL(A,B) *B=F2CLOGICAL(*B);
#define WSTRING(  A,B) (A[B.clen]!='\0'?A[B.clen]='\0':0); /* A?="constnt"*/
#define WPSTRING( A,B) kill_trailing(A,' ');
#ifdef vmsFortran
#define WSTRINGV( A,B) free(B.dsc$a_pointer);
#define WPSTRINGV(A,B)                                                         \
  vkill_trailing(f2cstrv((char*)A, (char*)A,                                   \
                           B.dsc$w_length+1, B.dsc$l_arsize+B.dsc$l_m[0]),     \
                   B.dsc$w_length+1, B.dsc$l_arsize+B.dsc$l_m[0], ' ');
#else
#define WSTRINGV( A,B) free(B.s);
#define WPSTRINGV(A,B) vkill_trailing(                                         \
         f2cstrv((char*)A,(char*)A,B.flen+1,B.sizeofA), B.flen+1,B.sizeofA,' ');
#endif
#define WZTRINGV           WSTRINGV
#define WPZTRINGV          WPSTRINGV

#define   NCF(TN,I,C)  _SEP_(TN,C,COMMA) _INT(2,N,TN,A##I,0) 
#define  NNCF          UUCF
#define NNNCF(TN,I,C)  _SEP_(TN,C,COLON) _INT(2,N,TN,A##I,0) 
#define NINT(       T,A) typeP##T##VVVVVVV * A
#define NINTV(      T,A) typeP##T##VVVVVV  * A
#define NINTVV(     T,A) typeP##T##VVVVV   * A
#define NINTVVV(    T,A) typeP##T##VVVV    * A
#define NINTVVVV(   T,A) typeP##T##VVV     * A
#define NINTVVVVV(  T,A) typeP##T##VV      * A
#define NINTVVVVVV( T,A) typeP##T##V       * A
#define NINTVVVVVVV(T,A) typeP##T            * A
#define NPINT(      T,A)  type##T##VVVVVVV * A
#define NPVOID(     T,A) void *                  A
#ifdef apolloFortran
#define NROUTINE(   T,A) void (**A)()
#else
#define NROUTINE(   T,A) void ( *A)()
#endif
#ifdef vmsFortran
#define NSTRING(    T,A) fstring *          A
#define NSTRINGV(   T,A) fstringvector *    A
#else
#ifdef CRAYFortran
#define NSTRING(    T,A) _fcd               A
#define NSTRINGV(   T,A) _fcd               A
#else
#define NSTRING(    T,A) char *             A
#define NSTRINGV(   T,A) char *             A
#endif
#endif
#define NPSTRING(   T,A) NSTRING(T,A)   /* CRAY insists on arg.'s here. */
#define NPNSTRING(  T,A) NSTRING(T,A)   /* CRAY insists on arg.'s here. */
#define NPPSTRING(  T,A) NSTRING(T,A)   /* CRAY insists on arg.'s here. */
#define NSTRVOID(    T,A) NSTRING(T,A)   /* CRAY insists on arg.'s here. */
#define NPSTRINGV(  T,A) NSTRINGV(T,A)
#define NZTRINGV(   T,A) NSTRINGV(T,A)
#define NPZTRINGV(  T,A) NPSTRINGV(T,A)

/* Note: Prevent compiler warnings, null #define PROTOCCALLSFSUB14/20 after 
   #include-ing cfortran.h if calling the FORTRAN wrapper within the same 
   source code where the wrapper is created. */
#ifndef __CF__KnR
#define PROTOCCALLSFSUB0(UN,LN)          extern void CFC_(UN,LN)();
#define PROTOCCALLSFSUB14(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE)     \
 extern void CFC_(UN,LN)(NCF(T1,1,0) NCF(T2,2,1) NCF(T3,3,1) NCF(T4,4,1)       \
 NCF(T5,5,1) NCF(T6,6,1) NCF(T7,7,1) NCF(T8,8,1) NCF(T9,9,1) NCF(TA,A,1)       \
 NCF(TB,B,1) NCF(TC,C,1) NCF(TD,D,1) NCF(TE,E,1) ,...);
#define PROTOCCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,TI,TJ,TK)\
 extern void CFC_(UN,LN)(NCF(T1,1,0) NCF(T2,2,1) NCF(T3,3,1) NCF(T4,4,1)       \
 NCF(T5,5,1) NCF(T6,6,1) NCF(T7,7,1) NCF(T8,8,1) NCF(T9,9,1) NCF(TA,A,1)       \
 NCF(TB,B,1) NCF(TC,C,1) NCF(TD,D,1) NCF(TE,E,1) NCF(TF,F,1) NCF(TG,G,1)       \
 NCF(TH,H,1) NCF(TI,I,1) NCF(TJ,J,1) NCF(TK,K,1) ,...);
#else
#define PROTOCCALLSFSUB0( UN,LN)
#define PROTOCCALLSFSUB14(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE)
#define PROTOCCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,TI,TJ,TK)
#endif

#ifdef OLD_VAXC                                  /* Allow %CC-I-PARAMNOTUSED. */
#pragma standard
#endif

/* do{...}while(FALSE) allows if(a==b) FORT(); else BORT(); */

#define CCALLSFSUB0(UN,LN) \
 do{PROTOCCALLSFSUB0(UN,LN) CFC_(UN,LN)();}while(FALSE)

#define CCALLSFSUB1( UN,LN,T1,                        A1)         \
        CCALLSFSUB5 (UN,LN,T1,CF_0,CF_0,CF_0,CF_0,A1,0,0,0,0)
#define CCALLSFSUB2( UN,LN,T1,T2,                     A1,A2)      \
        CCALLSFSUB5 (UN,LN,T1,T2,CF_0,CF_0,CF_0,A1,A2,0,0,0)
#define CCALLSFSUB3( UN,LN,T1,T2,T3,                  A1,A2,A3)   \
        CCALLSFSUB5 (UN,LN,T1,T2,T3,CF_0,CF_0,A1,A2,A3,0,0)
#define CCALLSFSUB4( UN,LN,T1,T2,T3,T4,               A1,A2,A3,A4)\
        CCALLSFSUB5 (UN,LN,T1,T2,T3,T4,CF_0,A1,A2,A3,A4,0)
#define CCALLSFSUB5( UN,LN,T1,T2,T3,T4,T5,            A1,A2,A3,A4,A5)          \
        CCALLSFSUB10(UN,LN,T1,T2,T3,T4,T5,CF_0,CF_0,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,0,0,0,0,0)
#define CCALLSFSUB6( UN,LN,T1,T2,T3,T4,T5,T6,         A1,A2,A3,A4,A5,A6)       \
        CCALLSFSUB10(UN,LN,T1,T2,T3,T4,T5,T6,CF_0,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,0,0,0,0)
#define CCALLSFSUB7( UN,LN,T1,T2,T3,T4,T5,T6,T7,      A1,A2,A3,A4,A5,A6,A7)    \
        CCALLSFSUB10(UN,LN,T1,T2,T3,T4,T5,T6,T7,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,0,0,0)
#define CCALLSFSUB8( UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,   A1,A2,A3,A4,A5,A6,A7,A8) \
        CCALLSFSUB10(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,0,0)
#define CCALLSFSUB9( UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,A1,A2,A3,A4,A5,A6,A7,A8,A9)\
        CCALLSFSUB10(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,0)
#define CCALLSFSUB10(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA)\
        CCALLSFSUB14(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,CF_0,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,0,0,0,0)
#define CCALLSFSUB11(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB)\
        CCALLSFSUB14(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,0,0,0)
#define CCALLSFSUB12(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC)\
        CCALLSFSUB14(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,0,0)
#define CCALLSFSUB13(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD)\
        CCALLSFSUB14(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,0)

#define CCALLSFSUB14(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE)\
do{VVCF(T1,A1,B1) VVCF(T2,A2,B2) VVCF(T3,A3,B3) VVCF(T4,A4,B4) VVCF(T5,A5,B5)  \
   VVCF(T6,A6,B6) VVCF(T7,A7,B7) VVCF(T8,A8,B8) VVCF(T9,A9,B9) VVCF(TA,AA,BA)  \
   VVCF(TB,AB,BB) VVCF(TC,AC,BC) VVCF(TD,AD,BD) VVCF(TE,AE,BE)                 \
   PROTOCCALLSFSUB14(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE)          \
   ACF(LN,T1,A1,1) ACF(LN,T2,A2,2) ACF(LN,T3,A3,3)                             \
   ACF(LN,T4,A4,4) ACF(LN,T5,A5,5) ACF(LN,T6,A6,6) ACF(LN,T7,A7,7)             \
   ACF(LN,T8,A8,8) ACF(LN,T9,A9,9) ACF(LN,TA,AA,A) ACF(LN,TB,AB,B)             \
   ACF(LN,TC,AC,C) ACF(LN,TD,AD,D) ACF(LN,TE,AE,E)                             \
   CFC_(UN,LN)(AACF(T1,A1,1,0) AACF(T2,A2,2,1) AACF(T3,A3,3,1)                 \
               AACF(T4,A4,4,1) AACF(T5,A5,5,1) AACF(T6,A6,6,1) AACF(T7,A7,7,1) \
               AACF(T8,A8,8,1) AACF(T9,A9,9,1) AACF(TA,AA,A,1) AACF(TB,AB,B,1) \
               AACF(TC,AC,C,1) AACF(TD,AD,D,1) AACF(TE,AE,E,1)                 \
      JCF(T1,1) JCF(T2,2) JCF(T3,3) JCF(T4,4) JCF(T5,5) JCF(T6,6) JCF(T7,7)    \
      JCF(T8,8) JCF(T9,9) JCF(TA,A) JCF(TB,B) JCF(TC,C) JCF(TD,D) JCF(TE,E)  );\
   WCF(T1,A1,1) WCF(T2,A2,2) WCF(T3,A3,3) WCF(T4,A4,4) WCF(T5,A5,5)            \
   WCF(T6,A6,6) WCF(T7,A7,7) WCF(T8,A8,8) WCF(T9,A9,9) WCF(TA,AA,A)            \
   WCF(TB,AB,B) WCF(TC,AC,C) WCF(TD,AD,D) WCF(TE,AE,E)             }while(FALSE)

/* Apollo 6.7, CRAY, Sun, VAX/Ultrix vcc/cc and HP can't hack more than 31 arg's */
#if !(defined(VAXUltrix)&&!defined(__GNUC__)) && !defined(__CF__APOLLO67) && !defined(sun) && !defined(__hpux) && !defined(_CRAY)
#define CCALLSFSUB15(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF)\
        CCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,CF_0,CF_0,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,0,0,0,0,0)
#define CCALLSFSUB16(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG)\
        CCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,CF_0,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG,0,0,0,0)
#define CCALLSFSUB17(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG,AH)\
        CCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG,AH,0,0,0)
#define CCALLSFSUB18(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,TI,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG,AH,AI)\
        CCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,TI,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG,AH,AI,0,0)
#define CCALLSFSUB19(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,TI,TJ,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG,AH,AI,AJ)\
        CCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,TI,TJ,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG,AH,AI,AJ,0)

/* PROTOCCALLSFSUB20 is commented out, because it chokes the VAX VMS compiler.
   It isn't required since we so far only pass pointers and integers to 
   FORTRAN routines and these arg.'s aren't promoted to anything else.        */

#define CCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH, \
        TI,TJ,TK, A1,A2,A3,A4,A5,A6,A7,A8,A9,AA,AB,AC,AD,AE,AF,AG,AH,AI,AJ,AK) \
do{VVCF(T1,A1,B1) VVCF(T2,A2,B2) VVCF(T3,A3,B3) VVCF(T4,A4,B4) VVCF(T5,A5,B5)  \
   VVCF(T6,A6,B6) VVCF(T7,A7,B7) VVCF(T8,A8,B8) VVCF(T9,A9,B9) VVCF(TA,AA,BA)  \
   VVCF(TB,AB,BB) VVCF(TC,AC,BC) VVCF(TD,AD,BD) VVCF(TE,AE,BE) VVCF(TF,AF,BF)  \
   VVCF(TG,AG,BG) VVCF(TH,AH,BH) VVCF(TI,AI,BI) VVCF(TJ,AJ,BJ) VVCF(TK,AK,BK)  \
/*   PROTOCCALLSFSUB20(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD,TE,TF,TG,TH,TI,TJ,TK)*/\
   ACF(LN,T1,A1,1) ACF(LN,T2,A2,2) ACF(LN,T3,A3,3) ACF(LN,T4,A4,4)             \
   ACF(LN,T5,A5,5) ACF(LN,T6,A6,6) ACF(LN,T7,A7,7) ACF(LN,T8,A8,8)             \
   ACF(LN,T9,A9,9) ACF(LN,TA,AA,A) ACF(LN,TB,AB,B) ACF(LN,TC,AC,C)             \
   ACF(LN,TD,AD,D) ACF(LN,TE,AE,E) ACF(LN,TF,AF,F) ACF(LN,TG,AG,G)             \
   ACF(LN,TH,AH,H) ACF(LN,TI,AI,I) ACF(LN,TJ,AJ,J) ACF(LN,TK,AK,K)             \
   CFC_(UN,LN)(AACF(T1,A1,1,0) AACF(T2,A2,2,1) AACF(T3,A3,3,1) AACF(T4,A4,4,1) \
               AACF(T5,A5,5,1) AACF(T6,A6,6,1) AACF(T7,A7,7,1) AACF(T8,A8,8,1) \
               AACF(T9,A9,9,1) AACF(TA,AA,A,1) AACF(TB,AB,B,1) AACF(TC,AC,C,1) \
               AACF(TD,AD,D,1) AACF(TE,AE,E,1) AACF(TF,AF,F,1) AACF(TG,AG,G,1) \
               AACF(TH,AH,H,1) AACF(TI,AI,I,1) AACF(TJ,AJ,J,1) AACF(TK,AK,K,1) \
      JCF(T1,1) JCF(T2,2) JCF(T3,3) JCF(T4,4) JCF(T5,5) JCF(T6,6) JCF(T7,7)    \
      JCF(T8,8) JCF(T9,9) JCF(TA,A) JCF(TB,B) JCF(TC,C) JCF(TD,D) JCF(TE,E)    \
      JCF(TF,F) JCF(TG,G) JCF(TH,H) JCF(TI,I) JCF(TJ,J) JCF(TK,K)          );  \
 WCF(T1,A1,1) WCF(T2,A2,2) WCF(T3,A3,3) WCF(T4,A4,4) WCF(T5,A5,5) WCF(T6,A6,6) \
 WCF(T7,A7,7) WCF(T8,A8,8) WCF(T9,A9,9) WCF(TA,AA,A) WCF(TB,AB,B) WCF(TC,AC,C) \
 WCF(TD,AD,D) WCF(TE,AE,E) WCF(TF,AF,F) WCF(TG,AG,G) WCF(TH,AH,H) WCF(TI,AI,I) \
 WCF(TJ,AJ,J) WCF(TK,AK,K) }while(FALSE)
#endif         /* Apollo 6.7, CRAY, Sun and HP can't hack more than 31 arg.'s */

/*-------------------------------------------------------------------------*/

/*               UTILITIES FOR C TO CALL FORTRAN FUNCTIONS                 */

/*N.B. PROTOCCALLSFFUNn(..) generates code, whether or not the FORTRAN
  function is called. Therefore, especially for creator's of C header files
  for large FORTRAN libraries which include many functions, to reduce
  compile time and object code size, it may be desirable to create
  preprocessor directives to allow users to create code for only those
  functions which they use.                                                */

/* The following defines the maximum length string that a function can return.
   Of course it may be undefine-d and re-define-d before individual
   PROTOCCALLSFFUNn(..) as required. It would also be nice to have this derived
   from the individual machines' limits.                                      */
#define MAX_LEN_FORTRAN_FUNCTION_STRING 0x4FE

/* The following defines a character used by CFORTRAN.H to flag the end of a
   string coming out of a FORTRAN routine.                                 */
#define CFORTRAN_NON_CHAR 0x7F

#ifdef OLD_VAXC                                /* Prevent %CC-I-PARAMNOTUSED. */
#pragma nostandard
#endif

#define _SEP_(TN,C,COMMA) __SEP_##C(TN,COMMA)
#define __SEP_0(TN,COMMA)  
#define __SEP_1(TN,COMMA)  _INT(2,SEP_,TN,COMMA,0)
#define SEP_INT(T,B)   A##B
#define SEP_INTV       SEP_INT
#define SEP_INTVV      SEP_INT
#define SEP_INTVVV     SEP_INT
#define SEP_INTVVVV    SEP_INT
#define SEP_INTVVVVV   SEP_INT
#define SEP_INTVVVVVV  SEP_INT
#define SEP_INTVVVVVVV SEP_INT
#define SEP_PINT       SEP_INT
#define SEP_PVOID      SEP_INT
#define SEP_ROUTINE    SEP_INT
#define SEP_SIMPLE     SEP_INT
#define SEP_VOID       SEP_INT    /* Need for FORTRAN calls to C subroutines. */
#define SEP_STRING     SEP_INT
#define SEP_STRINGV    SEP_INT
#define SEP_PSTRING    SEP_INT
#define SEP_PSTRINGV   SEP_INT
#define SEP_PNSTRING   SEP_INT
#define SEP_PPSTRING   SEP_INT
#define SEP_STRVOID     SEP_INT
#define SEP_ZTRINGV    SEP_INT
#define SEP_PZTRINGV   SEP_INT
                         
#if defined(SIGNED_BYTE) || !defined(UNSIGNED_BYTE)
#ifdef OLD_VAXC
#define INTEGER_BYTE               char    /* Old VAXC barfs on 'signed char' */
#else
#define INTEGER_BYTE        signed char    /* default */
#endif
#else
#define INTEGER_BYTE        unsigned char
#endif
#define    typePBYTEVVVVVVV INTEGER_BYTE
#define  typePDOUBLEVVVVVVV DOUBLE_PRECISION 
#define   typePFLOATVVVVVVV float
#define     typePINTVVVVVVV int
#define typePLOGICALVVVVVVV int
#define    typePLONGVVVVVVV long
#define   typePSHORTVVVVVVV short

#define CFARGS0(A,T,W,X,Y,Z) A##T
#define CFARGS1(A,T,W,X,Y,Z) A##T(W)
#define CFARGS2(A,T,W,X,Y,Z) A##T(W,X)
#define CFARGS3(A,T,W,X,Y,Z) A##T(W,X,Y)
#define CFARGS4(A,T,W,X,Y,Z) A##T(W,X,Y,Z)

#define _INT(N,T,I,Y,Z)              INT_##I(N,T,I,Y,Z)
#define INT_BYTE                     INT_DOUBLE
#define INT_DOUBLE(       N,A,B,Y,Z) CFARGS##N(A,INT,B,Y,Z,0)
#define INT_FLOAT                    INT_DOUBLE
#define INT_INT                      INT_DOUBLE
#define INT_LOGICAL                  INT_DOUBLE
#define INT_LONG                     INT_DOUBLE
#define INT_SHORT                    INT_DOUBLE
#define INT_PBYTE                    INT_PDOUBLE
#define INT_PDOUBLE(      N,A,B,Y,Z) CFARGS##N(A,PINT,B,Y,Z,0)
#define INT_PFLOAT                   INT_PDOUBLE
#define INT_PINT                     INT_PDOUBLE
#define INT_PLOGICAL                 INT_PDOUBLE
#define INT_PLONG                    INT_PDOUBLE
#define INT_PSHORT                   INT_PDOUBLE
#define INT_BYTEV                    INT_DOUBLEV
#define INT_BYTEVV                   INT_DOUBLEVV
#define INT_BYTEVVV                  INT_DOUBLEVVV
#define INT_BYTEVVVV                 INT_DOUBLEVVVV
#define INT_BYTEVVVVV                INT_DOUBLEVVVVV
#define INT_BYTEVVVVVV               INT_DOUBLEVVVVVV
#define INT_BYTEVVVVVVV              INT_DOUBLEVVVVVVV
#define INT_DOUBLEV(      N,A,B,Y,Z) CFARGS##N(A,INTV,B,Y,Z,0)
#define INT_DOUBLEVV(     N,A,B,Y,Z) CFARGS##N(A,INTVV,B,Y,Z,0)
#define INT_DOUBLEVVV(    N,A,B,Y,Z) CFARGS##N(A,INTVVV,B,Y,Z,0)
#define INT_DOUBLEVVVV(   N,A,B,Y,Z) CFARGS##N(A,INTVVVV,B,Y,Z,0)
#define INT_DOUBLEVVVVV(  N,A,B,Y,Z) CFARGS##N(A,INTVVVVV,B,Y,Z,0)
#define INT_DOUBLEVVVVVV( N,A,B,Y,Z) CFARGS##N(A,INTVVVVVV,B,Y,Z,0)
#define INT_DOUBLEVVVVVVV(N,A,B,Y,Z) CFARGS##N(A,INTVVVVVVV,B,Y,Z,0)
#define INT_FLOATV                   INT_DOUBLEV
#define INT_FLOATVV                  INT_DOUBLEVV
#define INT_FLOATVVV                 INT_DOUBLEVVV
#define INT_FLOATVVVV                INT_DOUBLEVVVV
#define INT_FLOATVVVVV               INT_DOUBLEVVVVV
#define INT_FLOATVVVVVV              INT_DOUBLEVVVVVV
#define INT_FLOATVVVVVVV             INT_DOUBLEVVVVVVV
#define INT_INTV                     INT_DOUBLEV
#define INT_INTVV                    INT_DOUBLEVV
#define INT_INTVVV                   INT_DOUBLEVVV
#define INT_INTVVVV                  INT_DOUBLEVVVV
#define INT_INTVVVVV                 INT_DOUBLEVVVVV
#define INT_INTVVVVVV                INT_DOUBLEVVVVVV
#define INT_INTVVVVVVV               INT_DOUBLEVVVVVVV
#define INT_LOGICALV                 INT_DOUBLEV
#define INT_LOGICALVV                INT_DOUBLEVV
#define INT_LOGICALVVV               INT_DOUBLEVVV
#define INT_LOGICALVVVV              INT_DOUBLEVVVV
#define INT_LOGICALVVVVV             INT_DOUBLEVVVVV
#define INT_LOGICALVVVVVV            INT_DOUBLEVVVVVV
#define INT_LOGICALVVVVVVV           INT_DOUBLEVVVVVVV
#define INT_LONGV                    INT_DOUBLEV
#define INT_LONGVV                   INT_DOUBLEVV
#define INT_LONGVVV                  INT_DOUBLEVVV
#define INT_LONGVVVV                 INT_DOUBLEVVVV
#define INT_LONGVVVVV                INT_DOUBLEVVVVV
#define INT_LONGVVVVVV               INT_DOUBLEVVVVVV
#define INT_LONGVVVVVVV              INT_DOUBLEVVVVVVV
#define INT_SHORTV                   INT_DOUBLEV
#define INT_SHORTVV                  INT_DOUBLEVV
#define INT_SHORTVVV                 INT_DOUBLEVVV
#define INT_SHORTVVVV                INT_DOUBLEVVVV
#define INT_SHORTVVVVV               INT_DOUBLEVVVVV
#define INT_SHORTVVVVVV              INT_DOUBLEVVVVVV
#define INT_SHORTVVVVVVV             INT_DOUBLEVVVVVVV
#define INT_PVOID(        N,A,B,Y,Z) CFARGS##N(A,B,B,Y,Z,0)
#define INT_ROUTINE                  INT_PVOID
/*CRAY coughs on the first, i.e. the usual trouble of not being able to
  define macros to macros with arguments. */
/*#define INT_SIMPLE                   INT_PVOID*/
#define INT_SIMPLE(       N,A,B,Y,Z) INT_PVOID(N,A,B,Y,Z)
#define INT_VOID                     INT_PVOID
#define INT_STRING                   INT_PVOID
#define INT_STRINGV                  INT_PVOID
#define INT_PSTRING                  INT_PVOID
#define INT_PSTRINGV                 INT_PVOID
#define INT_PNSTRING                 INT_PVOID
#define INT_PPSTRING                 INT_PVOID
#define INT_ZTRINGV                  INT_PVOID
#define INT_PZTRINGV                 INT_PVOID
#define INT_STRVOID                   INT_PVOID
#define INT_CF_0(         N,A,B,Y,Z)
                         
#define   UCF(TN,I,C)  _SEP_(TN,C,COMMA) _INT(2,U,TN,A##I,0)
#define  UUCF(TN,I,C)  _SEP_(TN,C,COMMA) _SEP_(TN,1,I) 
#define UUUCF(TN,I,C)  _SEP_(TN,C,COLON) _INT(2,U,TN,A##I,0)
#define UINT(       T,A) typeP##T##VVVVVVV  A
#define UINTV(      T,A) typeP##T##VVVVVV  *A
#define UINTVV(     T,A) typeP##T##VVVVV   *A
#define UINTVVV(    T,A) typeP##T##VVVV    *A
#define UINTVVVV(   T,A) typeP##T##VVV     *A
#define UINTVVVVV(  T,A) typeP##T##VV      *A
#define UINTVVVVVV( T,A) typeP##T##V       *A
#define UINTVVVVVVV(T,A) typeP##T            *A
#define UPINT(      T,A)  type##T##VVVVVVV *A
#define UPVOID(     T,A) void *A 
#define UROUTINE(   T,A) void (*A)() 
#define UVOID(      T,A) void  A     /* Needed for C calls FORTRAN subroutines. */
#define USTRING(    T,A) char *A     /*            via VOID and wrapper.        */
#define USTRINGV(   T,A) char *A
#define UPSTRING(   T,A) char *A
#define UPSTRINGV(  T,A) char *A
#define UZTRINGV(   T,A) char *A
#define UPZTRINGV(  T,A) char *A

/* VOID breaks U into U and UU. */
#define UUINT(      T,A) typeP##T##VVVVVVV  A
#define UUVOID(     T,A)           /* Needed for FORTRAN calls C subroutines. */
#define UUSTRING(   T,A) char *A 

/* Sun and VOID break U into U and PU. */
#define PUBYTE(      A) INTEGER_BYTE     A
#define PUDOUBLE(    A) DOUBLE_PRECISION A
#ifndef sunFortran
#define PUFLOAT(     A) float   A
#else
#define PUFLOAT(     A) FLOATFUNCTIONTYPE   A
#endif
#define PUINT(       A) int     A
#define PULOGICAL(   A) int     A
#define PULONG(      A) long    A
#define PUSHORT(     A) short   A
#define PUSTRING(    A) char   *A 
#define PUVOID(      A) void    A

#define EBYTE          INTEGER_BYTE     A0;
#define EDOUBLE        DOUBLE_PRECISION A0;
#ifndef sunFortran
#define EFLOAT         float  A0;
#else
#define EFLOAT         float AA0;   FLOATFUNCTIONTYPE A0;
#endif
#define EINT           int    A0;
#define ELOGICAL       int    A0;
#define ELONG          long   A0;
#define ESHORT         short  A0;
#define EVOID
#ifdef vmsFortran
#define ESTRING        static char AA0[MAX_LEN_FORTRAN_FUNCTION_STRING+1];     \
                       static fstring A0 =                                     \
             {MAX_LEN_FORTRAN_FUNCTION_STRING,DSC$K_DTYPE_T,DSC$K_CLASS_S,AA0};\
               memset(AA0, CFORTRAN_NON_CHAR, MAX_LEN_FORTRAN_FUNCTION_STRING);\
                                    *(AA0+MAX_LEN_FORTRAN_FUNCTION_STRING)='\0';
#else
#ifdef CRAYFortran
#define ESTRING        static char AA0[MAX_LEN_FORTRAN_FUNCTION_STRING+1];     \
                   static _fcd A0; *(AA0+MAX_LEN_FORTRAN_FUNCTION_STRING)='\0';\
                memset(AA0,CFORTRAN_NON_CHAR, MAX_LEN_FORTRAN_FUNCTION_STRING);\
                            A0 = _cptofcd(AA0,MAX_LEN_FORTRAN_FUNCTION_STRING);
#else
#define ESTRING        static char A0[MAX_LEN_FORTRAN_FUNCTION_STRING+1];      \
                       memset(A0, CFORTRAN_NON_CHAR,                           \
                              MAX_LEN_FORTRAN_FUNCTION_STRING);                \
                       *(A0+MAX_LEN_FORTRAN_FUNCTION_STRING)='\0';
#endif
#endif
/* ESTRING must use static char. array which is guaranteed to exist after
   function returns.                                                     */

/* N.B.i) The diff. for 0 (Zero) and >=1 arguments.
       ii)That the following create an unmatched bracket, i.e. '(', which
          must of course be matched in the call.
       iii)Commas must be handled very carefully                         */
#define GZINT(    T,UN,LN) A0=CFC_(UN,LN)(
#define GZVOID(   T,UN,LN)    CFC_(UN,LN)(
#ifdef vmsFortran
#define GZSTRING( T,UN,LN)    CFC_(UN,LN)(&A0
#else
#ifdef CRAYFortran
#define GZSTRING( T,UN,LN)    CFC_(UN,LN)( A0
#else
#define GZSTRING( T,UN,LN)    CFC_(UN,LN)( A0,MAX_LEN_FORTRAN_FUNCTION_STRING
#endif
#endif

#define GINT               GZINT
#define GVOID              GZVOID
#define GSTRING(  T,UN,LN) GZSTRING(T,UN,LN),

#define    PPBYTEVVVVVVV
#define     PPINTVVVVVVV     /* These complement PPFLOATVVVVVVV. */
#define  PPDOUBLEVVVVVVV
#define PPLOGICALVVVVVVV
#define    PPLONGVVVVVVV
#define   PPSHORTVVVVVVV

#define BCF(TN,AN,C)   _SEP_(TN,C,COMMA) _INT(2,B,TN,AN,0)
#define BINT(       T,A) (typeP##T##VVVVVVV) A
#define BINTV(      T,A)            A
#define BINTVV(     T,A)           (A)[0]
#define BINTVVV(    T,A)           (A)[0][0]
#define BINTVVVV(   T,A)           (A)[0][0][0]
#define BINTVVVVV(  T,A)           (A)[0][0][0][0]
#define BINTVVVVVV( T,A)           (A)[0][0][0][0][0]
#define BINTVVVVVVV(T,A)           (A)[0][0][0][0][0][0]
#define BPINT(      T,A) P##T##VVVVVVV  &A
#define BSTRING(    T,A) (char *)   A
#define BSTRINGV(   T,A) (char *)   A
#define BPSTRING(   T,A) (char *)   A
#define BPSTRINGV(  T,A) (char *)   A
#define BPVOID(     T,A) (void *)   A
#define BROUTINE(   T,A) (void(*)())A
#define BZTRINGV(   T,A) (char *)   A
#define BPZTRINGV(  T,A) (char *)   A
                                                              	
#define ZCF(TN,N,AN)   _INT(3,Z,TN,N,AN)
#define ZINT(       T,I,A) (__cfztringv[I]=(int)A),
#define ZPINT              ZINT
#define ZINTV(      T,I,A)
#define ZINTVV(     T,I,A)
#define ZINTVVV(    T,I,A)
#define ZINTVVVV(   T,I,A)
#define ZINTVVVVV(  T,I,A)
#define ZINTVVVVVV( T,I,A)
#define ZINTVVVVVVV(T,I,A)
#define ZSTRING(    T,I,A)
#define ZSTRINGV(   T,I,A)
#define ZPSTRING(   T,I,A)
#define ZPSTRINGV(  T,I,A)
#define ZPVOID(     T,I,A)
#define ZROUTINE(   T,I,A)
#define ZSIMPLE(    T,I,A)
#define ZZTRINGV(   T,I,A)
#define ZPZTRINGV(  T,I,A)

#define SCF(TN,NAME,I,A) STR_##TN(3,S,NAME,I,A,0)
#define SLOGICAL( M,I,A)
#define SPLOGICAL(M,I,A)
#define SSTRING(  M,I,A) ,sizeof(A)
#define SSTRINGV( M,I,A) ,( (unsigned)0xFFFF*firstindexlength(A)               \
                             +secondindexlength(A))
#define SPSTRING( M,I,A) ,sizeof(A)
#define SPSTRINGV          SSTRINGV
#define SZTRINGV( M,I,A) ,( (unsigned)0xFFFF*M##_ELEMS_##I                 \
                             +M##_ELEMLEN_##I+1)
#define SPZTRINGV        SZTRINGV

#define   HCF(TN,I)      STR_##TN(3,H,COMMA, H,C##I,0)
#define  HHCF(TN,I)      STR_##TN(3,H,COMMA,HH,C##I,0)
#define HHHCF(TN,I)      STR_##TN(3,H,COLON, H,C##I,0)
#define  H_CF_SPECIAL    unsigned
#define HH_CF_SPECIAL
#define HLOGICAL( S,U,B)
#define HPLOGICAL(S,U,B)
#define HSTRING(  S,U,B) A##S U##_CF_SPECIAL B
#define HSTRINGV         HSTRING    
#define HPSTRING         HSTRING
#define HPSTRINGV        HSTRING
#define HPNSTRING        HSTRING
#define HPPSTRING        HSTRING
#define HSTRVOID          HSTRING
#define HZTRINGV         HSTRING
#define HPZTRINGV        HSTRING

#define STR_BYTE(          N,T,A,B,C,D)
#define STR_DOUBLE(        N,T,A,B,C,D)      /* Can't add spaces inside       */
#define STR_FLOAT(         N,T,A,B,C,D)      /* expansion since it screws up  */
#define STR_INT(           N,T,A,B,C,D)      /* macro catenation kludge.      */
#define STR_LOGICAL(       N,T,A,B,C,D) CFARGS##N(T,LOGICAL,A,B,C,D)
#define STR_LONG(          N,T,A,B,C,D)
#define STR_SHORT(         N,T,A,B,C,D)
#define STR_BYTEV(         N,T,A,B,C,D)
#define STR_BYTEVV(        N,T,A,B,C,D)
#define STR_BYTEVVV(       N,T,A,B,C,D)
#define STR_BYTEVVVV(      N,T,A,B,C,D)
#define STR_BYTEVVVVV(     N,T,A,B,C,D)
#define STR_BYTEVVVVVV(    N,T,A,B,C,D)
#define STR_BYTEVVVVVVV(   N,T,A,B,C,D)
#define STR_DOUBLEV(       N,T,A,B,C,D)
#define STR_DOUBLEVV(      N,T,A,B,C,D)
#define STR_DOUBLEVVV(     N,T,A,B,C,D)
#define STR_DOUBLEVVVV(    N,T,A,B,C,D)
#define STR_DOUBLEVVVVV(   N,T,A,B,C,D)
#define STR_DOUBLEVVVVVV(  N,T,A,B,C,D)
#define STR_DOUBLEVVVVVVV( N,T,A,B,C,D)
#define STR_FLOATV(        N,T,A,B,C,D)
#define STR_FLOATVV(       N,T,A,B,C,D)
#define STR_FLOATVVV(      N,T,A,B,C,D)
#define STR_FLOATVVVV(     N,T,A,B,C,D)
#define STR_FLOATVVVVV(    N,T,A,B,C,D)
#define STR_FLOATVVVVVV(   N,T,A,B,C,D)
#define STR_FLOATVVVVVVV(  N,T,A,B,C,D)
#define STR_INTV(          N,T,A,B,C,D)
#define STR_INTVV(         N,T,A,B,C,D)
#define STR_INTVVV(        N,T,A,B,C,D)
#define STR_INTVVVV(       N,T,A,B,C,D)
#define STR_INTVVVVV(      N,T,A,B,C,D)
#define STR_INTVVVVVV(     N,T,A,B,C,D)
#define STR_INTVVVVVVV(    N,T,A,B,C,D)
#define STR_LOGICALV(      N,T,A,B,C,D)
#define STR_LOGICALVV(     N,T,A,B,C,D)
#define STR_LOGICALVVV(    N,T,A,B,C,D)
#define STR_LOGICALVVVV(   N,T,A,B,C,D)
#define STR_LOGICALVVVVV(  N,T,A,B,C,D)
#define STR_LOGICALVVVVVV( N,T,A,B,C,D)
#define STR_LOGICALVVVVVVV(N,T,A,B,C,D)
#define STR_LONGV(         N,T,A,B,C,D)
#define STR_LONGVV(        N,T,A,B,C,D)
#define STR_LONGVVV(       N,T,A,B,C,D)
#define STR_LONGVVVV(      N,T,A,B,C,D)
#define STR_LONGVVVVV(     N,T,A,B,C,D)
#define STR_LONGVVVVVV(    N,T,A,B,C,D)
#define STR_LONGVVVVVVV(   N,T,A,B,C,D)
#define STR_SHORTV(        N,T,A,B,C,D)
#define STR_SHORTVV(       N,T,A,B,C,D)
#define STR_SHORTVVV(      N,T,A,B,C,D)
#define STR_SHORTVVVV(     N,T,A,B,C,D)
#define STR_SHORTVVVVV(    N,T,A,B,C,D)
#define STR_SHORTVVVVVV(   N,T,A,B,C,D)
#define STR_SHORTVVVVVVV(  N,T,A,B,C,D)
#define STR_PBYTE(         N,T,A,B,C,D)
#define STR_PDOUBLE(       N,T,A,B,C,D)
#define STR_PFLOAT(        N,T,A,B,C,D)
#define STR_PINT(          N,T,A,B,C,D)
#define STR_PLOGICAL(      N,T,A,B,C,D) CFARGS##N(T,PLOGICAL,A,B,C,D)
#define STR_PLONG(         N,T,A,B,C,D)
#define STR_PSHORT(        N,T,A,B,C,D)
#define STR_STRING(        N,T,A,B,C,D) CFARGS##N(T,STRING,A,B,C,D)
#define STR_PSTRING(       N,T,A,B,C,D) CFARGS##N(T,PSTRING,A,B,C,D)
#define STR_STRINGV(       N,T,A,B,C,D) CFARGS##N(T,STRINGV,A,B,C,D)
#define STR_PSTRINGV(      N,T,A,B,C,D) CFARGS##N(T,PSTRINGV,A,B,C,D)
#define STR_PNSTRING(      N,T,A,B,C,D) CFARGS##N(T,PNSTRING,A,B,C,D)
#define STR_PPSTRING(      N,T,A,B,C,D) CFARGS##N(T,PPSTRING,A,B,C,D)
#define STR_STRVOID(        N,T,A,B,C,D) CFARGS##N(T,STRVOID,A,B,C,D)
#define STR_PVOID(         N,T,A,B,C,D)
#define STR_ROUTINE(       N,T,A,B,C,D)
#define STR_SIMPLE(        N,T,A,B,C,D)
#define STR_ZTRINGV(       N,T,A,B,C,D) CFARGS##N(T,ZTRINGV,A,B,C,D)
#define STR_PZTRINGV(      N,T,A,B,C,D) CFARGS##N(T,PZTRINGV,A,B,C,D)
#define STR_CF_0(          N,T,A,B,C,D)               

/* See ACF table comments, which explain why CCF was split into two. */
#define CCF(TN,I)          STR_##TN(3,C,A##I,B##I,C##I,0)
#define CLOGICAL( A,B,C)  A=C2FLOGICAL( A);
#define CPLOGICAL(A,B,C) *A=C2FLOGICAL(*A);
#ifdef vmsFortran
#define CSTRING(  A,B,C) (B.clen=strlen(A),B.f.dsc$a_pointer=A,                \
                    C==sizeof(char*)||C==B.clen+1?B.f.dsc$w_length=B.clen:     \
          (memset((A)+B.clen,' ',C-B.clen-1),A[B.f.dsc$w_length=C-1]='\0'));
#define CSTRINGV( A,B,C) (                                                     \
          initfstr(B, malloc((C/0xFFFF)*(C%0xFFFF-1)), C/0xFFFF, C%0xFFFF-1),  \
          c2fstrv(A,B.dsc$a_pointer,C%0xFFFF,(C/0xFFFF)*(C%0xFFFF)) );
#define CPSTRING( A,B,C) (B.dsc$w_length=strlen(A),B.dsc$a_pointer=A,          \
        C==sizeof(char*)?0:(memset((A)+B.dsc$w_length,' ',C-B.dsc$w_length-1), \
                             A[B.dsc$w_length=C-1]='\0'));
#define CPSTRINGV(A,B,C)  (initfstr(B, A, C/0xFFFF, C%0xFFFF-1),               \
                             c2fstrv(A,A,C%0xFFFF,(C/0xFFFF)*(C%0xFFFF)) );
#else
#ifdef CRAYFortran
#define CSTRING(  A,B,C) (B.clen=strlen(A),                                    \
                          C==sizeof(char*)||C==B.clen+1?B.flen=B.clen:         \
                        (memset((A)+B.clen,' ',C-B.clen-1),A[B.flen=C-1]='\0'));
#define CSTRINGV( A,B,C) (B.s=malloc((C/0xFFFF)*(C%0xFFFF-1)),                 \
                    c2fstrv(A,B.s,(B.flen=C%0xFFFF-1)+1,(C/0xFFFF)*(C%0xFFFF)));
#define CPSTRING( A,B,C) (B=strlen(A), C==sizeof(char*)?0:                     \
                            (memset((A)+B,' ',C-B-1),A[B=C-1]='\0'));
#define CPSTRINGV(A,B,C) c2fstrv(A,A,(B.flen=C%0xFFFF-1)+1,                    \
                                   B.sizeofA=(C/0xFFFF)*(C%0xFFFF));
#else
#define CSTRING(  A,B,C) (B.clen=strlen(A),                                    \
                            C==sizeof(char*)||C==B.clen+1?B.flen=B.clen:       \
                        (memset((A)+B.clen,' ',C-B.clen-1),A[B.flen=C-1]='\0'));
#define CSTRINGV( A,B,C) (B.s=malloc((C/0xFFFF)*(C%0xFFFF-1)),                 \
               B.fs=c2fstrv(A,B.s,(B.flen=C%0xFFFF-1)+1,(C/0xFFFF)*(C%0xFFFF)));
#define CPSTRING( A,B,C) (B=strlen(A), C==sizeof(char*)?0:                     \
                            (memset((A)+B,' ',C-B-1),A[B=C-1]='\0'));
#define CPSTRINGV(A,B,C) B.fs=c2fstrv(A,A,(B.flen=C%0xFFFF-1)+1,               \
                                        B.sizeofA=(C/0xFFFF)*(C%0xFFFF));
#endif
#endif
#define CZTRINGV         CSTRINGV
#define CPZTRINGV        CPSTRINGV

#define CCCBYTE(    A,B) &A
#define CCCDOUBLE(  A,B) &A
#if !defined(__CF__KnR)
#define CCCFLOAT(   A,B) &A
                            /* Although the VAX doesn't, at least the         */
#else                       /* HP and K&R mips promote float arg.'s of        */
#define CCCFLOAT(   A,B) &B /* unprototyped functions to double. So we can't  */
#endif                      /* use A here to pass the argument to FORTRAN.    */
#define CCCINT(     A,B) &A
#define CCCLOGICAL( A,B) &A
#define CCCLONG(    A,B) &A
#define CCCSHORT(   A,B) &A
#define CCCPBYTE(   A,B)  A
#define CCCPDOUBLE( A,B)  A
#define CCCPFLOAT(  A,B)  A
#define CCCPINT(    A,B)  A
#define CCCPLOGICAL(A,B)  B=A       /* B used to keep a common W table. */
#define CCCPLONG(   A,B)  A
#define CCCPSHORT(  A,B)  A

#define CCCF(TN,I,M)    _SEP_(TN,M,COMMA) _INT(3,CC,TN,A##I,B##I)
#define CCINT(       T,A,B) CCC##T(A,B) 
#define CCINTV(      T,A,B)  A
#define CCINTVV(     T,A,B)  A
#define CCINTVVV(    T,A,B)  A
#define CCINTVVVV(   T,A,B)  A
#define CCINTVVVVV(  T,A,B)  A
#define CCINTVVVVVV( T,A,B)  A
#define CCINTVVVVVVV(T,A,B)  A
#define CCPINT(      T,A,B) CCC##T(A,B) 
#define CCPVOID(     T,A,B)  A
#ifdef apolloFortran
#define CCROUTINE(   T,A,B) &A
#else
#define CCROUTINE(   T,A,B)  A
#endif
#define CCSIMPLE(    T,A,B)  A
#ifdef vmsFortran
#define CCSTRING(    T,A,B) &B.f
#define CCSTRINGV(   T,A,B) &B
#define CCPSTRING(   T,A,B) &B
#define CCPSTRINGV(  T,A,B) &B
#else
#ifdef CRAYFortran
#define CCSTRING(    T,A,B) _cptofcd(A,B.flen)
#define CCSTRINGV(   T,A,B) _cptofcd(B.s,B.flen)
#define CCPSTRING(   T,A,B) _cptofcd(A,B)
#define CCPSTRINGV(  T,A,B) _cptofcd(A,B.flen)
#else
#define CCSTRING(    T,A,B)  A
#define CCSTRINGV(   T,A,B)  B.fs
#define CCPSTRING(   T,A,B)  A
#define CCPSTRINGV(  T,A,B)  B.fs
#endif
#endif
#define CCZTRINGV           CCSTRINGV
#define CCPZTRINGV          CCPSTRINGV

#define XBYTE          return A0;
#define XDOUBLE        return A0;
#ifndef sunFortran
#define XFLOAT         return A0;
#else
#define XFLOAT         ASSIGNFLOAT(AA0,A0); return AA0;
#endif
#define XINT           return A0;
#define XLOGICAL       return F2CLOGICAL(A0);
#define XLONG          return A0;
#define XSHORT         return A0;
#define XVOID          return   ;
#if defined(vmsFortran) || defined(CRAYFortran)
#define XSTRING        return kill_trailing(                                   \
                                      kill_trailing(AA0,CFORTRAN_NON_CHAR),' ');
#else
#define XSTRING        return kill_trailing(                                   \
                                      kill_trailing( A0,CFORTRAN_NON_CHAR),' ');
#endif

#define CFFUN(NAME) __cf__##NAME

/* Note that we don't use LN here, but we keep it for consistency. */
#define CCALLSFFUN0(UN,LN) CFFUN(UN)()

#ifdef OLD_VAXC                                  /* Allow %CC-I-PARAMNOTUSED. */
#pragma standard
#endif

#define CCALLSFFUN1( UN,LN,T1,                        A1)         \
        CCALLSFFUN5 (UN,LN,T1,CF_0,CF_0,CF_0,CF_0,A1,0,0,0,0)
#define CCALLSFFUN2( UN,LN,T1,T2,                     A1,A2)      \
        CCALLSFFUN5 (UN,LN,T1,T2,CF_0,CF_0,CF_0,A1,A2,0,0,0)
#define CCALLSFFUN3( UN,LN,T1,T2,T3,                  A1,A2,A3)   \
        CCALLSFFUN5 (UN,LN,T1,T2,T3,CF_0,CF_0,A1,A2,A3,0,0)
#define CCALLSFFUN4( UN,LN,T1,T2,T3,T4,               A1,A2,A3,A4)\
        CCALLSFFUN5 (UN,LN,T1,T2,T3,T4,CF_0,A1,A2,A3,A4,0)
#define CCALLSFFUN5( UN,LN,T1,T2,T3,T4,T5,            A1,A2,A3,A4,A5)          \
        CCALLSFFUN10(UN,LN,T1,T2,T3,T4,T5,CF_0,CF_0,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,0,0,0,0,0)
#define CCALLSFFUN6( UN,LN,T1,T2,T3,T4,T5,T6,         A1,A2,A3,A4,A5,A6)       \
        CCALLSFFUN10(UN,LN,T1,T2,T3,T4,T5,T6,CF_0,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,0,0,0,0)
#define CCALLSFFUN7( UN,LN,T1,T2,T3,T4,T5,T6,T7,      A1,A2,A3,A4,A5,A6,A7)    \
        CCALLSFFUN10(UN,LN,T1,T2,T3,T4,T5,T6,T7,CF_0,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,0,0,0)
#define CCALLSFFUN8( UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,   A1,A2,A3,A4,A5,A6,A7,A8) \
        CCALLSFFUN10(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,CF_0,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,0,0)
#define CCALLSFFUN9( UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,A1,A2,A3,A4,A5,A6,A7,A8,A9)\
        CCALLSFFUN10(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,CF_0,A1,A2,A3,A4,A5,A6,A7,A8,A9,0)

#define CCALLSFFUN10(UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,A1,A2,A3,A4,A5,A6,A7,A8,A9,AA)\
(ZCF(T1,1,A1) ZCF(T2,2,A2) ZCF(T3,3,A3) ZCF(T4,4,A4) ZCF(T5,5,A5)              \
 ZCF(T6,6,A6) ZCF(T7,7,A7) ZCF(T8,8,A8) ZCF(T9,9,A9) ZCF(TA,A,AA)              \
 (CFFUN(UN)(  BCF(T1,A1,0) BCF(T2,A2,1) BCF(T3,A3,1) BCF(T4,A4,1) BCF(T5,A5,1) \
              BCF(T6,A6,1) BCF(T7,A7,1) BCF(T8,A8,1) BCF(T9,A9,1) BCF(TA,AA,1) \
           SCF(T1,LN,1,A1) SCF(T2,LN,2,A2) SCF(T3,LN,3,A3) SCF(T4,LN,4,A4)     \
           SCF(T5,LN,5,A5) SCF(T6,LN,6,A6) SCF(T7,LN,7,A7) SCF(T8,LN,8,A8)     \
           SCF(T9,LN,9,A9) SCF(TA,LN,A,AA))))

/*  N.B. Create a separate function instead of using (call function, function
value here) because in order to create the variables needed for the input
arg.'s which may be const.'s one has to do the creation within {}, but these
can never be placed within ()'s. Therefore one must create wrapper functions.
gcc, on the other hand may be able to avoid the wrapper functions. */

/* Prototypes are needed to correctly handle the value returned correctly. N.B.
Can only have prototype arg.'s with difficulty, a la G... table since FORTRAN
functions returning strings have extra arg.'s. Don't bother, since this only
causes a compiler warning to come up when one uses FCALLSCFUNn and CCALLSFFUNn
for the same function in the same source code. Something done by the experts in
debugging only.*/    

#define PROTOCCALLSFFUN0(F,UN,LN)                                              \
PU##F( CFC_(UN,LN))(CF_NULL_PROTO);                                          \
static _INT(2,U,F,CFFUN(UN),0)() {E##F  _INT(3,GZ,F,UN,LN)); X##F}

#define PROTOCCALLSFFUN1( T0,UN,LN,T1)                                         \
        PROTOCCALLSFFUN5 (T0,UN,LN,T1,CF_0,CF_0,CF_0,CF_0)
#define PROTOCCALLSFFUN2( T0,UN,LN,T1,T2)                                      \
        PROTOCCALLSFFUN5 (T0,UN,LN,T1,T2,CF_0,CF_0,CF_0)
#define PROTOCCALLSFFUN3( T0,UN,LN,T1,T2,T3)                                   \
        PROTOCCALLSFFUN5 (T0,UN,LN,T1,T2,T3,CF_0,CF_0)
#define PROTOCCALLSFFUN4( T0,UN,LN,T1,T2,T3,T4)                                \
        PROTOCCALLSFFUN5 (T0,UN,LN,T1,T2,T3,T4,CF_0)
#define PROTOCCALLSFFUN5( T0,UN,LN,T1,T2,T3,T4,T5)                             \
        PROTOCCALLSFFUN10(T0,UN,LN,T1,T2,T3,T4,T5,CF_0,CF_0,CF_0,CF_0,CF_0)
#define PROTOCCALLSFFUN6( T0,UN,LN,T1,T2,T3,T4,T5,T6)                          \
        PROTOCCALLSFFUN10(T0,UN,LN,T1,T2,T3,T4,T5,T6,CF_0,CF_0,CF_0,CF_0)
#define PROTOCCALLSFFUN7( T0,UN,LN,T1,T2,T3,T4,T5,T6,T7)                       \
        PROTOCCALLSFFUN10(T0,UN,LN,T1,T2,T3,T4,T5,T6,T7,CF_0,CF_0,CF_0)
#define PROTOCCALLSFFUN8( T0,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8)                    \
        PROTOCCALLSFFUN10(T0,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,CF_0,CF_0)
#define PROTOCCALLSFFUN9( T0,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9)                 \
        PROTOCCALLSFFUN10(T0,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,CF_0)

/* HP/UX 9.01 cc requires the blank between '_INT(3,G,T0,UN,LN) CCCF(T1,1,0)' */

#ifndef __CF__KnR
#define PROTOCCALLSFFUN10(T0,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA)              \
PU##T0(CFC_(UN,LN))(CF_NULL_PROTO);                                          \
static _INT(2,U,T0,CFFUN(UN),0)(UCF(T1,1,0) UCF(T2,2,1) UCF(T3,3,1) UCF(T4,4,1)  \
   UCF(T5,5,1) UCF(T6,6,1) UCF(T7,7,1) UCF(T8,8,1) UCF(T9,9,1) UCF(TA,A,1)     \
                         HCF(T1,1) HCF(T2,2) HCF(T3,3) HCF(T4,4) HCF(T5,5)     \
                         HCF(T6,6) HCF(T7,7) HCF(T8,8) HCF(T9,9) HCF(TA,A) )   \
{VCF(T1,1) VCF(T2,2) VCF(T3,3) VCF(T4,4) VCF(T5,5)                             \
 VCF(T6,6) VCF(T7,7) VCF(T8,8) VCF(T9,9) VCF(TA,A) E##T0                     \
 CCF(T1,1) CCF(T2,2) CCF(T3,3) CCF(T4,4) CCF(T5,5)                             \
 CCF(T6,6) CCF(T7,7) CCF(T8,8) CCF(T9,9) CCF(TA,A)                             \
 _INT(3,G,T0,UN,LN) CCCF(T1,1,0) CCCF(T2,2,1) CCCF(T3,3,1) CCCF(T4,4,1) CCCF(T5,5,1)\
                    CCCF(T6,6,1) CCCF(T7,7,1) CCCF(T8,8,1) CCCF(T9,9,1) CCCF(TA,A,1)\
               JCF(T1,1) JCF(T2,2) JCF(T3,3) JCF(T4,4) JCF(T5,5)               \
               JCF(T6,6) JCF(T7,7) JCF(T8,8) JCF(T9,9) JCF(TA,A));             \
 WCF(T1,A1,1) WCF(T2,A2,2) WCF(T3,A3,3) WCF(T4,A4,4) WCF(T5,A5,5)              \
 WCF(T6,A6,6) WCF(T7,A7,7) WCF(T8,A8,8) WCF(T9,A9,9) WCF(TA,AA,A) X##T0}
#else
#define PROTOCCALLSFFUN10(T0,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA)              \
PU##T0(CFC_(UN,LN))(CF_NULL_PROTO);                                          \
static _INT(2,U,T0,CFFUN(UN),0)(UUCF(T1,1,0) UUCF(T2,2,1) UUCF(T3,3,1) UUCF(T4,4,1) \
      UUCF(T5,5,1) UUCF(T6,6,1) UUCF(T7,7,1) UUCF(T8,8,1) UUCF(T9,9,1) UUCF(TA,A,1) \
                       HHCF(T1,1) HHCF(T2,2) HHCF(T3,3) HHCF(T4,4) HHCF(T5,5)  \
                       HHCF(T6,6) HHCF(T7,7) HHCF(T8,8) HHCF(T9,9) HHCF(TA,A)) \
 UUUCF(T1,1,0) UUUCF(T2,2,1) UUUCF(T3,3,1) UUUCF(T4,4,1) UUUCF(T5,5,1)         \
 UUUCF(T6,6,1) UUUCF(T7,7,1) UUUCF(T8,8,1) UUUCF(T9,9,1) UUUCF(TA,A,1)         \
           HHHCF(T1,1) HHHCF(T2,2) HHHCF(T3,3) HHHCF(T4,4) HHHCF(T5,5)         \
           HHHCF(T6,6) HHHCF(T7,7) HHHCF(T8,8) HHHCF(T9,9) HHHCF(TA,A);        \
{VCF(T1,1) VCF(T2,2) VCF(T3,3) VCF(T4,4) VCF(T5,5)                             \
 VCF(T6,6) VCF(T7,7) VCF(T8,8) VCF(T9,9) VCF(TA,A) E##T0                     \
 CCF(T1,1) CCF(T2,2) CCF(T3,3) CCF(T4,4) CCF(T5,5)                             \
 CCF(T6,6) CCF(T7,7) CCF(T8,8) CCF(T9,9) CCF(TA,A)                             \
 _INT(3,G,T0,UN,LN) CCCF(T1,1,0) CCCF(T2,2,1) CCCF(T3,3,1) CCCF(T4,4,1) CCCF(T5,5,1)\
                    CCCF(T6,6,1) CCCF(T7,7,1) CCCF(T8,8,1) CCCF(T9,9,1) CCCF(TA,A,1)\
               JCF(T1,1) JCF(T2,2) JCF(T3,3) JCF(T4,4) JCF(T5,5)               \
               JCF(T6,6) JCF(T7,7) JCF(T8,8) JCF(T9,9) JCF(TA,A) );            \
 WCF(T1,A1,1) WCF(T2,A2,2) WCF(T3,A3,3) WCF(T4,A4,4) WCF(T5,A5,5)              \
 WCF(T6,A6,6) WCF(T7,A7,7) WCF(T8,A8,8) WCF(T9,A9,9) WCF(TA,AA,A) X##T0}
#endif

/*-------------------------------------------------------------------------*/

/*               UTILITIES FOR FORTRAN TO CALL C ROUTINES                  */

#ifdef OLD_VAXC                                /* Prevent %CC-I-PARAMNOTUSED. */
#pragma nostandard
#endif

#if defined(vmsFortran) || defined(CRAYFortran)
#define   DCF(TN,I)
#define  DDCF(TN,I)
#define DDDCF(TN,I)
#else
#define   DCF                HCF
#define  DDCF               HHCF
#define DDDCF              HHHCF
#endif

#define QCF(TN,I)    STR_##TN(1,Q,B##I, 0,0,0)
#define QLOGICAL( B)
#define QPLOGICAL(B)
#define QSTRINGV( B) char *B; unsigned int B##N;
#define QSTRING(  B) char *B=NULL;
#define QPSTRING( B) char *B=NULL;
#define QPSTRINGV    QSTRINGV
#define QPNSTRING(B) char *B=NULL;
#define QPPSTRING(B)
#define QSTRVOID(  B)

#ifdef     apolloFortran
#define ROUTINE_orig     (void *)*   /* Else, function value has to match. */
#else  /* !apolloFortran */
#ifdef     __sgi   /* Else SGI gives warning 182 contrary to its C LRM A.17.7 */
#define ROUTINE_orig    *(void**)& 
#else  /* !__sgi */
#define ROUTINE_orig     (void *)  
#endif /* __sgi */
#endif /* apolloFortran */

#define ROUTINE_1     ROUTINE_orig   
#define ROUTINE_2     ROUTINE_orig   
#define ROUTINE_3     ROUTINE_orig   
#define ROUTINE_4     ROUTINE_orig   
#define ROUTINE_5     ROUTINE_orig   
#define ROUTINE_6     ROUTINE_orig   
#define ROUTINE_7     ROUTINE_orig   
#define ROUTINE_8     ROUTINE_orig   
#define ROUTINE_9     ROUTINE_orig   
#define ROUTINE_10    ROUTINE_orig  
 
#define ROUTINE_11    ROUTINE_orig   
#define ROUTINE_12    ROUTINE_orig   
#define ROUTINE_13    ROUTINE_orig      

#define TCF(NAME,TN,I,M)  _SEP_(TN,M,COMMA) T##TN(NAME,I,A##I,B##I,C##I)
#define TBYTE(          M,I,A,B,D) *A
#define TDOUBLE(        M,I,A,B,D) *A
#define TFLOAT(         M,I,A,B,D) *A
#define TINT(           M,I,A,B,D) *A
#define TLOGICAL(       M,I,A,B,D)  F2CLOGICAL(*A)
#define TLONG(          M,I,A,B,D) *A
#define TSHORT(         M,I,A,B,D) *A
#define TBYTEV(         M,I,A,B,D)  A
#define TDOUBLEV(       M,I,A,B,D)  A
#define TFLOATV(        M,I,A,B,D)  VOIDP0 A
#define TINTV(          M,I,A,B,D)  A
#define TLOGICALV(      M,I,A,B,D)  A
#define TLONGV(         M,I,A,B,D)  A
#define TSHORTV(        M,I,A,B,D)  A
#define TBYTEVV(        M,I,A,B,D)  (void *)A /* We have to cast to void *,   */
#define TBYTEVVV(       M,I,A,B,D)  (void *)A /* since we don't know the      */
#define TBYTEVVVV(      M,I,A,B,D)  (void *)A /* dimensions of the array.     */
#define TBYTEVVVVV(     M,I,A,B,D)  (void *)A /* i.e. Unfortunately, can't    */
#define TBYTEVVVVVV(    M,I,A,B,D)  (void *)A /* check that the type matches  */
#define TBYTEVVVVVVV(   M,I,A,B,D)  (void *)A /* with the prototype.          */
#define TDOUBLEVV(      M,I,A,B,D)  (void *)A
#define TDOUBLEVVV(     M,I,A,B,D)  (void *)A
#define TDOUBLEVVVV(    M,I,A,B,D)  (void *)A
#define TDOUBLEVVVVV(   M,I,A,B,D)  (void *)A
#define TDOUBLEVVVVVV(  M,I,A,B,D)  (void *)A
#define TDOUBLEVVVVVVV( M,I,A,B,D)  (void *)A
#define TFLOATVV(       M,I,A,B,D)  (void *)A
#define TFLOATVVV(      M,I,A,B,D)  (void *)A
#define TFLOATVVVV(     M,I,A,B,D)  (void *)A
#define TFLOATVVVVV(    M,I,A,B,D)  (void *)A
#define TFLOATVVVVVV(   M,I,A,B,D)  (void *)A
#define TFLOATVVVVVVV(  M,I,A,B,D)  (void *)A
#define TINTVV(         M,I,A,B,D)  (void *)A  
#define TINTVVV(        M,I,A,B,D)  (void *)A  
#define TINTVVVV(       M,I,A,B,D)  (void *)A  
#define TINTVVVVV(      M,I,A,B,D)  (void *)A
#define TINTVVVVVV(     M,I,A,B,D)  (void *)A
#define TINTVVVVVVV(    M,I,A,B,D)  (void *)A
#define TLOGICALVV(     M,I,A,B,D)  (void *)A
#define TLOGICALVVV(    M,I,A,B,D)  (void *)A
#define TLOGICALVVVV(   M,I,A,B,D)  (void *)A
#define TLOGICALVVVVV(  M,I,A,B,D)  (void *)A
#define TLOGICALVVVVVV( M,I,A,B,D)  (void *)A
#define TLOGICALVVVVVVV(M,I,A,B,D)  (void *)A
#define TLONGVV(        M,I,A,B,D)  (void *)A
#define TLONGVVV(       M,I,A,B,D)  (void *)A
#define TLONGVVVV(      M,I,A,B,D)  (void *)A
#define TLONGVVVVV(     M,I,A,B,D)  (void *)A
#define TLONGVVVVVV(    M,I,A,B,D)  (void *)A
#define TLONGVVVVVVV(   M,I,A,B,D)  (void *)A
#define TSHORTVV(       M,I,A,B,D)  (void *)A
#define TSHORTVVV(      M,I,A,B,D)  (void *)A
#define TSHORTVVVV(     M,I,A,B,D)  (void *)A
#define TSHORTVVVVV(    M,I,A,B,D)  (void *)A
#define TSHORTVVVVVV(   M,I,A,B,D)  (void *)A
#define TSHORTVVVVVVV(  M,I,A,B,D)  (void *)A
#define TPBYTE(         M,I,A,B,D)  A
#define TPDOUBLE(       M,I,A,B,D)  A
#define TPFLOAT(        M,I,A,B,D)  VOIDP0 A
#define TPINT(          M,I,A,B,D)  A
#define TPLOGICAL(      M,I,A,B,D)  ((*A=F2CLOGICAL(*A)),A)
#define TPLONG(         M,I,A,B,D)  A
#define TPSHORT(        M,I,A,B,D)  A
#define TPVOID(         M,I,A,B,D)  A
#define TROUTINE(       M,I,A,B,D)  ROUTINE_##I  A
/* A == pointer to the characters
   D == length of the string, or of an element in an array of strings
   E == number of elements in an array of strings                             */
#define TTSTR(    A,B,D)                                                       \
                  ((B=malloc(D+1))[D]='\0', memcpy(B,A,D), kill_trailing(B,' '))
#define TTTTSTR(  A,B,D)   (!(D<4||A[0]||A[1]||A[2]||A[3]))?NULL:              \
                            memchr(A,'\0',D)               ?A   : TTSTR(A,B,D)
#define TTTTSTRV( A,B,D,E) (B##N=E,B=malloc(B##N*(D+1)), (void *)          \
  vkill_trailing(f2cstrv(A,B,D+1, B##N*(D+1)), D+1,B##N*(D+1),' '))
#ifdef vmsFortran
#define TSTRING(        M,I,A,B,D)  TTTTSTR( A->dsc$a_pointer,B,A->dsc$w_length)
#define TSTRINGV(       M,I,A,B,D)  TTTTSTRV(A->dsc$a_pointer, B,              \
                                             A->dsc$w_length , A->dsc$l_m[0])
#define TPSTRING(       M,I,A,B,D)    TTSTR( A->dsc$a_pointer,B,A->dsc$w_length)
#define TPPSTRING(      M,I,A,B,D)           A->dsc$a_pointer
#define TSTRVOID(        M,I,A,B,D)           A->dsc$a_pointer,A->dsc$w_length
#else
#ifdef CRAYFortran
#define TSTRING(        M,I,A,B,D)  TTTTSTR( _fcdtocp(A),B,_fcdlen(A))
#define TSTRINGV(       M,I,A,B,D)  TTTTSTRV(_fcdtocp(A),B,_fcdlen(A),         \
                              num_elem(_fcdtocp(A),_fcdlen(A),M##_STRV_##A))
#define TPSTRING(       M,I,A,B,D)    TTSTR( _fcdtocp(A),B,_fcdlen(A))
#define TPPSTRING(      M,I,A,B,D)           _fcdtocp(A)
#define TSTRVOID(        M,I,A,B,D)           _fcdtocp(A),_fcdlen(A)
#else
#define TSTRING(        M,I,A,B,D)  TTTTSTR( A,B,D)
#define TSTRINGV(       M,I,A,B,D)  TTTTSTRV(A,B,D,                            \
                                             num_elem(A,D,M##_STRV_##A))
#define TPSTRING(       M,I,A,B,D)    TTSTR( A,B,D)
#define TPPSTRING(      M,I,A,B,D)           A
#define TSTRVOID(        M,I,A,B,D)           A,D
#endif
#endif
#define TPNSTRING                   TSTRING
#define TPSTRINGV                   TSTRINGV
#define TCF_0(          M,I,A,B,D)

#define RCF(TN,I)        STR_##TN(3,R,A##I,B##I,C##I,0)
#define RLOGICAL( A,B,D)
#define RPLOGICAL(A,B,D) *A=C2FLOGICAL(*A);
#define RSTRING(  A,B,D) if (B) free(B);
#define RSTRINGV( A,B,D) free(B);
/* A and D as defined above for TSTRING(V) */
#define RRRRPSTR( A,B,D) if (B) memcpy(A,B,PGSMIN(strlen(B),D)),                  \
                  (D>strlen(B)?memset(A+strlen(B),' ', D-strlen(B)):0), free(B);
#define RRRRPSTRV(A,B,D) c2fstrv(B,A,D+1,(D+1)*B##N), free(B);
#ifdef vmsFortran
#define RPSTRING( A,B,D) RRRRPSTR( A->dsc$a_pointer,B,A->dsc$w_length)
#define RPSTRINGV(A,B,D) RRRRPSTRV(A->dsc$a_pointer,B,A->dsc$w_length)
#else
#ifdef CRAYFortran
#define RPSTRING( A,B,D) RRRRPSTR( _fcdtocp(A),B,_fcdlen(A))
#define RPSTRINGV(A,B,D) RRRRPSTRV(_fcdtocp(A),B,_fcdlen(A))
#else
#define RPSTRING( A,B,D) RRRRPSTR( A,B,D)
#define RPSTRINGV(A,B,D) RRRRPSTRV(A,B,D)
#endif
#endif
#define RPNSTRING(A,B,D) RPSTRING( A,B,D)
#define RPPSTRING(A,B,D)
#define RSTRVOID(  A,B,D)

#define FZBYTE(   UN,LN) INTEGER_BYTE     fcallsc(UN,LN)(
#define FZDOUBLE( UN,LN) DOUBLE_PRECISION fcallsc(UN,LN)(
#define FZINT(    UN,LN) int   fcallsc(UN,LN)(
#define FZLOGICAL(UN,LN) int   fcallsc(UN,LN)(
#define FZLONG(   UN,LN) long  fcallsc(UN,LN)(
#define FZSHORT(  UN,LN) short fcallsc(UN,LN)(
#define FZVOID(   UN,LN) void  fcallsc(UN,LN)(
#ifndef __CF__KnR
/* The void is req'd by the Apollo, to make this an ANSI function declaration.
   The Apollo promotes K&R float functions to double. */
#define FZFLOAT(  UN,LN) float fcallsc(UN,LN)(void
#ifdef vmsFortran
#define FZSTRING( UN,LN) void  fcallsc(UN,LN)(fstring *AS
#else
#ifdef CRAYFortran
#define FZSTRING( UN,LN) void  fcallsc(UN,LN)(_fcd     AS
#else
#define FZSTRING( UN,LN) void  fcallsc(UN,LN)(char    *AS, unsigned D0
#endif
#endif
#else
#ifndef sunFortran
#define FZFLOAT(  UN,LN) float fcallsc(UN,LN)(
#else
#define FZFLOAT(  UN,LN) FLOATFUNCTIONTYPE fcallsc(UN,LN)(
#endif
#if defined(vmsFortran) || defined(CRAYFortran)
#define FZSTRING( UN,LN) void  fcallsc(UN,LN)(AS
#else
#define FZSTRING( UN,LN) void  fcallsc(UN,LN)(AS, D0
#endif
#endif

#define FBYTE            FZBYTE
#define FDOUBLE          FZDOUBLE
#ifndef __CF_KnR
#define FFLOAT(  UN,LN)  float   fcallsc(UN,LN)(
#else
#define FFLOAT           FZFLOAT
#endif
#define FINT             FZINT
#define FLOGICAL         FZLOGICAL
#define FLONG            FZLONG
#define FSHORT           FZSHORT
#define FVOID            FZVOID
#define FSTRING(  UN,LN) FZSTRING(UN,LN),

#define FFINT
#define FFVOID
#ifdef vmsFortran
#define FFSTRING          fstring *AS; 
#else
#ifdef CRAYFortran
#define FFSTRING          _fcd     AS;
#else
#define FFSTRING          char    *AS; unsigned D0;
#endif
#endif

#define LLINT              A0=
#define LLSTRING           A0=
#define LLVOID             

#define KINT            
#define KVOID
/* KSTRING copies the string into the position provided by the caller. */
#ifdef vmsFortran
#define KSTRING                                                                \
 memcpy(AS->dsc$a_pointer,A0, PGSMIN(AS->dsc$w_length,(A0==NULL?0:strlen(A0))) ); \
 AS->dsc$w_length>(A0==NULL?0:strlen(A0))?                                     \
  memset(AS->dsc$a_pointer+(A0==NULL?0:strlen(A0)),' ',                        \
         AS->dsc$w_length-(A0==NULL?0:strlen(A0))):0;
#else
#ifdef CRAYFortran
#define KSTRING                                                                \
 memcpy(_fcdtocp(AS),A0, PGSMIN(_fcdlen(AS),(A0==NULL?0:strlen(A0))) );           \
 _fcdlen(AS)>(A0==NULL?0:strlen(A0))?                                          \
  memset(_fcdtocp(AS)+(A0==NULL?0:strlen(A0)),' ',                             \
         _fcdlen(AS)-(A0==NULL?0:strlen(A0))):0;
#else
#define KSTRING          memcpy(AS,A0, PGSMIN(D0,(A0==NULL?0:strlen(A0))) );      \
                 D0>(A0==NULL?0:strlen(A0))?memset(AS+(A0==NULL?0:strlen(A0)), \
                                            ' ', D0-(A0==NULL?0:strlen(A0))):0;
#endif
#endif

/* Note that K.. and I.. can't be combined since K.. has to access data before
R.., in order for functions returning strings which are also passed in as
arguments to work correctly. Note that R.. frees and hence may corrupt the
string. */
#define IBYTE          return A0;
#define IDOUBLE        return A0;
#ifndef sunFortran
#define IFLOAT         return A0;
#else
#define IFLOAT         RETURNFLOAT(A0);
#endif
#define IINT           return A0;
#define ILOGICAL       return C2FLOGICAL(A0);
#define ILONG          return A0;
#define ISHORT         return A0;
#define ISTRING        return   ;
#define IVOID          return   ;

#ifdef OLD_VAXC                                  /* Allow %CC-I-PARAMNOTUSED. */
#pragma standard
#endif

#define FCALLSCSUB0( CN,UN,LN)             FCALLSCFUN0(VOID,CN,UN,LN)
#define FCALLSCSUB1( CN,UN,LN,T1)          FCALLSCFUN1(VOID,CN,UN,LN,T1)
#define FCALLSCSUB2( CN,UN,LN,T1,T2)       FCALLSCFUN2(VOID,CN,UN,LN,T1,T2)
#define FCALLSCSUB3( CN,UN,LN,T1,T2,T3)    FCALLSCFUN3(VOID,CN,UN,LN,T1,T2,T3)
#define FCALLSCSUB4( CN,UN,LN,T1,T2,T3,T4) FCALLSCFUN4(VOID,CN,UN,LN,T1,T2,T3,T4)
#define FCALLSCSUB5( CN,UN,LN,T1,T2,T3,T4,T5)                \
    FCALLSCFUN5(VOID,CN,UN,LN,T1,T2,T3,T4,T5)
#define FCALLSCSUB6( CN,UN,LN,T1,T2,T3,T4,T5,T6)             \
    FCALLSCFUN6(VOID,CN,UN,LN,T1,T2,T3,T4,T5,T6)       
#define FCALLSCSUB7( CN,UN,LN,T1,T2,T3,T4,T5,T6,T7)          \
    FCALLSCFUN7(VOID,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7)
#define FCALLSCSUB8( CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8)       \
    FCALLSCFUN8(VOID,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8)
#define FCALLSCSUB9( CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9)    \
    FCALLSCFUN9(VOID,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9)
#define FCALLSCSUB10(CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA) \
   FCALLSCFUN10(VOID,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA)

#define FCALLSCFUN1( T0,CN,UN,LN,T1)            \
        FCALLSCFUN5 (T0,CN,UN,LN,T1,CF_0,CF_0,CF_0,CF_0)
#define FCALLSCFUN2( T0,CN,UN,LN,T1,T2)         \
        FCALLSCFUN5 (T0,CN,UN,LN,T1,T2,CF_0,CF_0,CF_0)
#define FCALLSCFUN3( T0,CN,UN,LN,T1,T2,T3)      \
        FCALLSCFUN5 (T0,CN,UN,LN,T1,T2,T3,CF_0,CF_0)
#define FCALLSCFUN4( T0,CN,UN,LN,T1,T2,T3,T4)   \
        FCALLSCFUN5 (T0,CN,UN,LN,T1,T2,T3,T4,CF_0)
#define FCALLSCFUN5( T0,CN,UN,LN,T1,T2,T3,T4,T5)\
        FCALLSCFUN10(T0,CN,UN,LN,T1,T2,T3,T4,T5,CF_0,CF_0,CF_0,CF_0,CF_0)
#define FCALLSCFUN6( T0,CN,UN,LN,T1,T2,T3,T4,T5,T6)          \
        FCALLSCFUN10(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,CF_0,CF_0,CF_0,CF_0)
#define FCALLSCFUN7( T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7)       \
        FCALLSCFUN10(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,CF_0,CF_0,CF_0)
#define FCALLSCFUN8( T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8)    \
        FCALLSCFUN10(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,CF_0,CF_0)
#define FCALLSCFUN9( T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9) \
        FCALLSCFUN10(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,CF_0)
#define FCALLSCFUN11( T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB) \
        FCALLSCFUN13(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,CF_0,CF_0)
#define FCALLSCFUN12( T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC) \
        FCALLSCFUN13(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,CF_0)

#define FCALLSCFUN10( T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA) \
        FCALLSCFUN13(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,CF_0,CF_0,CF_0)

#ifndef __CF__KnR
#define FCALLSCFUN0(T0,CN,UN,LN)                                               \
FZ##T0(UN,LN)) {_INT(2,UU,T0,A0,0); _INT(0,LL,T0,0,0) CN(); _INT(0,K,T0,0,0) I##T0}

#define FCALLSCFUN13(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD)  \
F##T0(UN,LN) NCF(T1,1,0) NCF(T2,2,1) NCF(T3,3,1) NCF(T4,4,1) NCF(T5,5,1)     \
               NCF(T6,6,1) NCF(T7,7,1) NCF(T8,8,1) NCF(T9,9,1) NCF(TA,A,1)    \
           NCF(TB,B,1) NCF(TC,C,1) NCF(TD,D,1)     \
                        DCF(T1,1) DCF(T2,2) DCF(T3,3) DCF(T4,4) DCF(T5,5)      \
                        DCF(T6,6) DCF(T7,7) DCF(T8,8) DCF(T9,9) DCF(TA,A)      \
          DCF(TB,B) DCF(TC,C) DCF(TD,D) )    \
 {QCF(T1,1) QCF(T2,2) QCF(T3,3) QCF(T4,4) QCF(T5,5)                            \
  QCF(T6,6) QCF(T7,7) QCF(T8,8) QCF(T9,9) QCF(TA,A) \
       QCF(TB,B) QCF(TC,C) QCF(TD,D) _INT(2,UU,T0,A0,0);        \
 _INT(0,LL,T0,0,0) CN(TCF(LN,T1,1,0) TCF(LN,T2,2,1) TCF(LN,T3,3,1) TCF(LN,T4,4,1) \
      TCF(LN,T5,5,1) TCF(LN,T6,6,1) TCF(LN,T7,7,1) TCF(LN,T8,8,1) TCF(LN,T9,9,1) \
       TCF(LN,TA,A,1) TCF(LN,TB,B,1)  TCF(LN,TC,C,1) TCF(LN,TD,D,1));   \
   _INT(0,K,T0,0,0) RCF(T1,1) RCF(T2,2) RCF(T3,3) RCF(T4,4)  \
          RCF(T5,5) RCF(T6,6) RCF(T7,7) RCF(T8,8) RCF(T9,9)  RCF(TA,A) \
   RCF(TB,B) RCF(TC,C) RCF(TD,D) I##T0}

#else
#define FCALLSCFUN0(T0,CN,UN,LN) FZ##T0(UN,LN)) _INT(0,FF,T0,0,0)            \
{_INT(2,UU,T0,A0,0); _INT(0,LL,T0,0,0) CN(); _INT(0,K,T0,0,0) I##T0}

#define FCALLSCFUN13(T0,CN,UN,LN,T1,T2,T3,T4,T5,T6,T7,T8,T9,TA,TB,TC,TD)    \
F##T0(UN,LN) NNCF(T1,1,0) NNCF(T2,2,1) NNCF(T3,3,1) NNCF(T4,4,1) NNCF(T5,5,1)\
               NNCF(T6,6,1) NNCF(T7,7,1) NNCF(T8,8,1) NNCF(T9,9,1) \
    NNCF(TA,A,1) NNCF(TB,B,1) NNCF(TC,C,1) NNCF(TD,D,1)\
   DDCF(T1,1) DDCF(T2,2) DDCF(T3,3) DDCF(T4,4) DDCF(T5,5)                      \
   DDCF(T6,6) DDCF(T7,7) DDCF(T8,8) DDCF(T9,9)  \
  DDCF(TA,A) DDCF(TB,B) DDCF(TC,C) DDCF(TD,D) )  _INT(0,FF,T0,0,0) \
 NNNCF(T1,1,0) NNNCF(T2,2,1) NNNCF(T3,3,1) NNNCF(T4,4,1) NNNCF(T5,5,1)         \
 NNNCF(T6,6,1) NNNCF(T7,7,1) NNNCF(T8,8,1) NNNCF(T9,9,1) \
 NNNCF(TA,A,1) NNNCF(TB,B,1) NNNCF(TC,C,1) NNNCF(TD,D,1)         \
 DDDCF(T1,1) DDDCF(T2,2) DDDCF(T3,3) DDDCF(T4,4) DDDCF(T5,5)                   \
 DDDCF(T6,6) DDDCF(T7,7) DDDCF(T8,8) DDDCF(T9,9) \
        DDDCF(TA,A) DDDCF(TB,B) DDDCF(TC,C) DDDCF(TD,D);         \
 {QCF(T1,1) QCF(T2,2) QCF(T3,3) QCF(T4,4) QCF(T5,5)                            \
  QCF(T6,6) QCF(T7,7) QCF(T8,8) QCF(T9,9)  \
  QCF(TA,A)  QCF(TB,B) QCF(TC,C) QCF(TD,D) _INT(2,UU,T0,A0,0);        \
 _INT(0,LL,T0,0,0) CN( TCF(LN,T1,1,0) TCF(LN,T2,2,1) TCF(LN,T3,3,1)             \
                      TCF(LN,T4,4,1) TCF(LN,T5,5,1) TCF(LN,T6,6,1)             \
       TCF(LN,T7,7,1) TCF(LN,T8,8,1) TCF(LN,T9,9,1) \
  TCF(LN,TA,A,1) TCF(LN,TB,B,1) TCF(LN,TC,C,1) TCF(LN,TD,D,1));           \
 _INT(0,K,T0,0,0) RCF(T1,1) RCF(T2,2) RCF(T3,3) RCF(T4,4) RCF(T5,5)            \
                  RCF(T6,6) RCF(T7,7) RCF(T8,8) RCF(T9,9)  \
   RCF(TA,A) RCF(TB,B)  RCF(TC,C) RCF(TD,D) I##T0}

#endif

#endif   /* VAX VMS or Ultrix, Mips, CRAY, Sun, Apollo, HP9000, LynxOS, IBMR2.
            f2c, NAG f90. */


#ifdef __cplusplus
}
#endif





