/* make storage for one and two dimensional matricies */
#include <stdlib.h>
#include <projects.h>
	void * /* one dimension array */
vector1(int nvals, int size) { return((void *)pj_malloc(size * nvals)); }
	void /* free 2D array */
freev2(void **v, int nrows) {
	if (v) {
		for (v += nrows; nrows > 0; --nrows)
			pj_dalloc(*--v);
		pj_dalloc(v);
	}
}
	void ** /* two dimension array */
vector2(int nrows, int ncols, int size) {
	void **s;

	if ((s = (void **)pj_malloc(sizeof(void *) * nrows)) != NULL) {
		int rsize, i;

		rsize = size * ncols;
		for (i = 0; i < nrows; ++i)
			if (!(s[i] = pj_malloc(rsize))) {
				freev2(s, i);
				return (void **)0;
			}
	}
	return s;
}
