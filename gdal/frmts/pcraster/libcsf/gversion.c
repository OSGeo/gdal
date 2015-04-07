#include "csf.h"
#include "csfimpl.h"

/* get CSF version
 * returns CSF version
 */
UINT4 MgetVersion(
 const MAP *map) /* map handle */
{
	CHECKHANDLE(map);
	return (UINT4) (map->main.version);
}
