#include "csf.h"
#include "csfimpl.h"

/* get cell representation
 * RgetCellRepr returns the in-file cell representation.
 * returns the cell representation 
 */
CSF_CR RgetCellRepr(
	const MAP *map) /* map handle */
{
	return(map->raster.cellRepr);
}

/* get cell representation as set by RuseAs
 * RgetUseCellRepr returns the cell representation as set by RuseAs
 * returns the cell representation as set by RuseAs
 */

CSF_CR RgetUseCellRepr(
	const MAP *map) /* map handle */
{
   	return(map->appCR);
}
