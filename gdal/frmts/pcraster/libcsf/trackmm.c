#include "csf.h"
#include "csfimpl.h"

/* global header (opt.) and trackmm's prototypes "" */


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

/* disable automatic tracking of minimum and maximum value 
 * A call to RdontTrackMinMax disables the automatic tracking
 * of the min/max value in successive cell writes.
 * If used, one must always
 * use RputMinVal and RputMaxVal to set the correct values.
 */
void RdontTrackMinMax(MAP *m) /* map handle */
{
	m->minMaxStatus = MM_DONTKEEPTRACK;
}
