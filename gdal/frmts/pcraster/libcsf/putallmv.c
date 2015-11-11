#include <stdlib.h>

#include "csf.h"
#include "csfimpl.h"

/* make all cells missing value in map
 * RputAllMV writes a missing values to all the cells in a
 * map. For this is allocates a buffer to hold one row at a
 * time.
 * returns 1 if successfully, 0 in case of an error
 */
int RputAllMV(
	MAP *m)
{
	size_t i,nc,nr;
	void *buffer;
	CSF_CR cr;

	CHECKHANDLE_GOTO(m, error);
	if(! WRITE_ENABLE(m))
	{
		M_ERROR(NOACCESS);
		goto error;
	}

	cr = RgetCellRepr(m);
	nc = RgetNrCols(m);

	buffer = Rmalloc(m,nc);
	if(buffer == NULL)
	{
		M_ERROR(NOCORE);
		goto error;
	}

	/*  Fill buffer with determined Missingvalue*/
	SetMemMV(buffer, nc, cr);

	nr = RgetNrRows(m);
	for(i = 0 ; i < nr; i++)
	 if (RputRow(m, i, buffer) != nc)
	 {
	    M_ERROR(WRITE_ERROR);
	    goto error_f;
	 }
	CSF_FREE(buffer);

	CsfSetVarTypeMV( &(m->raster.minVal), cr);
	CsfSetVarTypeMV( &(m->raster.maxVal), cr);

	return(1);
error_f:  
	CSF_FREE(buffer);
error:
	return(0);
}
