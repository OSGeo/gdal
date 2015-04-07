#include "csf.h"
#include "csfimpl.h"

/* put the gis file id
 * returns
 * the "gis file id" field or MV_UINT4
 * in case of an error
 *
 * Merrno
 * NOACCESS
 */
UINT4 MputGisFileId(
	MAP *map,          /* map handle */
	UINT4 gisFileId)   /*  new gis file id */
{

	CHECKHANDLE_GOTO(map, error);
	if(! WRITE_ENABLE(map))
	{
		M_ERROR(NOACCESS);
		goto error;
	}
	map->main.gisFileId = gisFileId;
	return(gisFileId);
error:	return(MV_UINT4);
}
