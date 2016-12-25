.. _threads:

================================================================================
Threads
================================================================================

.. contents:: Contents
   :depth: 3
   :backlinks: none


This page is about efforts to make PROJ.4 thread safe.

Key Thread Safety Issues
--------------------------------------------------------------------------------

* the global pj_errno variable is shared between threads and makes it
  essentially impossible to handle errors safely.  Being addressed with the
  introduction of the projCtx execution context.
* the datum shift using grid files uses globally shared lists of loaded grid
  information. Access to this has been made safe in 4.7.0 with the introduction
  of a proj.4 mutex used to protect access to these memory structures (see
  pj_mutex.c).

projCtx
--------------------------------------------------------------------------------

Primarily in order to avoid having pj_errno as a global variable, a "thread
context" structure has been introduced into a variation of the PROJ.4 API for
the 4.8.0 release.  The pj_init() and pj_init_plus() functions now have context
variations called pj_init_ctx() and pj_init_plus_ctx() which take a projections
context.

The projections context can be created with pj_ctx_alloc(), and there is a
global default context used when one is not provided by the application.  There
is a pj_ctx_ set of functions to create, manipulate, query, and destroy
contexts.  The contexts are also used now to handle setting debugging mode, and
to hold an error reporting function for textual error and debug messages.   The
API looks like:

::

    projPJ pj_init_ctx( projCtx, int, char ** );
    projPJ pj_init_plus_ctx( projCtx, const char * );

    projCtx pj_get_default_ctx(void);
    projCtx pj_get_ctx( projPJ );
    void pj_set_ctx( projPJ, projCtx );
    projCtx pj_ctx_alloc(void);
    void    pj_ctx_free( projCtx );
    int pj_ctx_get_errno( projCtx );
    void pj_ctx_set_errno( projCtx, int );
    void pj_ctx_set_debug( projCtx, int );
    void pj_ctx_set_logger( projCtx, void (*)(void *, int, const char *) );
    void pj_ctx_set_app_data( projCtx, void * );
    void *pj_ctx_get_app_data( projCtx );

Multithreaded applications are now expected to create a projCtx per thread
using pj_ctx_alloc().  The context's error handlers, and app data may be
modified if desired, but at the very least each context has an internal error
value accessed with pj_ctx_get_errno() as opposed to looking at pj_errno.

Note that pj_errno continues to exist, and it is set by pj_ctx_set_errno() (as
well as setting the context specific error number), but pj_errno still suffers
from the global shared problem between threads and should not be used by
multithreaded applications.

Note that pj_init_ctx(), and pj_init_plus_ctx() will assign the projCtx to the
created projPJ object.  Functions like pj_transform(), pj_fwd() and pj_inv()
will use the context of the projPJ for error reporting.

src/multistresstest.c
--------------------------------------------------------------------------------

A small multi-threaded test program has been written (src/multistresstest.c)
for testing multithreaded use of PROJ.4.  It performs a series of reprojections
to setup a table expected results, and then it does them many times in several
threads to confirm that the results are consistent.  At this time this program
is not part of the builds but it can be built on linux like:

::

    gcc -g multistresstest.c .libs/libproj.so -lpthread -o multistresstest
    ./multistresstest
