/*
 * pmaxval.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.3  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.2  2005/09/29 18:43:23  cees
x86_64

Revision 1.1.1.1  2000/01/04 21:04:52  cees
Initial import Cees

Revision 2.1  1996/12/29 19:35:21  cees
src tree clean up

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/09/05  15:59:34  cees
 * added app2file conversion
 * added c2man docs
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
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
