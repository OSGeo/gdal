
/*
 * dumconv.c 
   $Log$
   Revision 1.1  2005/09/28 20:54:53  kdejong
   Initial version of internal csf library code.

   Revision 1.1.1.1  2000/01/04 21:04:37  cees
   Initial import Cees

   Revision 2.0  1996/05/23 13:16:26  cees
   csf2clean

   Revision 1.1  1996/05/23 13:11:49  cees
   Initial revision

   Revision 1.2  1995/11/01 17:23:03  cees
   .

 * Revision 1.1  1994/09/09  12:17:59  cees
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
#include "csf.h"
#include "csfimpl.h"

/* global header (opt.) and dumconv's prototypes "" */

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

/* ARGSUSED */

/* dummy conversion (LIBRARY_INTERNAL)
 * does nothing
 */
void CsfDummyConversion(
	size_t nrCells,  
	void  *buf)
{
	/* nothing */
}
