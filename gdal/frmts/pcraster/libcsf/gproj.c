#include "csf.h"
#include "csfimpl.h"

/* get projection type
 * MgetProjection returns the projection type of the map.
 * In version 2, projections are simplified. We only discern between
 * a projection with y increasing (PT_YINCT2B) and decreasing (PT_YDECT2B) 
 * from top to bottom.
 * The old constants are mapped to these two.
 * returns PT_YINCT2B or PT_YDECT2B
 */
CSF_PT MgetProjection(
	const MAP *map) /* map handle */
{
	return (map->main.projection) ? PT_YDECT2B : PT_YINCT2B;
}
