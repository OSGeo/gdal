/*
 * _putcell.c
 */
#include "csf.h"
#include "csfimpl.h"

/* write one cell to a CSF raster file
 * RputCell writes one cell value to a
 * file. 
 * returns
 * 1 if cell is successfully written, not 1 if not.
 *
 * example
 * .so examples/rawbin.tr
 */
size_t RputCell(
MAP *map,         /* map handle */
size_t rowNr,      /* Row number of cell */
size_t colNr,      /* Column number of cell */
void *cellValue)  /* read-write. Buffer large enough to
                   * hold one cell in the in-file cell representation
                   * or the in-app cell representation.
                   * If these types are not equal then the buffer is
                   * converted from the in-app to the in-file 
                   * cell representation. 
                   */
{
	return RputSomeCells(map, 
	                      (map->raster.nrCols * rowNr) + colNr,
	                      (size_t)1, cellValue) ;
}
