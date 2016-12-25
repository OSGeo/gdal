/* allocate and deallocate memory */
/* These routines are used so that applications can readily replace
** projection system memory allocation/deallocation call with custom
** application procedures.  */
#include <projects.h>
#include <errno.h>

	void *
pj_malloc(size_t size) {
/*
/ Currently, pj_malloc is a hack to solve an errno problem.
/ The problem is described in more details at
/ https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=86420.
/ It seems, that pj_init and similar functions incorrectly
/ (under debian/glibs-2.3.2) assume that pj_malloc resets
/ errno after success. pj_malloc tries to mimic this.
*/
        int old_errno = errno;
        void *res = malloc(size);
        if ( res && !old_errno )
                errno = 0;
        return res;
}
	void
pj_dalloc(void *ptr) {
	free(ptr);
}


/**********************************************************************/
void *pj_calloc (size_t n, size_t size) {
/***********************************************************************

pj_calloc is the pj-equivalent of calloc().

It allocates space for an array of <n> elements of size <size>.
The array is initialized to zeros.

***********************************************************************/
    void *res = pj_malloc (n*size);
    if (0==res)
        return 0;
    memset (res, 0, n*size);
    return res;
}


/**********************************************************************/
void *pj_dealloc (void *ptr) {
/***********************************************************************

pj_dealloc supports the common use case of "clean up and return a null
pointer" to signal an error in a multi level allocation:

    struct foo { int bar; int *baz; };

    struct foo *p = pj_calloc (1, sizeof (struct foo));
    if (0==p)
        return 0;

    p->baz = pj_calloc (10, sizeof(int));
    if (0==p->baz)
        return pj_dealloc (p); // clean up + signal error by 0-return

    return p;  // success

***********************************************************************/
    if (0==ptr)
        return 0;
    pj_dalloc (ptr);
    return 0;
}
