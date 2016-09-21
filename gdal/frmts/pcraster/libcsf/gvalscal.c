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
