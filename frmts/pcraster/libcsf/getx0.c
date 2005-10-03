/*
 * getx0.c
$Log$
Revision 1.2  2005/10/03 07:22:12  kdejong
Lots of small edits for x86-64 support, removed rcs id string.

Revision 1.1.1.1  2000/01/04 21:04:40  cees
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

/* x value, upper left co-ordinate of map
 * returns the x value, upper left co-ordinate of map
 */
REAL8 RgetXUL(
	const MAP *map) /* map handle */
{
	CHECKHANDLE(map);
	return(map->raster.xUL);
}
