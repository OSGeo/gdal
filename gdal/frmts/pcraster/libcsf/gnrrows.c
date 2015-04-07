#include "csf.h"
#include "csfimpl.h"

/* number of rows in a map 
 * RgetNrCols returns the number of rows in a map 
 * returns the number of rows in a map 
 *
 * example
 * .so examples/csfdump1.tr
 */
size_t RgetNrRows(
	const MAP *map) /* map handle */
{
	return(map->raster.nrRows);
}
