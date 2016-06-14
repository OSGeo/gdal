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
