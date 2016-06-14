#include "csf.h"
#include "csfimpl.h"

/* M_PI */
#include <math.h> 

#ifndef M_PI
# define M_PI        ((double)3.14159265358979323846)
#endif

/* put new angle 
 * RputAngle changes the angle 
 * of the map.
 * returns new angle or -1 in case of an error.
 *
 * Merrno
 * NOACCESS
 * BAD_ANGLE
 */
REAL8 RputAngle(
	MAP *map,    /* map handle */
	REAL8 angle) /* new angle */
{
	CHECKHANDLE_GOTO(map, error);
	if(! WRITE_ENABLE(map))
	{
		 M_ERROR(NOACCESS);
		 goto error;
	}
	if ((0.5*-M_PI) >= angle || angle >= (0.5*M_PI))
	{
		 M_ERROR(BAD_ANGLE);
		 goto error;
	}
	map->raster.angle = angle;

	return(angle);
error:	return(-1.0);
}

/* get new angle 
 * RgetAngle returns the angle 
 * of the map.
 * returns angle of the map
 */
REAL8 RgetAngle(
	const MAP *map) /* map handle */
{
	return map->raster.angle;
}
