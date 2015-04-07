#include "csf.h"
#include "csfimpl.h"

/* set new minimum cell value
 * RputMinVal set a new value stored in
 * the header as the minimum value.
 * minMaxStatus is set to MM_DONTKEEPTRACK
 * 
 * NOTE
 * Note that the header minimum set must be equal or 
 * smaller than the minimum value in the map.
 *
 * example
 * .so examples/set_min.tr
 */

void RputMinVal(
  MAP *map, /* map handle */
  const void *minVal)   /* New minimum value */
{
  /* use buffer that can hold largest
   * cell representation
   */
  CSF_VAR_TYPE buf_1;
  void *buf = (void *)(&buf_1);

  CHECKHANDLE(map);

  /* make a copy */
  CsfGetVarType(buf, minVal, map->appCR);

  /* convert */
  map->app2file((size_t)1,buf);

  /* set */
  CsfGetVarType( (void *)&(map->raster.minVal), buf, RgetCellRepr(map));

  map->minMaxStatus = MM_DONTKEEPTRACK;
}
