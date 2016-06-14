#include "csf.h"
#include "csfimpl.h"

/* get maximum cell value
 * RgetMaxVal returns the value stored in
 * the header as the maximum value. 
 * If the minMaxStatus is MM_WRONGVALUE
 * then a missing value is returned.
 * returns 0 if argument maxVal is returned with a missing
 * value, nonzero if not.
 *
 * example
 * .so examples/csfstat.tr
 */
int RgetMaxVal(
	const MAP *map, /* map handle */
	void *maxVal)   /* write-only. Maximum value or missing value */
{
	/* use buffer that can hold largest 
	 * cell representation
	 */
	CSF_VAR_TYPE buf_1;
	void *buf = (void *)(&buf_1);

	CHECKHANDLE(map);
	CsfGetVarType(buf, &(map->raster.maxVal), RgetCellRepr(map));

	map->file2app((size_t)1, buf);

	if (map->minMaxStatus == MM_WRONGVALUE)
		SetMV(map, buf);

	CsfGetVarType(maxVal, buf, map->appCR);

	return((!IsMV(map,maxVal)) &&
 		map->minMaxStatus!=MM_WRONGVALUE);
}
