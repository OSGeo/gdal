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
  size_t  nrCells,
  void   *buf)
{
  /* nothing */
  /* Shut up the C compiler */
  (void)nrCells;
  (void)buf;
}
