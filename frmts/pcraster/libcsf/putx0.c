#include "csf.h"
#include "csfimpl.h"


/* change the x value of upper left co-ordinate
 * RputXUL changes the x value of upper left co-ordinate.
 * returns the new x value of upper left co-ordinate or 0
 * case of an error.
 *
 * Merrno
 * NOACCESS
 */
REAL8 RputXUL(
	MAP *map, /* map handle */
	REAL8 xUL) /* new x value of top left co-ordinate */
{
	CHECKHANDLE_GOTO(map, error);
	if(! WRITE_ENABLE(map))
	{
		M_ERROR(NOACCESS);
		goto error;
	}
	map->raster.xUL = xUL;
	return(xUL);
error:  return(0);
}
