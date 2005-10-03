#ifndef CSF__IMPL_H
#define CSF__IMPL_H

/******************************************************************/
/******************************************************************/
/**                                                               */
/**  RUU CROSS SYSTEM MAP FORMAT                                  */
/**                                                               */
/******************************************************************/
/* number of maps that can be open at one time 
 * FOPEN_MAX should be there in Ansi-C in <stdio.h>
 * stdio.h is included in csf.h, check if csf.h is included first
 */
#ifndef INCLUDED_CSF
# error csfimpl.h included before csf.h
#endif

/******************************************************************/
/* CSFIMPL.H							  */
/******************************************************************/

/******************************************************************/
/* Starting Addresses                                             */
/******************************************************************/
/* Constants of type CSF_FADDR */

#define ADDR_MAIN_HEADER       ((CSF_FADDR)0)
#define ADDR_SECOND_HEADER    ((CSF_FADDR)64)
#define ADDR_DATA            ((CSF_FADDR)256)

/* Padding of headers
 */
#define RASTER_HEADER_FILL_SIZE ((size_t)124)
#define MAIN_HEADER_FILL_SIZE   ((size_t)14)
/* Used in mclose.c
 */
#define MAX_HEADER_FILL_SIZE    (RASTER_HEADER_FILL_SIZE)

/* values for MAIN_HEADER.byteOrder */
#define ORD_OK   0x00000001L
#define ORD_SWAB 0x01000000L

/* INTERFACE with PCRaster software
 */
#ifdef USE_IN_PCR
# include "stddefx.h" 
# include "misc.h" /* malloc, free */
# define  CSF_MALLOC ChkMalloc
# define  CSF_FREE   Free
#else
# include <stdlib.h> /* malloc, free,abs */
# include <assert.h> 
# define  CSF_MALLOC malloc
# define  CSF_FREE   free
# ifdef DEBUG
#  define  PRECOND(x)	assert(x)
#  define  POSTCOND(x)	assert(x)
# else
#  define  PRECOND(x)
#  define  POSTCOND(x)
# endif
#ifndef USE_IN_GDAL
# define  ABS(x)        abs(x)
#endif
# define  USED_UNINIT_ZERO 0
#endif

/******************************************************************/
/* Definition of the main header                                  */
/******************************************************************/

/* value for MAIN_HEADER.version */
#define CSF_VERSION_1 1
#define CSF_VERSION_2 2


#define IS_UNSIGNED(type) 	(!((type) & CSF_FLOAT_SIGN_MASK))
#define IS_SIGNED(type)   	((type) & CSF_SIGN_MASK)
#define IS_REAL(type)     	((type) & CSF_FLOAT_MASK)

/******************************************************************/
/* Compiler conditions                                            */
/******************************************************************/
/*
 sizeof(INT1)  == 1
 sizeof(INT2)  == 2
 sizeof(INT4)  == 4
 sizeof(UINT1) == 1
 sizeof(UINT2) == 2
 sizeof(UINT4) == 4
 sizeof(REAL4) == 4
 sizeof(REAL8) == 8
*/

/******************************************************************/
/* Definition of an attribute control block	                  */
/******************************************************************/

#define NR_ATTR_IN_BLOCK 	10
#define LAST_ATTR_IN_BLOCK 	(NR_ATTR_IN_BLOCK-1)


typedef struct ATTR_REC 
{
		UINT2 attrId;	/* attribute identifier */
		CSF_FADDR attrOffset;   /* file-offset of attribute */
		UINT4 attrSize;	/* size of attribute in bytes */
} ATTR_REC;

typedef struct ATTR_CNTRL_BLOCK
{
	ATTR_REC attrs[NR_ATTR_IN_BLOCK];
	CSF_FADDR    next; /* file-offset of next block */
} ATTR_CNTRL_BLOCK;

#define SIZE_OF_ATTR_CNTRL_BLOCK  \
 ((NR_ATTR_IN_BLOCK * (sizeof(UINT2) + sizeof(CSF_FADDR) + sizeof(UINT4))) \
  + sizeof(CSF_FADDR) )

/* Note that two empty holes in the attribute area are never merged */


#define ATTR_NOT_USED 0x0
	/* value of attrId field if an attribute is deleted */
	/* attrOffset and attrSize must remain valid; so a new
	 * attribute can be inserted if it's size is equal or
	 * smaller then attrSize
	 */

#define END_OF_ATTRS 0xFFFF
	/* value of attrId field if there are no more attributes */
	/* INDEED: A BUG we wanted to use the highest value (0xFFFFFFFF)
	 *  but we made a mistake. Don't change, 1023  is just as
	 *  good as (2^16)-1
	 */

/* does y decrements from 
 * top to bottom in this projection type?
 * this will also hold for the old types
 * since only PT_XY was increments from
 * top to bottom, like PT_YINCT2B
 * PT_XY and PT_YINCT2B are the only one that are
 * 0, the others all have a nonzero value
 */
#define PROJ_DEC_T2B(x)	(x != 0)

#define MM_KEEPTRACK		0
#define MM_DONTKEEPTRACK	1
#define MM_WRONGVALUE		2

#define M_ERROR(errorCode) Merrno = errorCode
#define PROG_ERROR(errorCode) Merrno = errorCode

#define S_READ		"rb"   /* Open for read only */
#define S_WRITE	 	"r+b"  /* Open for write only  I don't know an */
				/* appropriate mode "r+b" seems most app. */
#define S_READ_WRITE  	"r+b"  /* Open for reading and writing */
#define S_CREATE  	"w+b"  /* Create new file for reading and writing */

#define WRITE_ENABLE(m)	   (m->fileAccessMode & M_WRITE)
#define READ_ENABLE(m)	   (m->fileAccessMode & M_READ)
#define IS_BAD_ACCESS_MODE(mode) \
		 (mode >> 2) 	/* use only 2 bits for modes */

#define READ_AS	 0 /* note that READ_AS is also used on procedures
			that implies write access, under the condition of
			write access both type bytes are equal, and the 
			READ_AS byte is 0-alligned in the record, so this
			byte is quicker accessible */
	/* we will call READ_AS the ONLY_AS if write access is implied */
#define ONLY_AS		0
#define STORED_AS	1

/* Typed zero values to keep lint happy
 * mainly used in conversion macro's
 */
#define ZERO_UINT1	((UINT1)0)
#define ZERO_UINT2	((UINT2)0)
#define ZERO_UINT4	((UINT4)0)
#define ZERO_INT1	((INT1) 0)
#define ZERO_INT2	((INT2) 0)
#define ZERO_INT4	((INT4) 0)
#define ZERO_REAL4	((REAL4)0)
#define	ZERO_REAL8	((REAL8)0)

/* LIBRARY_INTERNAL's: */
/* OLD STUFF
void TransForm(const MAP *map, UINT4 nrCells, void *buf);
 */

void  CsfFinishMapInit(MAP *m);
void  CsfDummyConversion(size_t n, void *buf);
int   CsfIsValidMap(const MAP *m);
void  CsfUnloadMap(MAP *m);
void  CsfRegisterMap(MAP *m);
int   CsfIsBootedCsfKernel(void);
void  CsfBootCsfKernel(void);
void  CsfSetVarTypeMV( CSF_VAR_TYPE *var, CSF_CR cellRepr);
void  CsfGetVarType(void *dest, const CSF_VAR_TYPE *src, CSF_CR cellRepr);
void  CsfReadAttrBlock( MAP *m, CSF_FADDR pos, ATTR_CNTRL_BLOCK *b);
int   CsfWriteAttrBlock(MAP *m, CSF_FADDR pos, ATTR_CNTRL_BLOCK *b);
int   CsfGetAttrIndex(CSF_ATTR_ID id, const ATTR_CNTRL_BLOCK *b);
CSF_FADDR CsfGetAttrBlock(MAP *m, CSF_ATTR_ID id, ATTR_CNTRL_BLOCK *b);
CSF_FADDR CsfGetAttrPosSize(MAP *m, CSF_ATTR_ID id, size_t *size);
size_t CsfWriteSwapped(void *buf, size_t size, size_t n, FILE  *f);
size_t CsfReadSwapped(void *buf, size_t size, size_t n, FILE  *f);
size_t CsfWritePlain(void *buf, size_t size, size_t n, FILE  *f);
size_t CsfReadPlain(void *buf, size_t size, size_t n, FILE  *f);
void   CsfSwap(void *buf, size_t size, size_t n);
char *CsfStringPad(char *s, size_t reqSize);

CSF_FADDR CsfSeekAttrSpace(MAP *m, CSF_ATTR_ID id, size_t size);
CSF_ATTR_ID CsfPutAttribute( MAP *m, CSF_ATTR_ID id, size_t size, size_t nitems, void *attr);
CSF_ATTR_ID CsfGetAttribute(MAP *m, CSF_ATTR_ID id, size_t elSize, size_t *nmemb, void *attr);
size_t      CsfAttributeSize(MAP *m, CSF_ATTR_ID id);
CSF_ATTR_ID CsfUpdateAttribute(MAP *m, CSF_ATTR_ID id, size_t itemSize, size_t nitems, void *attr);

int CsfValidSize(size_t size);

#define CHECKHANDLE_GOTO(m, label)	\
			if (! CsfIsValidMap(m))	\
		   	{			\
				M_ERROR(ILLHANDLE);	\
				goto label;		\
			}
#define CHECKHANDLE_RETURN(m, value)	\
			if (! CsfIsValidMap(m))	\
		   	{			\
				M_ERROR(ILLHANDLE);	\
				return value;		\
			}
#define CHECKHANDLE(m)	\
			if (! CsfIsValidMap(m))	\
		   	{			\
				M_ERROR(ILLHANDLE);	\
			}

#endif /* CSF__IMPL_H */
