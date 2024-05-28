/*
 * This was the cpl_exceptions.i code.  But since python is the only one
 * different (should support old method as well as new one)
 * it was moved into this file.
 */
%{
#include "cpl_string.h"
#include "cpl_conv.h"

static int bUseExceptions=0;
static int bUserHasSpecifiedIfUsingExceptions = FALSE;
static thread_local int bUseExceptionsLocal = -1;

struct PythonBindingErrorHandlerContext
{
    std::string     osInitialMsg{};
    std::string     osFailureMsg{};
    CPLErrorNum     nLastCode = CPLE_None;
};

static void CPL_STDCALL
PythonBindingErrorHandler(CPLErr eclass, CPLErrorNum err_no, const char *msg )
{
  PythonBindingErrorHandlerContext* ctxt = static_cast<
      PythonBindingErrorHandlerContext*>(CPLGetErrorHandlerUserData());

  /*
  ** Generally we want to suppress error reporting if we have exceptions
  ** enabled as the error message will be in the exception thrown in
  ** Python.
  */

  /* If the error class is CE_Fatal, we want to have a message issued
     because the CPL support code does an abort() before any exception
     can be generated */
  if (eclass == CE_Fatal ) {
    CPLCallPreviousHandler(eclass, err_no, msg );
  }

  /*
  ** We do not want to interfere with non-failure messages since
  ** they won't be translated into exceptions.
  */
  else if (eclass != CE_Failure ) {
    CPLCallPreviousHandler(eclass, err_no, msg );
  }
  else {
    ctxt->nLastCode = err_no;
    if( ctxt->osFailureMsg.empty() ) {
      ctxt->osFailureMsg = msg;
      ctxt->osInitialMsg = ctxt->osFailureMsg;
    } else {
      if( ctxt->osFailureMsg.size() < 10000 ) {
        ctxt->osFailureMsg = std::string(msg) + "\nMay be caused by: " + ctxt->osFailureMsg;
        ctxt->osInitialMsg = ctxt->osFailureMsg;
      }
      else
        ctxt->osFailureMsg = std::string(msg) + "\n[...]\nMay be caused by: " + ctxt->osInitialMsg;
    }
  }
}

%}

%exception GetUseExceptions
{
%#ifdef SED_HACKS
    if( ReturnSame(TRUE) ) bLocalUseExceptionsCode = FALSE;
%#endif
    result = GetUseExceptions();
}

%exception _GetExceptionsLocal
{
%#ifdef SED_HACKS
    if( ReturnSame(TRUE) ) bLocalUseExceptionsCode = FALSE;
%#endif
    $action
}

%exception _SetExceptionsLocal
{
%#ifdef SED_HACKS
    if( ReturnSame(TRUE) ) bLocalUseExceptionsCode = FALSE;
%#endif
    $action
}

%exception _UserHasSpecifiedIfUsingExceptions
{
%#ifdef SED_HACKS
    if( ReturnSame(TRUE) ) bLocalUseExceptionsCode = FALSE;
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
    return bUserHasSpecifiedIfUsingExceptions || bUseExceptionsLocal >= 0;
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

static void pushErrorHandler()
{
    CPLErrorReset();
    PythonBindingErrorHandlerContext* ctxt = new PythonBindingErrorHandlerContext();
    CPLPushErrorHandlerEx(PythonBindingErrorHandler, ctxt);
}

static void popErrorHandler()
{
    PythonBindingErrorHandlerContext* ctxt = static_cast<
      PythonBindingErrorHandlerContext*>(CPLGetErrorHandlerUserData());
    CPLPopErrorHandler();
    if( !ctxt->osFailureMsg.empty() )
    {
      CPLErrorSetState(
          CPLGetLastErrorType() == CE_Failure ? CE_Failure: CE_Warning,
          ctxt->nLastCode, ctxt->osFailureMsg.c_str());
    }
    delete ctxt;
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
          if ExceptionMgr.__module__ == "osgeo.gdal":
              try:
                  from . import gdal_array
              except ImportError:
                  gdal_array = None
              if gdal_array:
                  gdal_array._SetExceptionsLocal(self.requestedUseExceptions)

      def __exit__(self, exc_type, exc_val, exc_tb):
          """
          On exit, restore the GDAL/OGR/OSR/GNM exception state which was
          current on entry to the context
          """
          _SetExceptionsLocal(self.currentUseExceptions)
          if ExceptionMgr.__module__ == "osgeo.gdal":
              try:
                  from . import gdal_array
              except ImportError:
                  gdal_array = None
              if gdal_array:
                  gdal_array._SetExceptionsLocal(self.currentUseExceptions)

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
        from . import gdal_array
        gdal_array._UseExceptions()
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
        from . import gdal_array
        gdal_array._DontUseExceptions()
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
