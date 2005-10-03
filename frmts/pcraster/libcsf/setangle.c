
/*
 * setangle.c 
   $Log$
   Revision 1.2  2005/10/03 07:22:12  kdejong
   Lots of small edits for x86-64 support, removed rcs id string.

   Revision 1.2  2000/02/05 21:25:48  cees
   added LOCATION_ATTRIBUTER struct

   Revision 1.1.1.1  2000/01/04 21:05:05  cees
   Initial import Cees

   Revision 2.0  1996/05/23 13:16:26  cees
   csf2clean

   Revision 1.1  1996/05/23 13:11:49  cees
   Initial revision

   Revision 1.2  1995/11/01 17:23:03  cees
   .

 * Revision 1.1  1994/09/07  13:23:08  cees
 * Initial revision
 *
 */
/********/
/* USES */
/********/

/* libs ext. <>, our ""  */
#include <math.h>
#include "csf.h"
#include "csfimpl.h"

/* global header (opt.) and setangle's prototypes "" */


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

/* Set the stuff in the header after header initialization (LIBRARY_INTERNAL)
 * Implements some common code for Mopen, Rcreate and family:
 *
 * set the map angle cosine and sin in header
 * these values are only used in the co-ordinate conversion
 * routines. And since they do a counter clockwise rotation we
 * take the sine and cosine of the negative angle.
 *
 * copy projection field into  raster, so raster can act as an 
 * indepent structure, for transformations
 */
void CsfFinishMapInit(
	MAP *m)		/* map handle */
{
	m->raster.angleCos   = cos(-(m->raster.angle));
	m->raster.angleSin   = sin(-(m->raster.angle));
	m->raster.projection = MgetProjection(m);
}
