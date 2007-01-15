/*
 * putx0.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:04:57  cees
Initial import Cees

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/09/08  17:16:23  cees
 * added c2man docs + small code changes
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csfimpl.h"


/* change the x value of upper left co-ordinate
 * RputXUL changes the x value of upper left co-ordinate.
 * returns the new x value of upper left co-ordinate or 0
 * case of an error.
 *
 * Merrno
 * NOACCESS
 */
REAL8 RputXUL(
	MAP *map, /* map handle */
	REAL8 xUL) /* new x value of top left co-ordinate */
{
	CHECKHANDLE_GOTO(map, error);
	if(! WRITE_ENABLE(map))
	{
		M_ERROR(NOACCESS);
		goto error;
	}
	map->raster.xUL = xUL;
	return(xUL);
error:  return(0);
}
