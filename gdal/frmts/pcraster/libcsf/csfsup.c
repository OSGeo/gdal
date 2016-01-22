/*
 * csfsup.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:04:34  cees
Initial import Cees

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/09/06  13:21:04  cees
 * added c2man docs
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csftypes.h"
#include <string.h>    /* memset */

/* set an array of cells to missing value
 * SetMemMV sets an array of cells to missing value
 */
void SetMemMV(
	void *buf,		/* write-only buffer with cells */
	size_t nrElements,      /* number of cells */
	CSF_CR cellRepr)         /* cell representation */
{
size_t index;

	switch (cellRepr) {
	  case CR_INT1: (void)memset(buf,MV_INT1,nrElements);break;
	  case CR_INT2: for (index=0;index<nrElements;index++)
					 ((INT2 *) buf)[index]=MV_INT2;
			break;
	  case CR_INT4: for (index=0;index<nrElements;index++)
					 ((INT4 *) buf)[index]=MV_INT4;
			break;
	  default: (void)memset(buf,MV_UINT1,CSFSIZEOF(nrElements,cellRepr));
	}
}
