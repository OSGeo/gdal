#include "csf.h"
#include "csfimpl.h"

/* set new maximum cell value
 * RputMaxVal set a new value stored in
 * the header as the maximum value. 
 * minMaxStatus is set to MM_DONTKEEPTRACK
 *
 * NOTE
 * Note that the header maximum set must be equal or 
 * larger than the maximum value in the map.
 *
 * example
 * .so examples/set_min.tr
 */
void RputMaxVal(
	MAP *map, /* map handle */
	const void *maxVal)   /* New maximum value */
{
	/* use buffer that can hold largest 
	 * cell representation
	 */
	CSF_VAR_TYPE buf_1;
	void *buf = (void *)(&buf_1);

	CHECKHANDLE(map);

	/* make a copy */
	CsfGetVarType(buf, maxVal, map->appCR);

	/* convert */
	map->app2file((size_t)1, buf);

	/* set */
	CsfGetVarType( (void *)&(map->raster.maxVal), buf, RgetCellRepr(map));

	map->minMaxStatus = MM_DONTKEEPTRACK;
}
