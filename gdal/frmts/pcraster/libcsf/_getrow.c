#include "csf.h"
#include "csfimpl.h"

/* read one row from a CSF raster file
 * RgetRow reads one row of cells from a
 * file. 
 * returns
 * Number of cells successfully read
 *
 * example
 * .so examples/_row.tr
 */
size_t RgetRow(
	MAP *map,        /* map handle */
	size_t rowNr,     /* row number to be read */
	void *buf)       /* write-only. buffer large enough to hold
	                  * cell values of one row in both the file
	                  * and in-app cell representation
	                  */
{
	return RgetSomeCells(map,
				map->raster.nrCols*rowNr,
				(size_t)map->raster.nrCols, 
				buf) ;
}
