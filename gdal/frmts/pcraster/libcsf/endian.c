#include "csf.h"
#include "csfimpl.h"

/* test if map is in native endian mode
 * test if map is in native endian mode
 * returns nonzero if native, 0 if not native
 */
int    MnativeEndian(const MAP *map)
{
	return map->main.byteOrder == ORD_OK;
}

/* test if the Rput functions alter their cell buffers
 * test if the Rput functions alter their cell buffers
 * returns nonzero if they do not alter, 0 if they alter their buffers
 */
int    RputDoNotChangeValues(const MAP *map)
{
	return MnativeEndian(map) && map->appCR == map->raster.cellRepr;
}
