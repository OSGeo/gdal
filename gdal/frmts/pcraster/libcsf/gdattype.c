/*
 * gdattype.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:04:37  cees
Initial import Cees

Revision 2.1  1996/12/29 19:35:21  cees
src tree clean up

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/09/02  16:22:20  cees
 * added c2man doc
 * const'ified map handle
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csfimpl.h"

/* data type of the map
 * MgetDataType returns the type of the map, which is always
 * a raster until now.
 * returns
 * T_RASTER
 */
UINT4 MgetMapDataType(
	const MAP *map) /* map handle */
{
	return (UINT4)(map->main.mapType);
}
