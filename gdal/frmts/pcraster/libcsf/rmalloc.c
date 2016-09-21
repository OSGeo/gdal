#include "csf.h"
#include "csfimpl.h"

/* allocate dynamic memory large enough to hold in-file and app cells
 * Rmalloc allocates memory to hold  nrOfCells 
 * cells in both the in-file and app cell representation. Allocation
 * is done by malloc for other users. Our own (utrecht university) applications
 * calls ChkMalloc. Freeing memory allocated by Rmalloc is done by free (or Free).
 *
 * NOTE
 * Note that a possible RuseAs call must be done BEFORE Rmalloc.
 *
 * returns
 * a pointer the allocated memory or
 * NULL
 * if the request fails
 *
 * example
 * .so examples/_row.tr
 */
void *Rmalloc(
	const MAP *m,      /* map handle */
	size_t nrOfCells)   /* number of cells allocated memory must hold */
{
	CSF_CR inFileCR = RgetCellRepr(m);
	CSF_CR largestCellRepr = 
		LOG_CELLSIZE(m->appCR) > LOG_CELLSIZE(inFileCR) 
		 ?  m->appCR : inFileCR;

	return CSF_MALLOC((size_t)CSFSIZEOF(nrOfCells, largestCellRepr));
}
