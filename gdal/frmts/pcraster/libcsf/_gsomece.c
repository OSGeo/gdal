#include "csf.h"
#include "csfimpl.h"

/* read a stream of cells
 * RgetSomeCells views a raster as one linear stream of
 * cells, with row i+1 placed after row i. 
 * In this stream any sequence can be read by specifying an
 * offset and the number of cells to be read
 * returns the number of cells read, just as fread
 *
 * example
 * .so examples/somecell.tr
 */
size_t RgetSomeCells(
	MAP *map,	/* map handle */
	size_t offset,   /* offset from pixel (row,col) = (0,0) */
	size_t nrCells,  /* number of cells to be read */
	void *buf)/* write-only. Buffer large enough to
                   * hold nrCells cells in the in-file cell representation
                   * or the in-app cell representation.
                   */
{

	CSF_FADDR  readAt;
	size_t cellsRead;
	UINT2  inFileCR = RgetCellRepr(map);

	offset <<= LOG_CELLSIZE(inFileCR);
	readAt = ADDR_DATA + (CSF_FADDR)offset;
	if( fseek(map->fp, (long)readAt, SEEK_SET) != 0 )
            return 0;
	cellsRead = map->read(buf, (size_t)CELLSIZE(inFileCR),
	(size_t)nrCells, map->fp);

	PRECOND(map->file2app != NULL);
	map->file2app(nrCells, buf);

	return(cellsRead);
}
