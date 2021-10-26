#include "csf.h"
#include "csfimpl.h"

/* gis file id
 * returns
 * the "gis file id" field
 */
UINT4 MgetGisFileId(
	const MAP *map) /* map handle */
{
	CHECKHANDLE(map);
	return(map->main.gisFileId);
}
