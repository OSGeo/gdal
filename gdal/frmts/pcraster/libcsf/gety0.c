#include "csf.h"
#include "csfimpl.h"

/* y value, upper left co-ordinate of map
 * RgetYUL returns the y value of the upper left co-ordinate of map.
 * Whether this is the largest or smallest y value depends on the
 * projection (See MgetProjection()).
 * returns the y value, upper left co-ordinate of map
 */
REAL8 RgetYUL(
	const MAP *map) /* map handle */
{
	CHECKHANDLE(map);
	return(map->raster.yUL);
}
