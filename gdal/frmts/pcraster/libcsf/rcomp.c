#include "csf.h"
#include "csfimpl.h"

/* compare 2 maps for their location attributes
 * Rcompare compares 2 maps for all location attributes:
 *
 * projection,
 *
 * xUL, yUL, angle,
 *
 * cell size and
 *
 * number of rows and columns
 *
 * returns 0 if one of these attributes differ or in case of an error, 1 
 * if they are all equal.
 *
 * Merrno
 * NOT_RASTER
 */
int Rcompare(
	const MAP *m1, /* map handle 1 */
	const MAP *m2) /* map handle 2 */
{
	CHECKHANDLE_GOTO(m1, error);

	/* check if mapType is T_RASTER */
	if ((m1->main.mapType != T_RASTER)
	|| (m2->main.mapType != T_RASTER))
	{
		M_ERROR(NOT_RASTER);
		goto error;
	}

	if (
	    MgetProjection(m1) == MgetProjection(m2) && 
	    m1->raster.xUL == m2->raster.xUL &&
    	    m1->raster.yUL == m2->raster.yUL &&
	    m1->raster.cellSize == m2->raster.cellSize &&
	    m1->raster.cellSizeDupl == m2->raster.cellSizeDupl &&
	    m1->raster.angle == m2->raster.angle &&
	    m1->raster.nrRows == m2->raster.nrRows &&
	    m1->raster.nrCols == m2->raster.nrCols 
	)	return(1);
error: 
		return(0);
}

int RgetLocationAttributes(
	CSF_RASTER_LOCATION_ATTRIBUTES *l, /* fill in this struct */
	const MAP *m) /* map handle to copy from */
{
	CHECKHANDLE_GOTO(m, error);
	*l = m->raster;
	return 1;
error:
	return 0;
}

int RcompareLocationAttributes(
	const CSF_RASTER_LOCATION_ATTRIBUTES *m1, /* */
	const CSF_RASTER_LOCATION_ATTRIBUTES *m2) /* */
{
	return  (
	    m1->projection == m2->projection && 
	    m1->xUL == m2->xUL && m1->yUL == m2->yUL &&
	    m1->cellSize == m2->cellSize &&
	    m1->angle == m2->angle &&
	    m1->nrRows == m2->nrRows && m1->nrCols == m2->nrCols );
}
