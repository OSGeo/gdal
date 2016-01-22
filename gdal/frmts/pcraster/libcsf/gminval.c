#include "csf.h"
#include "csfimpl.h"

/* get minimum cell value
 * RgetMinVal returns the value stored in
 * the header as the minimum value. 
 * If the minMaxStatus is MM_WRONGVALUE
 * then a missing value is returned.
 * returns 0 if argument minVal is returned with a missing
 * value, nonzero if not.
 *
 * example
 * .so examples/csfstat.tr
 */

int RgetMinVal(
	const MAP *map, /* map handle */
	void *minVal)   /* write-only. Minimum value or missing value */
{
	/* use buffer that can hold largest 
	 * cell representation
	 */
	CSF_VAR_TYPE buf_1;
	void *buf = (void *)(&buf_1);

	CHECKHANDLE(map);
	CsfGetVarType(buf, &(map->raster.minVal), RgetCellRepr(map));

	map->file2app((size_t)1, buf);

	if (map->minMaxStatus == MM_WRONGVALUE)
		SetMV(map, buf);

	CsfGetVarType(minVal, buf, map->appCR);

	return((!IsMV(map,minVal)) &&
 		map->minMaxStatus!=MM_WRONGVALUE);
}
