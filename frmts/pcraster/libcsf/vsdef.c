

/*
 * vsdef.c 
   $Log$
   Revision 1.1  2005/09/28 20:54:54  kdejong
   Initial version of internal csf library code.

   Revision 1.1.1.1  2000/01/04 21:05:12  cees
   Initial import Cees

   Revision 2.0  1996/05/23 13:16:26  cees
   csf2clean

   Revision 1.1  1996/05/23 13:11:49  cees
   Initial revision

   Revision 1.2  1995/11/01 17:23:03  cees
   .

 * Revision 1.1  1995/05/04  14:35:24  cees
 * Initial revision
 *
 * Revision 1.1  1994/09/02  14:30:00  cees
 * Initial revision
 *
 */
#ifndef lint  
static const char *rcs_id = 
 "$Header$";
#endif

/********/
/* USES */
/********/

/* libs ext. <>, our ""  */

/* global header (opt.) and vsis's prototypes "" */
#include "csf.h" 
#include "csfimpl.h" 


/* headers of this app. modules called */ 

/***************/
/* EXTERNALS   */
/***************/

/**********************/ 
/* LOCAL DECLARATIONS */
/**********************/ 

/*********************/ 
/* LOCAL DEFINITIONS */
/*********************/ 

/******************/
/* IMPLEMENTATION */
/******************/

/* returns default cell representation of a value scale/cellRepr
 * returns
 *  the appropriate cell representation constant (CR_something)
 *  or CR_UNDEFINED if vs is not a csf2 datatype
 *
 */
CSF_CR RdefaultCellRepr(
	CSF_VS     vs) /* value scale */
{
	switch(vs) {
	 case VS_LDD: 
	 case VS_BOOLEAN: return CR_UINT1;
	 case VS_NOMINAL:
	 case VS_ORDINAL: return CR_INT4;
	 case VS_SCALAR: 
	 case VS_DIRECTION: return CR_REAL4;
         case VS_CLASSIFIED: return CR_UINT1;
         case VS_CONTINUOUS: return CR_REAL4;
         case VS_NOTDETERMINED:
         default:
         	 return  CR_UNDEFINED;
      }
}
