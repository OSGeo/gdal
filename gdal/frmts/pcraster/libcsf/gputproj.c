/*
 * gputproj.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:04:44  cees
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

/* change projection type of map
 * MputProjection type changes the projection type of a map.
 * In version 2, projections are simplified. We only discern between
 * a projection with y increasing (PT_YINCT2B=0) and decreasing (PT_YDECT2B=1) 
 * from top to bottom.
 * All old constants that denote a projection with y decreasing are nonzero.
 * And the old constant that denote a projection with y decreasing (PT_XY) is 0.
 * returns the new projection (PT_YINCT2B or PT_YDECT2B) or MV_UINT2 if an 
 * error occurred.
 *
 * Merrno
 * NOACCESS
 */
CSF_PT MputProjection(
	MAP *map,      /* map handle */
	CSF_PT p)       /* projection type, all nonzero values are mapped to
	                * 1 (PT_YDECT2B) 
	                */
{
	CHECKHANDLE_GOTO(map, error);
	if(! WRITE_ENABLE(map))
	{
		M_ERROR(NOACCESS);
		goto error;
	}
	map->main.projection =  (p) ? PT_YDECT2B : PT_YINCT2B;
	return map->main.projection; 
error:	return(MV_UINT2);
}
