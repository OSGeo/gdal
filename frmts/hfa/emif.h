/**********************************************************************
***********************************************************************

$Header$

$Log$
Revision 1.1  1999/01/04 05:28:13  warmerda
New

 * Revision 1.4  95/03/09  17:19:06  martinez
 * Added a kludge for the DEC Alpha that will allow it to read and write longs
 * into an integer sized data storage area
 * 
 * Revision 1.3  94/08/18  01:52:35  martinez
 * Couched elib globals in EMSC_GLOBAL macros
 * 
 * Revision 1.2  94/06/22  19:04:36  martinez
 * Added EMIF_DESIGN* macros to allow neater and less repetitive design
 * creation in packages
 * 
 * Revision 1.1  94/03/23  14:46:27  huang
 * Initial revision
 * 

***********************************************************************
***********************************************************************/
/**************************************************************************
** file: emif.h
**
** description: Machine Independant Format Header
**
***************************************************************************
*/

#ifndef EMIF_DEFINED
#define EMIF_DEFINED 1

#include <emsc.h>
#include <efio.h>
#include <eerr.h>


#define EMIF_MAX_NUMBER_ARRAY_ELEMENTS_TO_PRINT 512
#define EMIF_MAX_NUMBER_CHAR_ARRAY_ELEMENTS_TO_PRINT 4096
#define EMIF_MAX_NAME_LEN 32
#define EMIF_MAX_NUMBER_BASEDATA_ROWS_TO_PRINT 20
#define EMIF_MAX_NUMBER_BASEDATA_COLUMNS_TO_PRINT 20

#define EMIF_T_IGNORE_LONG EMIF_T_LONG

/*
** The following five macros are intended to be used in packages to
** define functions that return a design when called.  The idea is to
** only call the emif_DesignCreate function once and to only define
** the design in one place in the source file.  The macros might be
** used as follows:
**
** EMIF_DICTIONARY_DECLARE();
** EMIF_DESIGN_DECLARE( foo );
**
** EMIF_DESIGN_BEGIN( foo );
** 	EMIF_T_LONG,	"foodata",
** EMIF_DESIGN_END()
**
** The macro EMIF_DESIGN_FETCH( foo, errptr ) could then be called any
** number of times to fetch the emif design for foo.  The design
** will only be created once and stored statically within the function.
*/
#define	EMIF_DICTIONARY_DECLARE()					\
static Emif_Dictionary *__packageDictionary=(Emif_Dictionary*)NULL

#define	EMIF_DESIGN_DECLARE( name )					\
static Emif_Design * EMSC_CAT( name, _Design ) __(( Eerr_ErrorReport ** ))
	
#define	EMIF_DESIGN_BEGIN( name )					\
static Emif_Design *							\
EMSC_CAT( name, _Design )( inerr )					\
Eerr_ErrorReport	**inerr;					\
{									\
	EERR_INIT( #name "_Design", inerr, err );			\
static	Emif_Design	*design=(Emif_Design*)NULL;			\
									\
	if ( __packageDictionary == (Emif_Dictionary*)NULL		\
	&& ((__packageDictionary = emif_DictionaryCreate( &err )),	\
		err ) )							\
	{								\
		EERR_FATAL_SET_0( __LINE__, "Dictionary create fail" );	\
	}								\
	else if ( design == (Emif_Design*)NULL )			\
	{								\
		design = emif_DesignCreate( __packageDictionary, #name, &err,

#define	EMIF_DESIGN_END( )						\
		EMIF_T_END );						\
		if ( err )						\
		{							\
			EERR_FATAL_SET_0( __LINE__,			\
				"Design create failed" );		\
		}							\
	}								\
	return design;							\
}

#define	EMIF_DESIGN_FETCH( name, eptr )	EMSC_CAT( name, _Design )( eptr )

/* These are the primitive types used by emif */

/*****************************************************************************
xx                                                                 #ERDDOC.XX
PP  Enumerated Data Types                                          #ERDDOC.ED
xx                                                                 #ERDDOC.XX
SH                                                                 #ERDDOC.SH
**  Emif_Type:
**
**  These types of values are used by [*emif_DesignCreate*] to create 
**  _Emif_Design_ structures.  See the description of [*emif_DesignCreate*]
**  for more details.
CO                                                                 #ERDDOC.CO
*/
typedef enum {
    EMIF_T_END = 0,	/* End of Structure                     */
    EMIF_T_STRUCT,	/* Structure Type                       */
    EMIF_T_OBJECT,	/* Item is another object               */
    EMIF_T_ENUM,	/* Enumerated type                      */
    EMIF_T_U1,		/* Unsigned one bit [0..1]              */
    EMIF_T_U2,		/* Unsigned two bit [0..3]              */
    EMIF_T_U4,		/* Unsigned four bit [0..15]            */
    EMIF_T_UCHAR,	/* Unsigned char [0..255]               */
    EMIF_T_CHAR,	/* Signed char [-128..127]              */
    EMIF_T_USHORT,	/* 16 bit unsigned short integer        */
    EMIF_T_SHORT,	/* 16 bit signed integer                */
    EMIF_T_ULONG,	/* 32 bit unsigned integer              */
    EMIF_T_LONG,	/* 32 bit signed integer                */
    EMIF_T_FLOAT,	/* 32 bit single precision float        */
    EMIF_T_DOUBLE,	/* 64 bit double precision float        */
    EMIF_T_COMPLEX,	/* single precision complex {real,imag} */
    EMIF_T_DCOMPLEX,	/* double precision complex {real,imag} */
    EMIF_T_BASEDATA,	/* Egda_BaseData structure              */
    EMIF_T_TIME,	/* calendar time                        */
    EMIF_T_S32,		/* 32 bit signed integer read into int on Alpha   */
    EMIF_M_TYPE = 0x00ff,
			/* Mask for type field                  */
    EMIF_M_ARRAY = 0x0100,
			/* Mask for array flag                  */
    EMIF_M_PTR = 0x0200,
			/* Mask for pointer flag                */
    EMIF_M_INDIRECT = 0x0400,
			/*
			** Mask for indirect access (i.e., object
			** is a pointer to another object)
			*/
/***************************************************************#ERDDOC.PAUSE */
    EMIF_T_STRING = EMIF_M_PTR | EMIF_T_CHAR /* Pointer to 
        NULL-terminated character string */
/*  EMIF_T_STRING = EMIF_M_INDIRECT | EMIF_T_CHAR | EMIF_M_ARRAY *//*Pointer to 
        NULL-terminated character string (i.e., structure item is char *) */
/*                                                                 #ERDDOC.ED */
/*                                                                 #ERDDOC.CO */
} Emif_Type;

/***************************************************************#ERDDOC.PAUSE */

EMSC_GLOBAL_TYPEDEF Emif_Type EMSC_GLOBAL_TYPE(emif_Type)[];
EMSC_GLOBAL_DECLARATION(emif_Type)

#define EMIF_CADDR unsigned char
/*
typedef Epxm_U1        Emif_U1;
typedef Epxm_U2        Emif_U2;
typedef Epxm_U4        Emif_U4;
typedef Epxm_C64       Emif_Complex;
typedef Epxm_C128      Emif_DComplex; */

typedef unsigned char           Emif_U1;
typedef unsigned char           Emif_U2;
typedef unsigned char           Emif_U4;

typedef struct {
    float   real;		/* Real part of the float */
    float   imag;		/* Imaginary part of the float */
} Emif_Complex;

typedef struct {
    double real;
    double imag;
} Emif_DComplex;

typedef char           Emif_Char;
typedef unsigned char  Emif_Uchar;
typedef short          Emif_Short;
typedef unsigned short Emif_Ushort;
typedef long           Emif_Long;
typedef int            Emif_S32;
typedef unsigned long  Emif_Ulong;
typedef float          Emif_Float;
typedef double         Emif_Double;
typedef int            Emif_Enum;
    
/*****************************************************************************
xx                                                                 #ERDDOC.XX
PP Macros                                                          #ERDDOC.MS 
xx                                                                 #ERDDOC.XX
SH                                                                 #ERDDOC.SH
**  EMIF_PTR( TYPE ):
**
**  _EMIF_PTR_ is a structure comprised of a count and a data pointer.
**  It is used for storing variable-length arrays so that emif can know
**  the array length.
CO                                                                 #ERDDOC.CO
*/
#define EMIF_PTR( t ) struct { long count; t *data; }

/***************************************************************#ERDDOC.PAUSE */

typedef EMIF_PTR( char ) Emif_String;
/* typedef char Emif_String; */


/* This is the definition of one of the elements of an emif description */

typedef struct EMIF_ITEM {
  struct EMIF_ITEM   *next;	       /* Next item                         */
  struct EMIF_ITEM   *prev;	       /* Previous item                     */
  Emif_Type          type;	       /* Type of this item                 */
  char               *name;	       /* Name of this item                 */
  float              unitMIFsize;      /* Number of bytes per emif item      */
  long               MIFsize;          /* Size (total) of this item         */
  long		     unitHostSize;     /* Number of bytes per HOST item     */
  long               HostSize;         /* Total number of bytes on the host */
  long               length;           /* Number of unit items [1]          */
  short              enumCount;        /* Number of enum values             */
  char               **enumList;       /* Enum name pointers                */
  struct EMIF_DESIGN *design;          /* Sub-Design pointer                */
  Emsc_Boolean       ptrItem;          /* EMSC_TRUE if Emif_Type is EMIF_M_PTR*/
  Emsc_Boolean       indirectItem;     /* EMSC_TRUE if Emif_Type is 
                                          EMIF_M_INDIRECT                   */
} Emif_Item;


/*****************************************************************************
xx                                                                 #ERDDOC.XX
PP                                                                 #ERDDOC.DA
xx                                                                 #ERDDOC.XX
SH                                                                 #ERDDOC.SH
**  Emif_Design:
**
**  An _Emif_Design_ is a definition of the data contained in an _Emif_Object_.
**  Consider the following definition:
**
CO                                                                 #ERDDOC.CO
**
**   Emif_Design *DemoDef;
**
**   DemoDef = emif_DesignCreate( dd, "Demo",
**		EMIF_T_CHAR|EMIF_M_ARRAY,
**					"name", 32,
**		EMIF_T_LONG,		"width", 
**		EMIF_T_LONG,		"height",
**		EMIF_T_DOUBLE,		"xOrg",
**		EMIF_T_DOUBLE,		"yOrg",
**		EMIF_T_ENUM,		"color",
**					"red","green","blue", NULL,
**		EMIF_T_SHORT|EMIF_M_PTR,
**					"table",
**		EMIF_T_END );
TE                                                                 #ERDDOC.TE
**
**  DemoDef now points to a description of the Demo structure which
**  can be used to tell the emif package how to write or read the data
**  in a file.
CO                                                                 #ERDDOC.CO
*/
/***************************************************************#ERDDOC.PAUSE */
typedef struct EMIF_DESIGN {
  struct EMIF_DESIGN     *next;	        /* Pointer to next design        */
  struct EMIF_DESIGN     *prev;	        /* Previous design               */
  char                   *name;	        /* Name of the entry             */
  long                   HostNaturalBoundary;	/* Natural boundary      */
  long                   HostSize;	/* Size in memory of the entry   */
  long                   MIFsize;	/* Size in the file of the entry */
  Emif_Item              *item;	        /* Pointer to item list          */
} Emif_Design;

/*
**  Emif_Dictionary:
**
**  An Emif_Dictionary (also called a ^dictionary^) is a collection of 
**  Emif_Designs
**  which define the data contained in a structure.  Any design may be located
**  in the dictionary by name. The definitions are placed into the dictionary
**  so that they may be treated as a single unit. When an emif file is closed,
**  the data dictionary is written to the file.
*/
typedef struct EMIF_DICTIONARY {
    Emsc_Boolean modified;
    Emif_Design *first;
    Emif_Design *last; 
} Emif_Dictionary;

/*
** All objects which are used with the emif package are derivative of the
** primitive Wclass Emif_Object.
*/

/*****************************************************************************
xx                                                                 #ERDDOC.XX
PP                                                                 #ERDDOC.DA
xx                                                                 #ERDDOC.XX
SH                                                                 #ERDDOC.SH
**  Emif_Object:
**
**  An Emif_Object is essentially a C structure. The members of the structure 
**  are built from typical C data types. Consider the following structure:
**
CO                                                                 #ERDDOC.CO
**  typedef struct _Demo {
**	char	name[32];	/# Could be an array of anything. #/
**	long	width;		/# A simple integer. #/
**	long	height;
**	double	xOrg;		/# Floating point data. #/
**	double	yOrg;
**	enum {
**		RED,
**		GREEN,
**		BLUE
**	}	color;		/# Enumerated data type. #/
**	EMIF_PTR(short) table;	/#
**				** The EMIF_PTR(type) macro is
**				** used to define a pointer. It
**				** reserves room for a pointer
**				** AND a count to record amount
**				** of data pointed to.
**				#/
**  } Demo;
**
**  Demo *demo = NULL;		/# demo will point to an Object. #/
CO                                                                 #ERDDOC.CO
*/
typedef Emsc_Opaque Emif_Object;

/***************************************************************#ERDDOC.PAUSE */

	
#define emif_NewDictionary emif_DictionaryCreate
Emif_Dictionary *emif_DictionaryCreate __((Eerr_ErrorReport **));

#define emif_DefineObject emif_DesignCreate
Emif_Design     *emif_DesignCreate __((Emif_Dictionary *, char *, 
			Eerr_ErrorReport **, ...));
Emif_Design     *emif_AddDesignToDictionary __((Emif_Dictionary *, 
			Emif_Design *, Eerr_ErrorReport **));
Emif_Design     *emif_FindDesignByName __((Emif_Dictionary *, char *, 
			Eerr_ErrorReport **));
Emif_Design     *emif_DesignCopy __((Emif_Design *, Eerr_ErrorReport **));
void             emif_DesignDelete __((Emif_Design *, Eerr_ErrorReport **));
void             emif_DictionaryDelete __((Emif_Dictionary *,
			Eerr_ErrorReport **));

Emif_Object     *emif_NewObject __((Emif_Design *, Eerr_ErrorReport **));

void             emif_EncodeDictionary __((Efio_Fd, Emif_Dictionary *, 
			Eerr_ErrorReport **));
Emif_Dictionary *emif_DecodeDictionary __((Efio_Fd, Emif_Dictionary *, 
			Eerr_ErrorReport **));

void             emif_FprintfDictionary __((Efio_Fd, Emif_Dictionary *, 
			Eerr_ErrorReport **));
void             emif_FprintfDesign __((Efio_Fd, long, Emif_Design *, 
			char *, long, EMIF_CADDR **, Eerr_ErrorReport **));

void             emif_ConvertToMIF __((Efio_Fd, EMIF_CADDR *, Emif_Design *, 
			EMIF_CADDR *, Eerr_ErrorReport **));
void             emif_ConvertToHost __((Efio_Fd, EMIF_CADDR *, Emif_Design *, 
			EMIF_CADDR *, Eerr_ErrorReport **));

long             emif_ObjectSize __((EMIF_CADDR *, Emif_Design *, 
			Eerr_ErrorReport **));

EXTERN	void	emif_FreeObject		__(( EMIF_CADDR **, Emif_Design *,
						Eerr_ErrorReport ** ));
EXTERN	void	emif_ObjectItemsFree	__(( EMIF_CADDR **, Emif_Design *,
						Eerr_ErrorReport ** ));
#endif
