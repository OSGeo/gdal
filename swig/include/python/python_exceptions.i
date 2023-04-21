/*
 * This was the cpl_exceptions.i code.  But since python is the only one
 * different (should support old method as well as new one)
 * it was moved into this file.
 */
%{
static int bUseExceptions=0;
static int bUserHasSpecifiedIfUsingExceptions = FALSE;
static thread_local int bUseExceptionsLocal = -1;
static thread_local CPLErrorHandler pfnPreviousHandler = CPLDefaultErrorHandler;

static void CPL_STDCALL
PythonBindingErrorHandler(CPLErr eclass, int code, const char *msg )
{
  /*
  ** Generally we want to suppress error reporting if we have exceptions
  ** enabled as the error message will be in the exception thrown in
  ** Python.
  */

  /* If the error class is CE_Fatal, we want to have a message issued
     because the CPL support code does an abort() before any exception
     can be generated */
  if (eclass == CE_Fatal ) {
    pfnPreviousHandler(eclass, code, msg );
  }

  /*
  ** We do not want to interfere with non-failure messages since
  ** they won't be translated into exceptions.
  */
  else if (eclass != CE_Failure ) {
    pfnPreviousHandler(eclass, code, msg );
  }
  else {
    CPLSetThreadLocalConfigOption("__last_error_message", msg);
    CPLSetThreadLocalConfigOption("__last_error_code", CPLSPrintf("%d", code));
  }
}
%}

%exception GetUseExceptions
{
%#ifdef SED_HACKS
    if( bUseExceptions ) bLocalUseExceptionsCode = FALSE;
%#endif
    result = GetUseExceptions();
}

%exception _GetExceptionsLocal
{
%#ifdef SED_HACKS
    if( bUseExceptions ) bLocalUseExceptionsCode = FALSE;
%#endif
    $action
}

%exception _SetExceptionsLocal
{
%#ifdef SED_HACKS
    if( bUseExceptions ) bLocalUseExceptionsCode = FALSE;
%#endif
    $action
}

%exception _UserHasSpecifiedIfUsingExceptions
{
%#ifdef SED_HACKS
    if( bUseExceptions ) bLocalUseExceptionsCode = FALSE;
%#endif
    $action
}

%inline %{

static
int GetUseExceptions() {
  return bUseExceptionsLocal >= 0 ? bUseExceptionsLocal : bUseExceptions;
}

static int _GetExceptionsLocal()
{
  return bUseExceptionsLocal;
}

static void _SetExceptionsLocal(int bVal)
{
  bUseExceptionsLocal = bVal;
}

static
void _UseExceptions() {
  CPLErrorReset();
  bUserHasSpecifiedIfUsingExceptions = TRUE;
  if( !bUseExceptions )
  {
    bUseExceptions = 1;
  }
}

static
void _DontUseExceptions() {
  CPLErrorReset();
  bUserHasSpecifiedIfUsingExceptions = TRUE;
  if( bUseExceptions )
  {
    bUseExceptions = 0;
  }
}

static int _UserHasSpecifiedIfUsingExceptions()
{
    return bUserHasSpecifiedIfUsingExceptions;
}

%}

%{
/* Completely unrelated: just to avoid Coverity warnings */

static int bReturnSame = 1;

void NeverCallMePlease() {
    bReturnSame = 0;
}

/* Some SWIG code generates dead code, which Coverity warns about */
template<class T> static T ReturnSame(T x)
{
    if( bReturnSame )
        return x;
    return 0;
}

static void ClearErrorState()
{
    CPLSetThreadLocalConfigOption("__last_error_message", NULL);
    CPLSetThreadLocalConfigOption("__last_error_code", NULL);
    CPLErrorReset();
}

static void StoreLastException() CPL_UNUSED;

static void StoreLastException()
{
    const char* pszLastErrorMessage =
        CPLGetThreadLocalConfigOption("__last_error_message", NULL);
    const char* pszLastErrorCode =
        CPLGetThreadLocalConfigOption("__last_error_code", NULL);
    if( pszLastErrorMessage != NULL && pszLastErrorCode != NULL )
    {
        CPLErrorSetState( CE_Failure,
            static_cast<CPLErrorNum>(atoi(pszLastErrorCode)),
            pszLastErrorMessage);
    }
}

static void pushErrorHandler()
{
    ClearErrorState();
    void* pPreviousHandlerUserData = NULL;
    CPLErrorHandler previousHandler = CPLGetErrorHandler(&pPreviousHandlerUserData);
    if(previousHandler != PythonBindingErrorHandler)
    {
        // Store the previous handler only if it is not ourselves (which might
        // happen in situations where a GDAL function will end up calling python
        // again), to avoid infinite recursion.
        pfnPreviousHandler = previousHandler;
    }
    CPLPushErrorHandlerEx(PythonBindingErrorHandler, pPreviousHandlerUserData);
}

static void popErrorHandler()
{
    CPLPopErrorHandler();
}

%}

%include exception.i

%exception {
    const int bLocalUseExceptions = GetUseExceptions();
    if ( bLocalUseExceptions ) {
        pushErrorHandler();
    }
    $action
    if ( bLocalUseExceptions ) {
        popErrorHandler();
    }
%#ifndef SED_HACKS
    if ( bLocalUseExceptions ) {
      CPLErr eclass = CPLGetLastErrorType();
      if ( eclass == CE_Failure || eclass == CE_Fatal ) {
        SWIG_exception( SWIG_RuntimeError, CPLGetLastErrorMsg() );
      }
    }
%#endif
}

%feature("except") Open {
    const int bLocalUseExceptions = GetUseExceptions();
    if ( bLocalUseExceptions ) {
        pushErrorHandler();
    }
    $action
    if ( bLocalUseExceptions ) {
        popErrorHandler();
    }
%#ifndef SED_HACKS
    if( result == NULL && bLocalUseExceptions ) {
      CPLErr eclass = CPLGetLastErrorType();
      if ( eclass == CE_Failure || eclass == CE_Fatal ) {
        SWIG_exception( SWIG_RuntimeError, CPLGetLastErrorMsg() );
      }
    }
%#endif
    if( result != NULL && bLocalUseExceptions ) {
        StoreLastException();
%#ifdef SED_HACKS
        bLocalUseExceptionsCode = FALSE;
%#endif
    }
}

%feature("except") OpenShared {
    const int bLocalUseExceptions = GetUseExceptions();
    if ( bLocalUseExceptions ) {
        pushErrorHandler();
    }
    $action
    if ( bLocalUseExceptions ) {
        popErrorHandler();
    }
%#ifndef SED_HACKS
    if( result == NULL && bLocalUseExceptions ) {
      CPLErr eclass = CPLGetLastErrorType();
      if ( eclass == CE_Failure || eclass == CE_Fatal ) {
        SWIG_exception( SWIG_RuntimeError, CPLGetLastErrorMsg() );
      }
    }
%#endif
    if( result != NULL && bLocalUseExceptions ) {
        StoreLastException();
%#ifdef SED_HACKS
        bLocalUseExceptionsCode = FALSE;
%#endif
    }
}

%feature("except") OpenEx {
    const int bLocalUseExceptions = GetUseExceptions();
    if ( bLocalUseExceptions ) {
        pushErrorHandler();
    }
    $action
    if ( bLocalUseExceptions ) {
        popErrorHandler();
    }
%#ifndef SED_HACKS
    if( result == NULL && bLocalUseExceptions ) {
      CPLErr eclass = CPLGetLastErrorType();
      if ( eclass == CE_Failure || eclass == CE_Fatal ) {
        SWIG_exception( SWIG_RuntimeError, CPLGetLastErrorMsg() );
      }
    }
%#endif
    if( result != NULL && bLocalUseExceptions ) {
        StoreLastException();
%#ifdef SED_HACKS
        bLocalUseExceptionsCode = FALSE;
%#endif
    }
}

%pythoncode %{
  class ExceptionMgr(object):
      """
      Context manager to manage Python Exception state
      for GDAL/OGR/OSR/GNM.

      Separate exception state is maintained for each
      module (gdal, ogr, etc), and this class appears independently
      in all of them. This is built in top of calls to the older
      UseExceptions()/DontUseExceptions() functions.

      Example::

          >>> print(gdal.GetUseExceptions())
          0
          >>> with gdal.ExceptionMgr():
          ...     # Exceptions are now in use
          ...     print(gdal.GetUseExceptions())
          1
          >>>
          >>> # Exception state has now been restored
          >>> print(gdal.GetUseExceptions())
          0

      """
      def __init__(self, useExceptions=True):
          """
          Save whether or not this context will be using exceptions
          """
          self.requestedUseExceptions = useExceptions

      def __enter__(self):
          """
          On context entry, save the current GDAL exception state, and
          set it to the state requested for the context

          """
          self.currentUseExceptions = _GetExceptionsLocal()
          _SetExceptionsLocal(self.requestedUseExceptions)

      def __exit__(self, exc_type, exc_val, exc_tb):
          """
          On exit, restore the GDAL/OGR/OSR/GNM exception state which was
          current on entry to the context
          """
          _SetExceptionsLocal(self.currentUseExceptions)
%}


%pythoncode %{

def UseExceptions():
    """ Enable exceptions in all GDAL related modules (osgeo.gdal, osgeo.ogr, osgeo.osr, osgeo.gnm).
        Note: prior to GDAL 3.7, this only affected the calling module"""

    try:
        from . import gdal
        gdal._UseExceptions()
    except ImportError:
        pass
    try:
        from . import ogr
        ogr._UseExceptions()
    except ImportError:
        pass
    try:
        from . import osr
        osr._UseExceptions()
    except ImportError:
        pass
    try:
        from . import gnm
        gnm._UseExceptions()
    except ImportError:
        pass

def DontUseExceptions():
    """ Disable exceptions in all GDAL related modules (osgeo.gdal, osgeo.ogr, osgeo.osr, osgeo.gnm).
        Note: prior to GDAL 3.7, this only affected the calling module"""

    try:
        from . import gdal
        gdal._DontUseExceptions()
    except ImportError:
        pass
    try:
        from . import ogr
        ogr._DontUseExceptions()
    except ImportError:
        pass
    try:
        from . import osr
        osr._DontUseExceptions()
    except ImportError:
        pass
    try:
        from . import gnm
        gnm._DontUseExceptions()
    except ImportError:
        pass

%}
