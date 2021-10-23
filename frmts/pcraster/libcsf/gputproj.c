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
