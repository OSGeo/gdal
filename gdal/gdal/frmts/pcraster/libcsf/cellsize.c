#include "csf.h"
#include "csfimpl.h"

/* global header (opt.) and cellsize's prototypes "" */


/* headers of this app. modules called */ 

/***************/
/* EXTERNALS   */
/***************/

/**********************/ 
/* LOCAL DECLARATIONS */
/**********************/ 

/*********************/ 
/* LOCAL DEFINITIONS */
/*********************/ 

/******************/
/* IMPLEMENTATION */
/******************/


/* get cell size 
 * returns the cell size or -1 in case of an error
 *
 * Merrno
 *  ILL_CELLSIZE
 *  ILLHANDLE
 */
REAL8 RgetCellSize(
	const MAP *map) /* map handle */
{
	CHECKHANDLE(map);
	if ( map->raster.cellSize != map->raster.cellSizeDupl)
	{
		M_ERROR(ILL_CELLSIZE);
		return -1;
	}

	return(map->raster.cellSize);
}

/* put cell size
 * returns the new cell size or -1
 * in case of an error.
 *
 * Merrno
 * ILLHANDLE
 * NOACCESS
 * ILL_CELLSIZE
 */
REAL8 RputCellSize(
	MAP *map, /* map handle */
	REAL8 cellSize) /* new cell size */
{
	CHECKHANDLE_GOTO(map, error);
	if(! WRITE_ENABLE(map))
	{
		M_ERROR(NOACCESS);
		goto error;
	}
	if (cellSize <= 0.0)
	{
		M_ERROR(ILL_CELLSIZE);
		goto error;
	}
	map->raster.cellSize     = cellSize;
	map->raster.cellSizeDupl = cellSize;
	return(cellSize);
error:  return(-1.0);
}
