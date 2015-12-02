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
 * Copy projection field into the raster, so raster can act as an 
 * independent structure for transformations.
 */
void CsfFinishMapInit(
	MAP *m)		/* map handle */
{
	m->raster.angleCos   = cos(-(m->raster.angle));
	m->raster.angleSin   = sin(-(m->raster.angle));
	m->raster.projection = MgetProjection(m);
}
