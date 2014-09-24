
/*
 * trackmm.c 
   $Log$
   Revision 1.3  2006/02/07 10:17:15  kdejong
   Fixed endian compile problem
   some rcs issues of Kor, I guess
   Checked in by cees (cees@pcraster.nl) on account of Kor

   Revision 1.2  2005/10/03 07:23:00  kor
   Removed rcs id string

   Revision 1.1.1.1  2000/01/04 21:05:11  cees
   Initial import Cees

   Revision 2.1  1996/12/29 19:35:21  cees
   src tree clean up

   Revision 2.0  1996/05/23 13:16:26  cees
   csf2clean

   Revision 1.1  1996/05/23 13:11:49  cees
   Initial revision

   Revision 1.2  1995/11/01 17:23:03  cees
   .

 * Revision 1.1  1994/09/13  10:56:47  cees
 * Initial revision
 *
 */

/********/
/* USES */
/********/

/* libs ext. <>, our ""  */
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
 * of the min/max value in succesive cell writes. 
 * If used, one must always
 * use RputMinVal and RputMaxVal to set the correct values.
 */
void RdontTrackMinMax(MAP *m) /* map handle */
{
	m->minMaxStatus = MM_DONTKEEPTRACK;
}
