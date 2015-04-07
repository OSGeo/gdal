#include "csf.h"
#include "csfimpl.h"

/* change the y value of upper left co-ordinate
 * RputYUL changes the y value of upper left co-ordinate.
 * Whether this is the largest or smallest y value depends on the
 * projection (See MgetProjection()).
 * returns the new y value of upper left co-ordinate or 0
 * case of an error.
 *
 * Merrno
 * NOACCESS
 */
REAL8 RputYUL(
	MAP *map, /* map handle */
	REAL8 yUL) /* new y value of upper left co-ordinate */
{
	CHECKHANDLE_GOTO(map, error);
	if(! WRITE_ENABLE(map))
	{
		M_ERROR(NOACCESS);
		goto error;
	}
	map->raster.yUL = yUL;
	return(yUL);
error:  return(0);
}
