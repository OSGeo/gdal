#include "csf.h"
#include "csfimpl.h"

/* number of columns in a map 
 * RgetNrCols returns the number of columns in a map 
 * returns the number of columns in a map 
 *
 * example
 * .so examples/csfdump1.tr
 */
size_t RgetNrCols(
	const MAP *map) /* map handle */
{
	return(map->raster.nrCols);
}
