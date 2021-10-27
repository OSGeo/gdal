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
