#include "csf.h"
#include "csfimpl.h"

/* put new value scale
 * RputValueScale changes the value scale
 * of the map.
 *
 * returns
 *  new value scale or VS_UNDEFINED in case of an error.
 * 
 * NOTE
 * Note that there is no check if the cell representation
 * is complaint.
 *
 * Merrno
 * NOACCESS
 */
CSF_VS RputValueScale(
	MAP *map,         /* map handle */
	CSF_VS valueScale) /* new value scale */
{
	CHECKHANDLE_GOTO(map, error);
	if(! WRITE_ENABLE(map))
	{
		 M_ERROR(NOACCESS);
		 goto error;
	}
	map->raster.valueScale = valueScale;
	return(valueScale);
error:	return(VS_UNDEFINED);
}
