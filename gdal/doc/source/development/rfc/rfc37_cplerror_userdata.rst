.. _rfc-37:

=========================================================================
RFC 37: User data callbacks in CPLError
=========================================================================

:Date:  2011/10/25
:Author: Howard Butler
:Contact: hobu.inc at gmail dot com
:Status: Implemented
:Version: GDAL 1.9
:Voting: +1 Frank, Howard, Tamas, Daniel, Even


Description: This RFC proposes to implement user context data in
CPLErrorHandler callback functions. It does so without disrupting existing
callback patterns already in use, and provides completely auxiliary
functionality to CPLErrorHandler.

Rationale
------------------------------------------------------------------------------

It could be argued that users could already manage user context of error
handling functions with application-level globals that control its
interaction. While this sentiment is technically true, this approach adds a
ton of complication for library users. A scenario that has error callbacks
pass back user context data means simpler code for users wishing to have the
state of their application be returned along with errors from inside of GDAL.

The case for user data be passed in callbacks:

* It is a common idiom for signal-based APIs (of which CPLErrorHandler is one)
* It is simpler than requiring library users to manage the state of internal 
  library error handling externally in their own applications


Implementation Concerns
------------------------------------------------------------------------------

GDAL's (and OGR and OSR's) error handling callback mechanisms are in wide use
and changes to the base library that were to break either the callback
signatures *or* the behavior of existing callback operations should be
rejected. Adding support for user data in the call back is to be provided
in addition to existing functionality that already exists in the error
handling, and an approach that mimics and looks similar to the existing
operations is likely the best approach for GDAL -- if not the cleanest
approach in general.

Planned Changes
------------------------------------------------------------------------------

The first change will add a void* to CPLErrorHandlerNode:

::

    typedef struct errHandler
    {
        struct errHandler   *psNext;
        void                *pUserData;
        CPLErrorHandler     pfnHandler;
    } CPLErrorHandlerNode;

and to methods to add error handlers with user data will be provided:

::

    CPLErrorHandler CPL_DLL CPL_STDCALL CPLSetErrorHandlerEx(CPLErrorHandler, void*);
    void CPL_DLL CPL_STDCALL CPLPushErrorHandlerEx( CPLErrorHandler, void* );

``CPLSetErrorHandler`` and ``CPLPushErrorHandler`` will simply use the ``Ex``
functions and pass NULL in for the pUserData member.

Finally, similar to ``CPLGetLastErrorType`` and ``CPLGetLastErrorMsg`` methods, 
a ``CPLGetErrorHandlerUserData``

::

    void* CPL_STDCALL CPLGetErrorHandlerUserData(void);

SWIG bindings consideration
..............................................................................

The SWIG bindings will *not* be updated to provide access to user data for the 
currently active error handler for implementation of this RFC. SWIG bindings
maintainers can take advantage of this new functionality at their discretion, 
however.

Ticket History
------------------------------------------------------------------------------

`http://trac.osgeo.org/gdal/ticket/4295 <http://trac.osgeo.org/gdal/ticket/4295>`_ contains a patch that implements the proposed solution and 
provides context and discussion about this feature.  http://trac.osgeo.org/gdal/attachment/ticket/4295/4295-hobu-rfc.patch 
contains the current patch to implemented the proposed functionality.

Documentation
------------------------------------------------------------------------------

Documentation of the added functions is provided as part of the patch.

Implementation
------------------------------------------------------------------------------

All code will be implemented in trunk by Howard Butler after passage of the 
RFC.
