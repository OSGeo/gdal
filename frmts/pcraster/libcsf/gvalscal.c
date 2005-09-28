/*
 * gvalscal.c
$Log$
Revision 1.1  2005/09/28 20:54:53  kdejong
Initial version of internal csf library code.

Revision 1.1.1.1  2000/01/04 21:04:44  cees
Initial import Cees

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/09/05  13:15:05  cees
 * added c2man
 * const'fied map handle
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#ifndef lint
 static const char *rcs_id = 
 "$Header$";
#endif

#include "csf.h"
#include "csfimpl.h"

/* get value scale of map
 * returns the value scale of a map which is one of the 
 * constants prefixed by "VS_".
 *
 */
CSF_VS RgetValueScale(
	const MAP *map) /* map handle */
{
	return(map->raster.valueScale);
}
