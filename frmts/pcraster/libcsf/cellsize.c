
/*
 * cellsize.c 
   $Log$
   Revision 1.3  2006/02/07 10:17:15  kdejong
   Fixed endian compile problem
   some rcs issues of Kor, I guess
   Checked in by cees (cees@pcraster.nl) on account of Kor

   Revision 1.3  2005/10/03 07:23:00  kor
   Removed rcs id string

   Revision 1.2  2000/02/05 21:25:48  cees
   added LOCATION_ATTRIBUTER struct

   Revision 1.1.1.1  2000/01/04 21:04:12  cees
   Initial import Cees

   Revision 2.0  1996/05/23 13:16:26  cees
   csf2clean

   Revision 1.1  1996/05/23 13:11:49  cees
   Initial revision

   Revision 1.2  1995/11/01 17:23:03  cees
   .

 * Revision 1.1  1994/09/08  17:16:23  cees
 * Initial revision
 *
 */

/********/
/* USES */
/********/

/* libs ext. <>, our ""  */
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
