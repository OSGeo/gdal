#include "csf.h"
#include "csfimpl.h"

typedef void (*DF)(void *min, void *max, size_t n, const void *buf);

/* DET: 
 *  while (min is MV) && (i != nrCells)
 *      min = max = buf[i++];
 *  while (i != nrCells)
 *   if (buf[i] is not MV)
 *      if (buf[i] < min) min = buf[i];
 *      if (buf[i] > max) max = buf[i];
 *   i++
 */
#define DET(min, max, nrCells, buf, type) \
	{\
		size_t i=0;\
		while (((*(type *)min) == MV_##type) && (i != nrCells))\
			 (*(type *)max) = (*(type *)min) =\
			 ((const type *)buf)[i++];\
		while (i != nrCells) \
		{\
			if (((const type *)buf)[i] != MV_##type)\
			{\
				if (((const type *)buf)[i] < (*(type *)min) )\
			  		(*(type *)min) = ((const type *)buf)[i];\
				if (((const type *)buf)[i] > (*(type *)max) )\
			  		(*(type *)max) = ((const type *)buf)[i];\
			}\
			i++;\
		}\
	}


/* determines new minimum and new maximum
 * DetMinMax({U}INT[124]|REAL[48]) analyzes
 * an array of cells and adjust the min and max argument
 * if necessary. If min and max are not yet set then they
 * must be MV both. The function 
 * assumes that both min and max are MV if min is MV.
 */
static void DetMinMaxINT1(
INT1 *min,    /* read-write.  adjusted minimum */
INT1 *max,   /* read-write.  adjusted maximum */
size_t nrCells, /* number of cells in buf */
const INT1 *buf) /* cell values to be examined */
{
	DET(min, max, nrCells, buf, INT1);
}

/* determines new minimum and new maximum
 * DetMinMax* (* = all cell representation) analyzes
 * an array of cells and adjust the min and max argument
 * if necessary. If min and max are not yet set then they
 * must be MV both. The function 
 * assumes that both min and max are MV if min is MV.
 */
 static void DetMinMaxINT2(
INT2 *min,   /* read-write.  adjusted minimum */
INT2 *max,  /* read-write.  adjusted maximum */
size_t nrCells,/* number of cells in buf */
const INT2 *buf) /* cell values to be examined */
{
	DET(min, max,  nrCells, buf, INT2);
}

/* determines new minimum and new maximum
 * DetMinMax* (* = all cell representation) analyzes
 * an array of cells and adjust the min and max argument
 * if necessary. If min and max are not yet set then they
 * must be MV both. The function 
 * assumes that both min and max are MV if min is MV.
 */
 static void DetMinMaxINT4(
INT4 *min,   /* read-write.  adjusted minimum */
INT4 *max,  /* read-write.  adjusted maximum */
size_t nrCells,/* number of cells in buf */
const INT4 *buf) /* cell values to be examined */
{
	DET(min, max,  nrCells, buf, INT4);
}


/* determines new minimum and new maximum
 * DetMinMax* (* = all cell representation) analyzes
 * an array of cells and adjust the min and max argument
 * if necessary. If min and max are not yet set then they
 * must be MV both. The function 
 * assumes that both min and max are MV if min is MV.
 */
 static void DetMinMaxUINT1(
UINT1 *min,   /* read-write.  adjusted minimum */
UINT1 *max,  /* read-write.  adjusted maximum */
size_t nrCells,/* number of cells in buf */
const UINT1 *buf) /* cell values to be examined */
{
	DET(min, max,  nrCells, buf, UINT1);
}

/* determines new minimum and new maximum
 * DetMinMax* (* = all cell representation) analyzes
 * an array of cells and adjust the min and max argument
 * if necessary. If min and max are not yet set then they
 * must be MV both. The function 
 * assumes that both min and max are MV if min is MV.
 */
 static void DetMinMaxUINT2(
UINT2 *min,   /* read-write.  adjusted minimum */
UINT2 *max,  /* read-write.  adjusted maximum */
size_t nrCells,/* number of cells in buf */
const UINT2 *buf) /* cell values to be examined */
{
	DET(min, max,  nrCells, buf, UINT2);
}

/* determines new minimum and new maximum
 * DetMinMax* (* = all cell representation) analyzes
 * an array of cells and adjust the min and max argument
 * if necessary. If min and max are not yet set then they
 * must be MV both. The function 
 * assumes that both min and max are MV if min is MV.
 */
 static void DetMinMaxUINT4(
UINT4 *min,   /* read-write.  adjusted minimum */
UINT4 *max,  /* read-write.  adjusted maximum */
size_t nrCells,/* number of cells in buf */
const UINT4 *buf) /* cell values to be examined */
{
	DET(min, max,  nrCells, buf, UINT4);
}


/* determines new minimum and new maximum
 * DetMinMax* (* = all cell representation) analyzes
 * an array of cells and adjust the min and max argument
 * if necessary. If min and max are not yet set then they
 * must be MV both. The function 
 * assumes that both min and max are MV if min is MV.
 */
static void DetMinMaxREAL4(
REAL4 *min,   /* read-write.  adjusted minimum */
REAL4 *max,  /* read-write.  adjusted maximum */
size_t nrCells,/* number of cells in buf */
const REAL4 *buf) /* cell values to be examined */
{
	size_t i = 0; 

	if ( IS_MV_REAL4(min))
	{
	 while ( IS_MV_REAL4(min) && (i != nrCells))
		*((UINT4 *)min) = ((const UINT4 *)buf)[i++];
	 *max = *min;
	}
	while (i != nrCells) 
	{
		if (! IS_MV_REAL4(buf+i))
		{
			if (buf[i] < *min )
			  	*min = buf[i];
			if (buf[i] > *max)
			  *max = buf[i];
		}
		i++;
	}
}

	

/* determines new minimum and new maximum
 * DetMinMax* (* = all cell representation) analyzes
 * an array of cells and adjust the min and max argument
 * if necessary. If min and max are not yet set then they
 * must be MV both. The function 
 * assumes that both min and max are MV if min is MV.
 */
static void DetMinMaxREAL8(
REAL8 *min,   /* read-write.  adjusted minimum */
REAL8 *max,  /* read-write.  adjusted maximum */
size_t nrCells,/* number of cells in buf */
const REAL8 *buf) /* cell values to be examined */
{
	size_t i = 0; 

	if ( IS_MV_REAL8(min))
	{
	 while ( IS_MV_REAL8(min) && (i != nrCells))
	 {
		((UINT4 *)min)[0] = ((const UINT4 *)buf)[2*i];
		((UINT4 *)min)[1] = ((const UINT4 *)buf)[(2*i++)+1];
	 }
	 *max = *min;
	}
	while (i != nrCells) 
	{
		if (! IS_MV_REAL8(buf+i))
		{
			if (buf[i] < *min )
			  	*min = buf[i];
			if (buf[i] > *max)
			  	*max = buf[i];
		}
		i++;
	}
}


/* write a stream of cells
 * RputSomeCells views a raster as one linear stream of
 * cells, with row i+1 placed after row i. 
 * In this stream any sequence can be written by specifying an
 * offset and the number of cells to be written
 * returns the number of cells written, just as fwrite
 *
 * example
 * .so examples/somecell.tr
 */
size_t RputSomeCells(
	MAP *map,	/* map handle */
	size_t offset,   /* offset from pixel (row,col) = (0,0) */
	size_t nrCells,  /* number of cells to be read */
	void *buf)/* read-write. Buffer large enough to
                   * hold nrCells cells in the in-file cell representation
                   * or the in-app cell representation.
                   * If these types are not equal then the buffer is
                   * converted from the in-app to the in-file 
                   * cell representation. 
                   */
{
	CSF_FADDR  writeAt;
	CSF_CR  cr = map->raster.cellRepr;

	/* convert */
	map->app2file(nrCells, buf);
	

	if (map->minMaxStatus == MM_KEEPTRACK)
	{
		const DF  detMinMaxFunc[12] = {
			 (DF)DetMinMaxUINT1, (DF)DetMinMaxUINT2, 
			 (DF)DetMinMaxUINT4, NULL /* 0x03  */  ,
			 (DF)DetMinMaxINT1 , (DF)DetMinMaxINT2 , 
			 (DF)DetMinMaxINT4 , NULL /* 0x07  */  ,
			 NULL /* 0x08 */   , NULL /* 0x09 */   , 
			 (DF)DetMinMaxREAL4, (DF)DetMinMaxREAL8 };

		void *min = &(map->raster.minVal);
		void *max = &(map->raster.maxVal);

		PRECOND(CSF_UNIQ_CR_MASK(cr) < 12);
		PRECOND(detMinMaxFunc[CSF_UNIQ_CR_MASK(cr)] != NULL);

		detMinMaxFunc[CSF_UNIQ_CR_MASK(cr)](min, max, nrCells, buf);
		
	}
	else
		map->minMaxStatus = MM_WRONGVALUE;

	writeAt  = ((CSF_FADDR)offset) << LOG_CELLSIZE(cr);
	writeAt += ADDR_DATA;
	if( csf_fseek(map->fp, writeAt, SEEK_SET) != 0 )
            return 0;
	return(map->write(buf, (size_t)CELLSIZE(cr), (size_t)nrCells, map->fp));
}
